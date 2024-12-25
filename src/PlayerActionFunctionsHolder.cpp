#include "PlayerActionFunctionsHolder.h"
#include <Compatibility.h>
#include <GlobalCoopData.h>
#include <Settings.h>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	PlayerActionFunctionsHolder::PlayerActionFunctionsHolder()
	{
		// Add all functions to their respective lists.
		PopulateFuncLists();
	}

	void PlayerActionFunctionsHolder::PopulateFuncLists()
	{
		// Ignore input actions that correspond to input indices.
		const auto actionIndexOffset = !InputAction::kFirstAction;
		// Populate with nullopt first before individually assigning defined funcs.
		// No actions will have a function in every category.
		_condFuncs		=	std::vector<CondFunc>(!InputAction::kActionTotal, CondFunc(std::nullopt));
		_startFuncs		=	std::vector<StartFunc>(!InputAction::kActionTotal, StartFunc(std::nullopt));
		_progFuncs		=	std::vector<ProgressFunc>(!InputAction::kActionTotal, ProgressFunc(std::nullopt));
		_cleanupFuncs	=	std::vector<CleanupFunc>(!InputAction::kActionTotal, CleanupFunc(std::nullopt));

#pragma region SET_COND_FUNCS
		// clang-format off
		_condFuncs[!InputAction::kActivate - actionIndexOffset] = ConditionFuncs::Activate;
		_condFuncs[!InputAction::kActivateAllOfType - actionIndexOffset] = ConditionFuncs::ActivateAllOfType;
		_condFuncs[!InputAction::kActivateCancel - actionIndexOffset] = ConditionFuncs::ActivateCancel;
		_condFuncs[!InputAction::kAdjustAimPitch - actionIndexOffset] = ConditionFuncs::AdjustAimPitch;
		_condFuncs[!InputAction::kAttackLH - actionIndexOffset] = ConditionFuncs::AttackLH;
		_condFuncs[!InputAction::kAttackRH - actionIndexOffset] = ConditionFuncs::AttackRH;
		_condFuncs[!InputAction::kBash - actionIndexOffset] = ConditionFuncs::Bash;
		_condFuncs[!InputAction::kBlock - actionIndexOffset] = ConditionFuncs::Block;
		_condFuncs[!InputAction::kCastLH - actionIndexOffset] = ConditionFuncs::CastLH;
		_condFuncs[!InputAction::kCastRH - actionIndexOffset] = ConditionFuncs::CastRH;
		_condFuncs[!InputAction::kChangeDialoguePlayer - actionIndexOffset] = ConditionFuncs::ChangeDialoguePlayer;
		
		// Adjusting the camera.
		_condFuncs[!InputAction::kCamLockOn - actionIndexOffset] = 
		_condFuncs[!InputAction::kCamManualPos - actionIndexOffset] = 
		_condFuncs[!InputAction::kDisableCoopCam - actionIndexOffset] = ConditionFuncs::CamActive;
		_condFuncs[!InputAction::kRotateCam - actionIndexOffset] = 
		_condFuncs[!InputAction::kZoomCam - actionIndexOffset] = ConditionFuncs::CanAdjustCamera;

		// Debug options.
		_condFuncs[!InputAction::kDebugRagdollPlayer - actionIndexOffset] = ConditionFuncs::DebugRagdollPlayer;
		_condFuncs[!InputAction::kDebugReEquipHandForms - actionIndexOffset] = ConditionFuncs::CycleEquipment;
		_condFuncs[!InputAction::kDebugRefreshPlayerManagers - actionIndexOffset] = ConditionFuncs::DebugRefreshPlayerManagers;
		_condFuncs[!InputAction::kDebugResetPlayer - actionIndexOffset] = ConditionFuncs::DebugResetPlayer;


		// Event-triggered custom menus.
		_condFuncs[!InputAction::kCoopDebugMenu - actionIndexOffset] =
		_condFuncs[!InputAction::kCoopIdlesMenu - actionIndexOffset] = 
		_condFuncs[!InputAction::kCoopMiniGamesMenu - actionIndexOffset] = ConditionFuncs::PlayerCanOpenMenu;
		_condFuncs[!InputAction::kCoopSummoningMenu - actionIndexOffset] = ConditionFuncs::CoopSummoningMenu;

		// Cycling/equipping hotkeyed gear/magic.
		_condFuncs[!InputAction::kCycleAmmo - actionIndexOffset] =
		_condFuncs[!InputAction::kCycleSpellLH - actionIndexOffset] =
		_condFuncs[!InputAction::kCycleSpellRH - actionIndexOffset] =
		_condFuncs[!InputAction::kCycleVoiceSlotMagic - actionIndexOffset] =
		_condFuncs[!InputAction::kCycleWeaponLH - actionIndexOffset] =
		_condFuncs[!InputAction::kCycleWeaponRH - actionIndexOffset] =
		_condFuncs[!InputAction::kHotkeyEquip - actionIndexOffset] = ConditionFuncs::CycleEquipment;

		_condFuncs[!InputAction::kFaceTarget - actionIndexOffset] = ConditionFuncs::FaceTarget;
		_condFuncs[!InputAction::kDismount - actionIndexOffset] = ConditionFuncs::Dismount;
		_condFuncs[!InputAction::kDodge - actionIndexOffset] = ConditionFuncs::Dodge;
		_condFuncs[!InputAction::kGrabRotateYZ - actionIndexOffset] = ConditionFuncs::GrabRotateYZ;
		
		// Menus that allow players to access their inventory or magic and equip items.
		_condFuncs[!InputAction::kFavorites - actionIndexOffset] = ConditionFuncs::Favorites;
		_condFuncs[!InputAction::kInventory - actionIndexOffset] = 
		_condFuncs[!InputAction::kMagicMenu - actionIndexOffset] = ConditionFuncs::PlayerCanOpenItemMenu;

		// Menus that allow player to modify their perks and HMS AVs.
		_condFuncs[!InputAction::kStatsMenu - actionIndexOffset] = 
		_condFuncs[!InputAction::kTweenMenu - actionIndexOffset] = ConditionFuncs::PlayerCanOpenPerksMenu;

		// Other menus.
		_condFuncs[!InputAction::kMapMenu - actionIndexOffset] = 
		_condFuncs[!InputAction::kPause - actionIndexOffset] = 
		_condFuncs[!InputAction::kTradeWithPlayer - actionIndexOffset] = 
		_condFuncs[!InputAction::kWaitMenu - actionIndexOffset] = ConditionFuncs::PlayerCanOpenMenu;

		_condFuncs[!InputAction::kJump - actionIndexOffset] = ConditionFuncs::Jump;
		_condFuncs[!InputAction::kPowerAttackDual - actionIndexOffset] = ConditionFuncs::PowerAttackDual;
		_condFuncs[!InputAction::kPowerAttackLH - actionIndexOffset] = ConditionFuncs::PowerAttackLH;
		_condFuncs[!InputAction::kPowerAttackRH - actionIndexOffset] = ConditionFuncs::PowerAttackRH;
		_condFuncs[!InputAction::kQuickSlotCast - actionIndexOffset] = ConditionFuncs::QuickSlotCast;
		_condFuncs[!InputAction::kQuickSlotItem - actionIndexOffset] = ConditionFuncs::QuickSlotItem;
		_condFuncs[!InputAction::kResetAim - actionIndexOffset] = ConditionFuncs::ResetAim;

		// Arms rotation.
		_condFuncs[!InputAction::kRotateLeftForearm - actionIndexOffset] =
		_condFuncs[!InputAction::kRotateLeftHand - actionIndexOffset] =
		_condFuncs[!InputAction::kRotateLeftShoulder - actionIndexOffset] = 
		_condFuncs[!InputAction::kRotateRightForearm - actionIndexOffset] =
		_condFuncs[!InputAction::kRotateRightHand - actionIndexOffset] =
		_condFuncs[!InputAction::kRotateRightShoulder - actionIndexOffset] = ConditionFuncs::CanRotateArms;

		_condFuncs[!InputAction::kSheathe - actionIndexOffset] = ConditionFuncs::Sheathe;
		_condFuncs[!InputAction::kShout - actionIndexOffset] = ConditionFuncs::Shout;
		_condFuncs[!InputAction::kSneak - actionIndexOffset] = ConditionFuncs::Sneak;
		_condFuncs[!InputAction::kSpecialAction - actionIndexOffset] = ConditionFuncs::SpecialAction;
		_condFuncs[!InputAction::kSprint - actionIndexOffset] = ConditionFuncs::Sprint;
		_condFuncs[!InputAction::kTeleportToPlayer - actionIndexOffset] = ConditionFuncs::TeleportToPlayer;
		
		// Without conditions:
		// CycleSpellCategoryLH, CycleSpellCategoryRH, 
		// CycleWeaponCategoryLH, CycleWeaponCategoryRH, 
		// GrabObject, HotkeyEquip,
		// MoveCrosshair, RotateCam, ZoomCam
		// clang-format on
#pragma endregion

#pragma region SET_PROG_FUNCS
		// clang-format off
		_progFuncs[!InputAction::kActivate - actionIndexOffset] = ProgressFuncs::Activate;
		_progFuncs[!InputAction::kAttackLH - actionIndexOffset] = ProgressFuncs::AttackLH;
		_progFuncs[!InputAction::kAttackRH - actionIndexOffset] = ProgressFuncs::AttackRH;
		_progFuncs[!InputAction::kBash - actionIndexOffset] = ProgressFuncs::Bash;
		_progFuncs[!InputAction::kCastLH - actionIndexOffset] = ProgressFuncs::CastLH;
		_progFuncs[!InputAction::kCastRH - actionIndexOffset] = ProgressFuncs::CastRH;
		_progFuncs[!InputAction::kCycleAmmo - actionIndexOffset] = ProgressFuncs::CycleAmmo;
		_progFuncs[!InputAction::kCycleSpellCategoryLH - actionIndexOffset] = ProgressFuncs::CycleSpellCategoryLH;
		_progFuncs[!InputAction::kCycleSpellCategoryRH - actionIndexOffset] = ProgressFuncs::CycleSpellCategoryRH;
		_progFuncs[!InputAction::kCycleSpellLH - actionIndexOffset] = ProgressFuncs::CycleSpellLH;
		_progFuncs[!InputAction::kCycleSpellRH - actionIndexOffset] = ProgressFuncs::CycleSpellRH;
		_progFuncs[!InputAction::kCycleVoiceSlotMagic - actionIndexOffset] = ProgressFuncs::CycleVoiceSlotMagic;
		_progFuncs[!InputAction::kCycleWeaponCategoryLH - actionIndexOffset] = ProgressFuncs::CycleWeaponCategoryLH;
		_progFuncs[!InputAction::kCycleWeaponCategoryRH - actionIndexOffset] = ProgressFuncs::CycleWeaponCategoryRH;
		_progFuncs[!InputAction::kCycleWeaponLH - actionIndexOffset] = ProgressFuncs::CycleWeaponLH;
		_progFuncs[!InputAction::kCycleWeaponRH - actionIndexOffset] = ProgressFuncs::CycleWeaponRH;
		_progFuncs[!InputAction::kGrabObject - actionIndexOffset] = ProgressFuncs::GrabObject;
		_progFuncs[!InputAction::kHotkeyEquip - actionIndexOffset] = ProgressFuncs::HotkeyEquip;
		_progFuncs[!InputAction::kQuickSlotCast - actionIndexOffset] = ProgressFuncs::QuickSlotCast;
		_progFuncs[!InputAction::kShout - actionIndexOffset] = ProgressFuncs::Shout;
		_progFuncs[!InputAction::kSpecialAction - actionIndexOffset] = ProgressFuncs::SpecialAction;
		// clang-format on
#pragma endregion

#pragma region SET_START_FUNCS
		// clang-format off
		_startFuncs[!InputAction::kActivateAllOfType - actionIndexOffset] = StartFuncs::ActivateAllOfType;
		_startFuncs[!InputAction::kActivateCancel - actionIndexOffset] = StartFuncs::ActivateCancel;
		_startFuncs[!InputAction::kBlock - actionIndexOffset] = StartFuncs::Block;
		_startFuncs[!InputAction::kCamLockOn - actionIndexOffset] = StartFuncs::CamLockOn;
		_startFuncs[!InputAction::kCamManualPos - actionIndexOffset] = StartFuncs::CamManualPos;
		_startFuncs[!InputAction::kChangeDialoguePlayer - actionIndexOffset] = StartFuncs::ChangeDialoguePlayer;
		_startFuncs[!InputAction::kCoopDebugMenu - actionIndexOffset] = StartFuncs::CoopDebugMenu;
		_startFuncs[!InputAction::kCoopIdlesMenu - actionIndexOffset] = StartFuncs::CoopIdlesMenu;
		_startFuncs[!InputAction::kCoopMiniGamesMenu - actionIndexOffset] = StartFuncs::CoopMiniGamesMenu;
		_startFuncs[!InputAction::kCoopSummoningMenu - actionIndexOffset] = StartFuncs::CoopSummoningMenu;
		_startFuncs[!InputAction::kDebugRagdollPlayer - actionIndexOffset] = StartFuncs::DebugRagdollPlayer;
		_startFuncs[!InputAction::kDebugReEquipHandForms - actionIndexOffset] = StartFuncs::DebugReEquipHandForms;
		_startFuncs[!InputAction::kDebugRefreshPlayerManagers - actionIndexOffset] = StartFuncs::DebugRefreshPlayerManagers;
		_startFuncs[!InputAction::kDebugResetPlayer - actionIndexOffset] = StartFuncs::DebugResetPlayer;
		_startFuncs[!InputAction::kDisableCoopCam - actionIndexOffset] = StartFuncs::DisableCoopCam;
		_startFuncs[!InputAction::kDismount - actionIndexOffset] = StartFuncs::Dismount;
		_startFuncs[!InputAction::kDodge - actionIndexOffset] = StartFuncs::Dodge;
		_startFuncs[!InputAction::kFaceTarget - actionIndexOffset] = StartFuncs::FaceTarget;
		_startFuncs[!InputAction::kFavorites - actionIndexOffset] = StartFuncs::Favorites;
		_startFuncs[!InputAction::kInventory - actionIndexOffset] = StartFuncs::Inventory;
		_startFuncs[!InputAction::kJump - actionIndexOffset] = StartFuncs::Jump;
		_startFuncs[!InputAction::kMagicMenu - actionIndexOffset] = StartFuncs::MagicMenu;
		_startFuncs[!InputAction::kMapMenu - actionIndexOffset] = StartFuncs::MapMenu;
		_startFuncs[!InputAction::kPause - actionIndexOffset] = StartFuncs::Pause;
		_startFuncs[!InputAction::kPowerAttackDual - actionIndexOffset] = StartFuncs::PowerAttackDual;
		_startFuncs[!InputAction::kPowerAttackLH - actionIndexOffset] = StartFuncs::PowerAttackLH;
		_startFuncs[!InputAction::kPowerAttackRH - actionIndexOffset] = StartFuncs::PowerAttackRH;
		_startFuncs[!InputAction::kQuickSlotItem - actionIndexOffset] = StartFuncs::QuickSlotItem;
		_startFuncs[!InputAction::kResetAim - actionIndexOffset] = StartFuncs::ResetAim;
		_startFuncs[!InputAction::kRotateCam - actionIndexOffset] = StartFuncs::RotateCam;
		_startFuncs[!InputAction::kSheathe - actionIndexOffset] = StartFuncs::Sheathe;
		_startFuncs[!InputAction::kSneak - actionIndexOffset] = StartFuncs::Sneak;
		_startFuncs[!InputAction::kSprint - actionIndexOffset] = StartFuncs::Sprint;
		_startFuncs[!InputAction::kStatsMenu - actionIndexOffset] = StartFuncs::StatsMenu;
		_startFuncs[!InputAction::kTeleportToPlayer - actionIndexOffset] = StartFuncs::TeleportToPlayer;
		_startFuncs[!InputAction::kTradeWithPlayer - actionIndexOffset] = StartFuncs::TradeWithPlayer;
		_startFuncs[!InputAction::kTweenMenu - actionIndexOffset] = StartFuncs::TweenMenu;
		_startFuncs[!InputAction::kWaitMenu - actionIndexOffset] = StartFuncs::WaitMenu;
		_startFuncs[!InputAction::kZoomCam - actionIndexOffset] = StartFuncs::ZoomCam;
		// clang-format on
#pragma endregion

#pragma region SET_CLEANUP_FUNCS
		// clang-format off
		_cleanupFuncs[!InputAction::kActivate - actionIndexOffset] = CleanupFuncs::Activate;
		_cleanupFuncs[!InputAction::kAttackLH - actionIndexOffset] = CleanupFuncs::AttackLH;
		_cleanupFuncs[!InputAction::kAttackRH - actionIndexOffset] = CleanupFuncs::AttackRH;
		_cleanupFuncs[!InputAction::kBash - actionIndexOffset] = CleanupFuncs::Bash;
		_cleanupFuncs[!InputAction::kBlock - actionIndexOffset] = CleanupFuncs::Block;
		_cleanupFuncs[!InputAction::kCastLH - actionIndexOffset] = CleanupFuncs::CastLH;
		_cleanupFuncs[!InputAction::kCastRH - actionIndexOffset] = CleanupFuncs::CastRH;
		_cleanupFuncs[!InputAction::kCycleAmmo - actionIndexOffset] = CleanupFuncs::CycleAmmo;
		_cleanupFuncs[!InputAction::kCycleSpellCategoryLH - actionIndexOffset] = CleanupFuncs::CycleSpellCategoryLH;
		_cleanupFuncs[!InputAction::kCycleSpellCategoryRH - actionIndexOffset] = CleanupFuncs::CycleSpellCategoryRH;
		_cleanupFuncs[!InputAction::kCycleSpellLH - actionIndexOffset] = CleanupFuncs::CycleSpellLH;
		_cleanupFuncs[!InputAction::kCycleSpellRH - actionIndexOffset] = CleanupFuncs::CycleSpellRH;
		_cleanupFuncs[!InputAction::kCycleVoiceSlotMagic - actionIndexOffset] = CleanupFuncs::CycleVoiceSlotMagic;
		_cleanupFuncs[!InputAction::kCycleWeaponCategoryLH - actionIndexOffset] = CleanupFuncs::CycleWeaponCategoryLH;
		_cleanupFuncs[!InputAction::kCycleWeaponCategoryRH - actionIndexOffset] = CleanupFuncs::CycleWeaponCategoryRH;
		_cleanupFuncs[!InputAction::kCycleWeaponLH - actionIndexOffset] = CleanupFuncs::CycleWeaponLH;
		_cleanupFuncs[!InputAction::kCycleWeaponRH - actionIndexOffset] = CleanupFuncs::CycleWeaponRH;
		_cleanupFuncs[!InputAction::kGrabObject - actionIndexOffset] = CleanupFuncs::GrabObject;
		_cleanupFuncs[!InputAction::kHotkeyEquip - actionIndexOffset] = CleanupFuncs::HotkeyEquip;
		_cleanupFuncs[!InputAction::kQuickSlotCast - actionIndexOffset] = CleanupFuncs::QuickSlotCast;
		_cleanupFuncs[!InputAction::kRotateCam - actionIndexOffset] = CleanupFuncs::RotateCam;
		_cleanupFuncs[!InputAction::kShout - actionIndexOffset] = CleanupFuncs::Shout;
		_cleanupFuncs[!InputAction::kSpecialAction - actionIndexOffset] = CleanupFuncs::SpecialAction;
		_cleanupFuncs[!InputAction::kSprint - actionIndexOffset] = CleanupFuncs::Sprint;
		_cleanupFuncs[!InputAction::kZoomCam - actionIndexOffset] = CleanupFuncs::ZoomCam;
		// clang-format on
#pragma endregion
	}

	namespace ConditionFuncs
	{
		bool Activate(const std::shared_ptr<CoopPlayer>& a_p)
		{
			if (!a_p->pam->downedPlayerTarget)
			{
				// If there is no downed player target, choose next refr to activate.
				return true;
			}
			else if (a_p->pam->downedPlayerTarget->isDowned && !a_p->pam->downedPlayerTarget->isRevived)
			{
				// Otherwise, attempt to revive the downed player if they are not fully revived and if this player has enough health to transfer.
				if (HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kActivate)) 
				{
					return true;
				}
				else
				{
					// Inform the player that they do not have enough health to revive another player.
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kReviveAlert,
						fmt::format("P{}: <font color=\"#FF0000\">Not enough health to revive another player!</font>", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
				}
			}
			
			return false;
		}

		bool ActivateAllOfType(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// First check validity and if the targeted refr is in range.
			const auto lastTargetedRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle); 
			if (lastTargetedRefrPtr && Util::IsValidRefrForTargeting(lastTargetedRefrPtr.get()) && a_p->tm->RefrIsInActivationRange(a_p->tm->activationRefrHandle))
			{
				// Next, the targeted refr must not be off limits, or the player must be sneaking to signal intent to steal.
				// Finally, the refr must be a lootable loose refr, a corpse, or an unlocked container.
				return 
				(
					(!lastTargetedRefrPtr->IsCrimeToActivate() || a_p->coopActor->IsSneaking()) &&
					(
						(Util::IsLootableRefr(lastTargetedRefrPtr.get())) || 
						(lastTargetedRefrPtr->As<RE::Actor>() && lastTargetedRefrPtr->As<RE::Actor>()->IsDead()) ||
						(!lastTargetedRefrPtr->IsLocked() && !lastTargetedRefrPtr->As<RE::Actor>() && lastTargetedRefrPtr->HasContainer())
					)
				);
			}

			return false;
		}

		bool ActivateCancel(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cannot cancel if no object is targeted.
			return (bool)(Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle));
		}

		bool AdjustAimPitch(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Don't rotate torso while rotating arms, or if some inputs are still pressed for the arm rotation binds.
			bool isTryingToRotateArms = 
			{
				(Settings::bRotateArmsWhenSheathed && !a_p->coopActor->IsWeaponDrawn()) &&
				a_p->pam->AllInputsPressedForAtLeastOneAction
				(
					InputAction::kRotateRightShoulder,
					InputAction::kRotateLeftShoulder,
					InputAction::kRotateRightForearm,
					InputAction::kRotateLeftForearm,
					InputAction::kRotateRightHand,
					InputAction::kRotateLeftHand
				)
			};
			if (isTryingToRotateArms) 
			{
				return false;
			}

			// RS moved and Y component larger than X component.
			const auto& rsState = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
			return rsState.normMag > 0.0f && fabsf(rsState.yComp) > fabsf(rsState.xComp);
		}

		bool AttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// If arms rotation is disabled, will draw weapons/magic/fists on release instead of attacking.
			bool drawn = a_p->coopActor->IsWeaponDrawn();
			if (!drawn && !a_p->isTransformed)
			{
				return true;
			}

			// P1 is AI driven while mounted, so the game handles attacks by default.
			if (a_p->isPlayer1 && a_p->coopActor->IsOnMount())
			{
				return false;
			}

			// Must have weapons/magic out and not blocking.
			bool baselineCheck = 
			{
				drawn && !a_p->pam->isBlocking
			};

			// Return early if baseline check fails.
			if (!baselineCheck)
			{
				return false;
			}

			const auto& em = a_p->em;
			// Just have to ensure the transformed player does not have a spell equipped in the LH.
			if (a_p->isTransformed && !em->HasLHSpellEquipped())
			{
				return true;
			}

			return 
			{
				// Unmounted: 
				// 1. Has LH one handed weapon equipped or is unarmed.
				// 2. Has 2H ranged weapon and has enough stamina to bash.
				// Mounted:
				// 1. Has a melee weapon equipped.
				(em->HasLHWeapEquipped() || em->IsUnarmed() || 
				(a_p->coopActor->IsOnMount() && (em->HasLHMeleeWeapEquipped() || em->HasRHMeleeWeapEquipped() || em->Has2HMeleeWeapEquipped())) || 
				(em->Has2HRangedWeapEquipped() && HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kAttackLH)))
			};
		}

		bool AttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// If arms rotation is disabled, will draw weapons/magic/fists on release instead of attacking.
			bool drawn = a_p->coopActor->IsWeaponDrawn();
			if (!drawn && !a_p->isTransformed)
			{
				return true;
			}

			// P1 is AI driven while mounted, so the game handles attacks by default.
			if (a_p->isPlayer1 && a_p->coopActor->IsOnMount())
			{
				return false;
			}

			// Must have weapons/magic out and not blocking.
			bool baselineCheck = 
			{
				drawn && !a_p->pam->isBlocking
			};

			// Return early if baseline check fails.
			if (!baselineCheck)
			{
				return false;
			}

			const auto& em = a_p->em;
			// Just have to ensure the transformed player does not have a spell equipped in the RH.
			if (a_p->isTransformed && !em->HasRHSpellEquipped())
			{
				return true;
			}

			return 
			{
				// Unmounted: Does not have a RH spell equipped or is unarmed.
				// Mounted: Has a melee weapon equipped.
				(!em->HasRHSpellEquipped() || em->IsUnarmed() ||
				(a_p->coopActor->IsOnMount() && (em->HasLHMeleeWeapEquipped() || em->HasRHMeleeWeapEquipped() || em->Has2HMeleeWeapEquipped())))
			};
		}

		bool Bash(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// No weapon/magic out means no bash.
			if (!a_p->coopActor->IsWeaponDrawn())
			{
				return false;
			}

			// Not attacking and must be blocking beforehand.
			// Then check stamina requirement.
			return !a_p->pam->isAttacking && a_p->pam->IsPerforming(InputAction::kBlock) && HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kBash);
		}

		bool Block(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can't block when weapons/magic aren't drawn or if transformed.
			if (!a_p->coopActor->IsWeaponDrawn() || a_p->isTransformed)
			{
				return false;
			}

			const auto& em = a_p->em;
			// Not mounted, LH must be empty with anything but a staff in the RH, 
			// or LH must contain a shield/torch or a 2H weapon.
			return 
			{
				!a_p->coopActor->IsOnMount() &&
				((em->HasTorchEquipped() || em->HasShieldEquipped() || em->Has2HMeleeWeapEquipped()) ||
				 (em->HandIsEmpty(false) && !em->HandIsEmpty(true) && !em->HasRHStaffEquipped()))
			};
		}

		bool CastLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Spells are not out.
			if (!a_p->coopActor->IsWeaponDrawn())
			{
				return false;
			}

			// Do not cast when swapping out weapons/spells. Can lead to crashes.
			bool isEquipping = false;
			a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			bool isUnequipping = false;
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);

			if (isEquipping || isUnequipping)
			{
				return false;
			}

			// No conditions for P1 since the game handles
			// conditions when it processes the trigger button event that is sent.
			if (a_p->isPlayer1)
			{
				return a_p->em->HasLHSpellEquipped();
			}
			else
			{
				// Cast if LH contains a spell and the player has enough magicka to cast.
				return a_p->em->HasLHSpellEquipped() && HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kCastLH);
			}
		}

		bool CastRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Spells are not out.
			if (!a_p->coopActor->IsWeaponDrawn())
			{
				return false;
			}

			// Do not cast when swapping out weapons/spells. Can lead to crashes.
			bool isEquipping = false;
			a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			bool isUnequipping = false;
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);

			if (isEquipping || isUnequipping)
			{
				return false;
			}

			// No conditions for P1 since the game handles
			// conditions when it processes the trigger button event that is sent.
			if (a_p->isPlayer1)
			{
				return a_p->em->HasRHSpellEquipped();
			}
			else
			{
				// Cast if RH contains a spell, and the player has enough magicka to cast.
				return a_p->em->HasRHSpellEquipped() && HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kCastRH);
			}
		}

		bool ChangeDialoguePlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Dialogue menu must be open to swap control to another player.
			if (auto ui = RE::UI::GetSingleton(); ui) 
			{
				return ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
			}

			return false;
		}

		bool CoopSummoningMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Normal menu check + no active players can be in combat.
			bool playerInCombat = std::any_of(glob.coopPlayers.begin(), glob.coopPlayers.end(), [](const auto& p) { return p->isActive && p->coopActor->IsInCombat(); });
			return PlayerCanOpenMenu(a_p) && !playerInCombat;
		}

		bool DebugRagdollPlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can ragdoll if not controlling menus.
			return GlobalCoopData::IsNotControllingMenus(a_p->controllerID);
		}

		bool DebugRefreshPlayerManagers(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can refresh player managers if not controlling menus.
			return GlobalCoopData::IsNotControllingMenus(a_p->controllerID);
		}

		bool DebugResetPlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can reset player if no temporary menus are open.
			return Util::MenusOnlyAlwaysOpen();
		}

		bool Dismount(const std::shared_ptr<CoopPlayer>& a_p)
		{
			if (a_p->isPlayer1) 
			{
				// Dismount only if mounted.
				return a_p->coopActor->IsOnMount() || a_p->coopActor->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal;
			}
			else
			{
				// Dismount if mounted, or exit furniture if running the interaction package.
				return a_p->coopActor->IsOnMount() || a_p->coopActor->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal || a_p->pam->GetCurrentPackage() != a_p->pam->GetDefaultPackage();
			}
		}

		bool Dodge(const std::shared_ptr<CoopPlayer>& a_p) 
		{
			// Dodge if the player has enough stamina.
			// TODO: Support as many dodge mods as possible.
			// Defaulting to the mod's generic dash dodge for now.
			// Prospective compatibility:
			// - TK Dodge RE
			// - TUDM
			// - DMCO

			bool tkDodging = false;
			a_p->coopActor->GetGraphVariableBool("bIsDodging", tkDodging);
			auto charController = a_p->coopActor->GetCharController();
			// Can't dodge if already dodging, airborne, mounted, sneaking while not transformed,
			// blocking, swimming, flying, or not having enough stamina.
			return 
			(
				(!tkDodging && !a_p->mm->isDashDodging) && 
				(
					(a_p->mm->isParagliding) || 
					(
						!a_p->mm->isAirborneWhileJumping && 
						charController && charController->context.currentState == RE::hkpCharacterStateType::kOnGround &&
						charController->surfaceInfo.supportedState.all(RE::hkpSurfaceInfo::SupportedState::kSupported)
					)
				) &&
				(!a_p->coopActor->IsSneaking() || a_p->isTransformed) && 
				(
					!a_p->coopActor->IsOnMount() && 
					!a_p->pam->IsPerforming(InputAction::kBlock) &&
					!a_p->coopActor->IsSwimming() && 
					!a_p->coopActor->IsFlying()
				) &&
				(HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kDodge))
			);
		}

		bool FaceTarget(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Do not face a target when mounted because horses do not have backpedaling or strafe animations
			// and will only be able to move towards the target or moonwalk backwards.
			return !a_p->coopActor->IsOnMount() && !a_p->coopActor->IsSprinting();
		}

		bool Favorites(const std::shared_ptr<CoopPlayer>& a_p)
		{
			bool isEquipping = false;
			bool isUnequipping = false;
			a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);

			// No supported menus open or control is obtainable AND
			// Either not transformed and not (un)equipping OR
			// Player is a Vampire Lord (access to vampiric spells through the FavoritesMenu)
			return
			{
				(!glob.supportedMenuOpen.load() || GlobalCoopData::CanControlMenus(a_p->controllerID)) &&
				((!Util::IsRaceWithTransformation(a_p->coopActor->race) && !isEquipping && !isUnequipping) || (Util::IsVampireLord(a_p->coopActor.get())))
			};
		}

		bool GrabRotateYZ(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Must be moving the RS and grabbing at least 1 object.
			const auto& rsState = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
			return rsState.normMag > 0.0f && a_p->tm->rmm->isGrabbing && a_p->tm->rmm->GetNumGrabbedRefrs() > 0;
		}

		bool Jump(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can only start jumping if not ragdolled and not getting up or staggered.
			if (a_p->coopActor->IsInRagdollState() || a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal)
			{
				return false;
			}

			// Can jump if using furniture.
			if ((!a_p->isPlayer1 && a_p->mm->interactionPackageRunning) || 
				(a_p->coopActor->GetOccupiedFurniture() && !a_p->coopActor->IsOnMount()))
			{
				return true;
			}

			if (auto charController = a_p->mm->movementActor->GetCharController(); charController)
			{
				// Cannot start jumping if not on the ground.
				bool stateAllowsJump = 
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
				if (stateAllowsJump) 
				{
					// Check slope angle for the surface the player would like to jump off.
					// Must be less than the defined max jump surface slope angle.
					const float& zComp = charController->surfaceInfo.surfaceNormal.quad.m128_f32[2];
					float supportAng = FLT_MAX;
					// Vector along the surface is at 90 degrees to the normal, so subtract the normal from PI / 2.
					if (charController->surfaceInfo.surfaceNormal.Length3() != 0.0f) 
					{
						supportAng = PI / 2.0f - fabsf(asinf(charController->surfaceInfo.surfaceNormal.quad.m128_f32[2]));
					}
					else
					{
						// Raycast as a fallback option if the support surface normal data is unavailable (0).
						glm::vec4 start = ToVec4(a_p->coopActor->data.location);
						glm::vec4 end = start - glm::vec4(0.0f, 0.0f, a_p->coopActor->GetHeight(), 0.0f);
						auto rayResult = Raycast::hkpCastRay(start, end, true, false);
						if (rayResult.hit)
						{
							supportAng = PI / 2.0f - fabsf(asinf(rayResult.rayNormal.z));
						}
					}	

					return supportAng < Settings::fJumpingMaxSlopeAngle;
				}
			}

			return false;
		}

		bool PowerAttackDual(const std::shared_ptr<CoopPlayer>& a_p)
		{
			return CanPowerAttack(a_p, InputAction::kPowerAttackDual);
		}

		bool PowerAttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			return CanPowerAttack(a_p, InputAction::kPowerAttackLH);
		}

		bool PowerAttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			return CanPowerAttack(a_p, InputAction::kPowerAttackRH);
		}

		bool QuickSlotCast(const std::shared_ptr<CoopPlayer>& a_p)
		{
			return true;
			// Cast if QS contains a spell, the instant caster is available, and the player has enough magicka.
			/*return 
			(
				a_p->em->equippedForms[!EquipIndex::kQuickSlotSpell] &&
				a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant) &&
				HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kQuickSlotCast)
			); */
		}

		bool QuickSlotItem(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Allow usage of a QS item only if the player HAD at least 1 in their inventory,
			// as indicated by presence of an entry in the player's inventory counts.
			// Otherwise, clear out the slot later when the action is started later.
			auto qsItem = a_p->em->equippedForms[!EquipIndex::kQuickSlotItem];
			auto qsBoundObj = qsItem && qsItem->IsBoundObject() ? qsItem->As<RE::TESBoundObject>() : nullptr;
			const bool hadItem = 
			{ 
				qsBoundObj && 
				qsBoundObj->Is(RE::FormType::AlchemyItem, RE::FormType::Ingredient) &&
				a_p->coopActor->GetInventoryCounts().contains(qsBoundObj) 
			};
			return hadItem;
		}

		bool ResetAim(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can reset aim pitch if not grabbing anything.
			return true;  //!a_p->tm->rmm->isGrabbing;
		}

		bool Sheathe(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can only sheathe with co-op companions if they are not in the process
			// of attacking, since actions such as loading a crossbow while
			// attempting to sheathe cause the crossbow to disappear.
			return (a_p->isPlayer1 || !a_p->pam->isAttacking);
		}

		bool Shout(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Must have a voice form equipped.
			return (bool)a_p->em->voiceForm;
		}

		bool Sneak(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// On the ground and not mounted.
			const auto charController = a_p->coopActor->GetCharController();
			return 
			(
				!a_p->coopActor->IsSwimming() && !a_p->coopActor->IsOnMount() &&
				a_p->coopActor->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && charController && 
				charController->context.currentState == RE::hkpCharacterStateType::kOnGround
			);
		}

		bool SpecialAction(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Perform special contextual actions based on what weapons/magic the player has
			// equipped in either hand.
			// Additional actions can also be performed on consecutive presses.
			// Possible TODO: add config option to trigger a custom action with this bind.

			const auto& pam = a_p->pam;
			const auto& em = a_p->em;
			// Clear old requested action first.
			pam->reqSpecialAction = SpecialActionType::kNone;

			// [Single Tap/Hold Actions]:
			// - Get up
			// - Transformation
			// - Block
			// - Dual Cast
			// - Cast With Both Hands
			// - Quick Cast
			// - Cycle or Play Emote Idle
			// [Double Tap Actions]:
			// - Ragdoll
			// - Bash
			// - Dodge

			const bool isRagdolled = a_p->coopActor->IsInRagdollState();
			// Single tap/hold.
			if (!pam->PassesConsecTapsCheck(InputAction::kSpecialAction))
			{
				if (a_p->isTransforming)
				{
					return false;
				}

				// Hold to get up.
				if (isRagdolled)
				{
					// Attempt to get up if grabbed or on the ground.
					bool isGrabbed = std::any_of(glob.coopPlayers.begin(), glob.coopPlayers.end(),
						[&a_p](const auto& p) {
							return p->isActive && p->coopActor != a_p->coopActor && p->tm->rmm->IsManaged(a_p->coopActor->GetHandle(), true);
						});

					auto charController = a_p->coopActor->GetCharController();
					bool shouldAttemptGetUp = 
					{
						((isGrabbed) ||
						(charController && charController->context.currentState == RE::hkpCharacterStateType::kOnGround))
					};
					if (shouldAttemptGetUp)
					{
						pam->reqSpecialAction = SpecialActionType::kGetUp;
					}
				}
				else if (a_p->isTransformed)
				{
					// If transformed into a Werewolf, show transformation time remaining or feed on a targeted corpse.
					// Currently, nothing for Vampire Lords.
					// Revert transformation if transformed into another non-playable race.
					pam->reqSpecialAction = SpecialActionType::kTransformation;
				}
				else if (a_p->mm->isParagliding && em->quickSlotSpell)
				{
					// Quick slot cast while paragliding.
					pam->reqSpecialAction = SpecialActionType::kQuickCast;
				}
				else if (a_p->coopActor->IsWeaponDrawn())
				{
					if (em->IsUnarmed() || em->Has2HMeleeWeapEquipped() || em->Has2HRangedWeapEquipped() ||
						(em->HasRHMeleeWeapEquipped()) || (em->HasLHMeleeWeapEquipped() && em->HandIsEmpty(true)) || em->HasTorchEquipped())
					{
						// Block if unarmed, if there's a 2H weapon equipped, if the RH has a weapon,
						// if the player has a LH weapon and an empty RH, or the LH has a torch.
						pam->reqSpecialAction = SpecialActionType::kBlock;
					}
					else if (HelperFuncs::CanDualCast(a_p))
					{
						// TODO: Dual cast when LH and RH both have the same 1H spells equipped and the player has the dual cast perk.
						pam->reqSpecialAction = SpecialActionType::kDualCast;
					}
					else if ((em->HasLHSpellEquipped() && em->HasRHSpellEquipped()) || (em->HasLHStaffEquipped() && em->HasRHStaffEquipped()))
					{
						// Cast simultaneously with both hands if both have a spell/staff equipped.
						pam->reqSpecialAction = SpecialActionType::kCastBothHands;
					}
					else if (em->quickSlotSpell)
					{
						// Quick slot cast otherwise.
						pam->reqSpecialAction = SpecialActionType::kQuickCast;
					}
				}
				else if (a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kGetUp)
				{
					// Emote when weapons are sheathed and not getting up/ragdolling.
					pam->reqSpecialAction = SpecialActionType::kCycleOrPlayEmoteIdle;
				}
			}
			else
			{
				// 2+ consecutive taps.
				// Nothing to do right now when transformed.
				if (a_p->isTransforming)
				{
					return false;
				}

				// Actions to perform when not ragdolled or 
				// transformed and the player has drawn their weapon/magic and is not paragliding.
				if (!isRagdolled && !a_p->isTransformed && a_p->coopActor->IsWeaponDrawn() && !a_p->mm->isParagliding)
				{
					if (em->quickSlotItem)
					{
						// Use quick slot item if it is equipped.
						pam->reqSpecialAction = SpecialActionType::kQuickItem;
					}
					else if ((em->IsUnarmed() || em->Has2HMeleeWeapEquipped() || em->Has2HRangedWeapEquipped()) || em->HasRHMeleeWeapEquipped())
					{
						// Bash if unarmed or if a 2H weapon is equipped or if the RH contains a weapon while the LH is empty.
						pam->reqSpecialAction = SpecialActionType::kBash;
					}
					else
					{
						// Dodge otherwise.
						pam->reqSpecialAction = SpecialActionType::kDodge;
					}
				}
				else if (Settings::bAllowFlopping)
				{
					bool isGrabbed = std::any_of(glob.coopPlayers.begin(), glob.coopPlayers.end(),
						[&a_p](const auto& p) {
							return p->isActive && p->coopActor != a_p->coopActor && p->tm->rmm->IsManaged(a_p->coopActor->GetHandle(), true);
						});

					// Can flop if grabbed by another player or if not ragdolled 
					// and weapons/magic are sheathed or the player is paragliding.
					if ((isGrabbed) || ((!isRagdolled) && (!a_p->coopActor->IsWeaponDrawn() || a_p->mm->isParagliding)))
					{
						// Flop (if enabled) when weapons are sheathed or the player is transformed.
						// Must also not be ragdolling, unless grabbed by another player.
						pam->reqSpecialAction = SpecialActionType::kRagdoll;
					}
				}
			}

			// If a new special action to perform was chosen, perform HMS AV checks.
			return pam->reqSpecialAction != SpecialActionType::kNone && HelperFuncs::CanPerformSpecialAction(a_p, pam->reqSpecialAction);
		}

		bool Sprint(const std::shared_ptr<CoopPlayer>& a_p)
		{
			bool canPerform = false;
			if (a_p->coopActor->IsSneaking()) 
			{
				// Must be moving while sneaking and have the skill-giving perk.
				canPerform =  
				(
					a_p->mm->lsMoved &&
					a_p->coopActor->HasPerk(glob.sneakRollPerk)
				);
			}
			else if (a_p->pam->IsPerforming(InputAction::kBlock))
			{
				// Moving and blocking with a shield equipped + have the required perk.
				canPerform =
				(
					a_p->mm->lsMoved &&
					a_p->em->HasShieldEquipped() &&
					a_p->coopActor->HasPerk(glob.shieldChargePerk)
				);
			}
			else
			{
				// Must be moving/paragliding, not attacking/bashing/blocking/casting,
				// and cannot be dash dodging//ragdolled without M.A.R.F.
				canPerform =  
				(
					(a_p->mm->lsMoved || a_p->mm->isParagliding) &&
					(!a_p->pam->isJumping) &&
					(!a_p->pam->isAttacking && !a_p->pam->isBashing && !a_p->pam->isBlocking && !a_p->pam->isInCastingAnim) && 
					(!a_p->mm->isDashDodging) &&
					(!a_p->coopActor->IsInRagdollState() || a_p->tm->isMARFing)
				);
			}

			// Return here if false, no need for stamina check.
			if (!canPerform)
			{
				return false;
			}

			// Must also have enough stamina.
			return HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kSprint);
		}

		bool TeleportToPlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// No menus that stop movement (ex. StatsMenu/MapMenu/LockpickingMenu) can be open
			// and the player must not be controlling menus.
			return !Util::OpenMenuStopsMovement() && GlobalCoopData::IsNotControllingMenus(a_p->controllerID);
		}

		//=========================================================================================
		// [Multiple Use]
		//=========================================================================================
	
		bool CamActive(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Co-op camera manager thread is active.
			return glob.cam->IsRunning();
		}

		bool CanAdjustCamera(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can't adjust something that isn't active.
			if (!CamActive(a_p)) 
			{
				return false;
			}

			// Can't adjust the camera if the player is trying to rotate their arms, 
			// since the RS input is used for that purpose.
			bool isTryingToRotateArms =
			{ 
				(Settings::bRotateArmsWhenSheathed && !a_p->coopActor->IsWeaponDrawn()) && 
				a_p->pam->AllInputsPressedForAtLeastOneAction
				(
					InputAction::kRotateRightShoulder,
					InputAction::kRotateLeftShoulder,
					InputAction::kRotateRightForearm,
					InputAction::kRotateLeftForearm,
					InputAction::kRotateRightHand,
					InputAction::kRotateLeftHand
				)
			};

			const auto& rsState = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
			return rsState.normMag > 0.0f && !isTryingToRotateArms;
		}

		bool CanPowerAttack(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// If arms rotation is disabled, will draw weapons/magic/fists on release instead of attacking.
			bool drawn = a_p->coopActor->IsWeaponDrawn();
			if (!drawn && !a_p->isTransformed)
			{
				return true;
			}

			// Must have weapons/magic drawn, not be power attacking, not mounted, and have enough stamina.
			bool baselineCheck = 
			{
				drawn && !a_p->pam->isPowerAttacking && !a_p->coopActor->IsOnMount() &&
				HelperFuncs::EnoughOfAVToPerformPA(a_p, a_action)
			};

			// Return early if baseline check fails.
			if (!baselineCheck)
			{
				return false;
			}

			// No need to consider weapons/fists or perks if transformed.
			if (a_p->isTransformed)
			{
				return true;
			}

			// Check sprint or normal power attack conditions next.
			if (a_p->pam->isSprinting || a_p->coopActor->IsSprinting()) 
			{
				// Next, check if a melee weapon is equipped in the appropriate hand and if the player has the appropriate perk.
				const auto& em = a_p->em;
				switch (a_action)
				{
				case InputAction::kPowerAttackLH:
				{
					// Must have the corresponding perk and a melee weapon in the LH.
					return 
					{
						((a_p->coopActor->HasPerk(glob.criticalChargePerk) && em->HasLHMeleeWeapEquipped()) ||
						(a_p->coopActor->HasPerk(glob.greatCriticalChargePerk) && em->Has2HMeleeWeapEquipped()))
					};
				}
				case InputAction::kPowerAttackRH:
				{
					// Must have the corresponding perk and a melee weapon in the RH.
					return 
					{
						((a_p->coopActor->HasPerk(glob.criticalChargePerk) && em->HasRHMeleeWeapEquipped()) ||
						(a_p->coopActor->HasPerk(glob.greatCriticalChargePerk) && em->Has2HMeleeWeapEquipped()))
					};
				}
				default:
					return false;
				}
			}
			else
			{
				// Must have a melee weapon in the appropriate hand.
				switch (a_action)
				{
				case InputAction::kPowerAttackLH:
				{
					// Either unarmed, LH is empty, or using a 1H LH weapon.
					return (a_p->em->IsUnarmed() || a_p->em->LHEmpty() || a_p->em->HasLHMeleeWeapEquipped());
				}
				case InputAction::kPowerAttackRH:
				{
					// Either unarmed RH is empty or using a weapon in the RH.
					return (a_p->em->IsUnarmed() || a_p->em->RHEmpty() || a_p->em->HasRHMeleeWeapEquipped() || a_p->em->Has2HMeleeWeapEquipped());
				}
				case InputAction::kPowerAttackDual:
				{
					// Either unarmed, has a 2H weapon equipped, or is dual wielding (two 1H weapons).
					return (a_p->em->IsUnarmed() || a_p->em->Has2HMeleeWeapEquipped() || a_p->em->IsDualWielding());
				}
				default:
					return false;
				}
			}
		}

		bool CanRotateArms(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// The player can rotate their arms if the setting is enabled,
			// if they are in dialogue or not controlling menus,
			// if they are not knocked down, getting up, or staggered,
			// and if their weapons/magic are sheathed.

			auto ui = RE::UI::GetSingleton();
			if (!ui)
			{
				return false;
			}

			bool inDialogueOrNotControllingMenus = 
			{
				(glob.menuCID != a_p->controllerID) ||
				(ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
			};

			return 
			(
				Settings::bRotateArmsWhenSheathed &&
				inDialogueOrNotControllingMenus &&
				a_p->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal && 
				!a_p->coopActor->IsWeaponDrawn()
			);
		}

		bool CycleEquipment(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// NOTE: (Un)equipping when attempting to cycle equipment used to lead to crashes,
			// so if these issues resurface, I'll uncomment the lines related to equip state.
			bool isAttacking = false;
			bool isCasting = false;
			bool isCastingDual = false;
			bool isCastingLH = false;
			bool isCastingRH = false;
			a_p->coopActor->GetGraphVariableBool("IsAttacking", isAttacking);
			a_p->coopActor->GetGraphVariableBool("IsAttacking", isAttacking);
			a_p->coopActor->GetGraphVariableBool("IsCastingDual", isCastingDual);
			a_p->coopActor->GetGraphVariableBool("IsCastingLeft", isCastingLH);
			a_p->coopActor->GetGraphVariableBool("IsCastingRight", isCastingRH);
			isCasting = isCastingDual || isCastingLH || isCastingRH;

			/*bool isEquipping = false;
			bool isUnequipping = false;*/
			/*a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);*/

			bool inventoryCopiedToP1 = glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory);
			return !isAttacking && !isCasting && !inventoryCopiedToP1;	//&& !isEquipping && !isUnequipping;
		}

		bool PlayerCanOpenMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Odd crash when equipping magic/weapons while opening the Magic/Favorites menus,
			// so ensure the player is not (un)equipping before opening any menus.
			bool isEquipping = false;
			bool isUnequipping = false;
			a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			bool canControlMenus = GlobalCoopData::CanControlMenus(a_p->controllerID);
			// No supported menus open or can obtain menu control,
			// and either a non-playable race, or not (un)equipping as a playable race.
			// Non-playable races can still access menus which do not allow them to equip items.
			return 
			(
				(!glob.supportedMenuOpen.load() || canControlMenus) && 
				(Util::IsRaceWithTransformation(a_p->coopActor->race) || (!isEquipping && !isUnequipping))
			);
		}

		bool PlayerCanOpenItemMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// No supported menus open or can obtain menu control and
			// is part of a playable race and not (un)equipping.
			// Equipping items when not a member of a playable race causes problems,
			// so to prevent issues, we don't open any item menus under these conditions.
			bool isEquipping = false;
			bool isUnequipping = false;
			a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			return 
			(
				(!glob.supportedMenuOpen.load() || GlobalCoopData::CanControlMenus(a_p->controllerID)) &&
				!Util::IsRaceWithTransformation(a_p->coopActor->race) &&
				!isEquipping && !isUnequipping
			);
		}

		bool PlayerCanOpenPerksMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// By 'perks menus', I mean menus that give access to the player's perk tree,
			// whether directly (StatsMenu) or indirectly (TweenMenu).
			
			// Same general condition check.
			bool baselineCheck = PlayerCanOpenMenu(a_p);
			if (!baselineCheck)
			{
				return false;
			}

			if (a_p->isPlayer1)
			{
				// No additional conditions for P1.
				return true;
			}
			else
			{
				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
				// Only open if P1 is transformed or if this co-op companion player is not transformed.
				// Don't want to modify default skill tree perks/HMS AVs for co-op companions while transformed.
				// Will change once I figure out how to open the Stats Menu with the special Werewolf and Vampire Lord perk trees
				// when P1 is not transformed.
				return coopP1->isTransformed || !a_p->isTransformed;
			}
		}
	};

	namespace HelperFuncs
	{
		const bool ActionJustStarted(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Action stage is started and performed duration is at 0.
			const auto& paState = a_p->pam->paStatesList[!a_action - !InputAction::kFirstAction];
			return paState.perfStage == PerfStage::kStarted && paState.secsPerformed == 0.0f;
		}

		void BashInstant(const std::shared_ptr<CoopPlayer>& a_p)
		{
			bool wasBlocking = a_p->coopActor->IsBlocking();
			a_p->coopActor->NotifyAnimationGraph("attackStop");
			a_p->coopActor->NotifyAnimationGraph("blockStart");
			// 'BowBash' triggers the normal bash animation,
			// whereas the 'BashStart' idle does not (fails conditions).
			Util::PlayIdle("BowBash", a_p->coopActor.get());
			// Send block start/stop animation events.
			if (wasBlocking)
			{
				a_p->coopActor->NotifyAnimationGraph("blockStart");
			}
		}

		void BlockInstant(const std::shared_ptr<CoopPlayer>& a_p, bool&& a_shouldStart)
		{
			// Send block start/stop animation events.
			if (a_shouldStart) 
			{
				a_p->coopActor->NotifyAnimationGraph("attackStop");
				a_p->coopActor->NotifyAnimationGraph("blockStart");
			}
			else
			{
				a_p->coopActor->NotifyAnimationGraph("blockStop");
			}
		}

		const bool CanCycleOnHold(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Cycle on hold not enabled or not cycling emote idles.
			// Emote idles are always cycled on hold.
			if (!Settings::bHoldToCycle && a_action != InputAction::kSpecialAction) 
			{
				return false;
			}

			// Cycle once on first press.
			// Subsequent cyclings at a set interval.
			if (bool justStarted = HelperFuncs::ActionJustStarted(a_p, a_action); justStarted) 
			{
				a_p->lastCyclingTP = SteadyClock::now();
				return true;
			}

			// Cycling interval passed, can cycle.
			return Util::GetElapsedSeconds(a_p->lastCyclingTP) >= max(0.1f, Settings::fSecsCyclingInterval);
		}

		bool CanDualCast(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// TODO:
			// Dual cast if the player has the same 1H spell equipped in both hands,
			// and has the required dual casting perk.
			// Overrides default single-hand double cast.
			
			// Not a player 1 action, since single trigger buttons events must
			// be sent when dual casting.
			bool isEquipping = false;
			a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			bool isUnequipping = false;
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			// Can't be (un)equipping items either.
			if (a_p->isPlayer1 || isEquipping || isUnequipping)
			{
				return false;
			}

			const auto& em = a_p->em;
			const auto lhSpell = em->GetLHSpell();
			const auto rhSpell = em->GetRHSpell();
			bool sameSpellInBothHands = 
			{ 
				lhSpell && rhSpell &&
				em->copiedMagicFormIDs[!PlaceholderMagicIndex::kLH] == em->copiedMagicFormIDs[!PlaceholderMagicIndex::kRH] &&
				lhSpell->equipSlot != glob.bothHandsEquipSlot 
			};

			bool canDualCast = false;
			// Must have the same 1H spell equipped in both hands.
			if (sameSpellInBothHands)
			{
				auto lhSpellSchool = lhSpell->avEffectSetting ? lhSpell->avEffectSetting->data.associatedSkill : RE::ActorValue::kNone;
				switch (lhSpellSchool)
				{
				case RE::ActorValue::kAlteration:
				{
					canDualCast = a_p->coopActor->HasPerk(glob.dualCastingAlterationPerk);
					break;
				}
				case RE::ActorValue::kConjuration:
				{
					canDualCast = a_p->coopActor->HasPerk(glob.dualCastingConjurationPerk);
					break;
				}
				case RE::ActorValue::kDestruction:
				{
					canDualCast = a_p->coopActor->HasPerk(glob.dualCastingDestructionPerk);
					break;
				}
				case RE::ActorValue::kIllusion:
				{
					canDualCast = a_p->coopActor->HasPerk(glob.dualCastingIllusionPerk);
					break;
				}
				case RE::ActorValue::kRestoration:
				{
					canDualCast = a_p->coopActor->HasPerk(glob.dualCastingRestorationPerk);
					break;
				}
				default:
					return false;
				}
			}

			return canDualCast;
		}

		const bool CanPerformSpecialAction(const std::shared_ptr<CoopPlayer>& a_p, const SpecialActionType& a_actionType)
		{
			// HMS AV check for special actions.
			const auto& pam = a_p->pam;
			auto& paState = a_p->pam->paStatesList[!InputAction::kSpecialAction - !InputAction::kFirstAction];
			float cost = 0.0f;
			float costPerUpdate = 0.0f;
			bool canPerform = false;
			switch (a_actionType)
			{
			case SpecialActionType::kBash:
			{
				// Don't need to handle the cost, since the player action commands will do that.
				// Only have to check if the player's stamina is on cooldown or not.
				canPerform = pam->secsTotalStaminaRegenCooldown == 0.0f;
				break;
			}
			case SpecialActionType::kCastBothHands:
			{
				if (a_p->em->HasLHSpellEquipped() && a_p->em->HasRHSpellEquipped())
				{
					auto lhSpell = a_p->em->GetLHSpell();
					auto rhSpell = a_p->em->GetRHSpell();
					// Cost multiplier from here:
					// https://en.uesp.net/wiki/Skyrim:Magic_Overview#Dual-Casting
					float costLH = lhSpell->CalculateMagickaCost(a_p->coopActor.get());
					float costRH = rhSpell->CalculateMagickaCost(a_p->coopActor.get());

					// Set individual costs for magicka consumption for each casting action.
					a_p->pam->paStatesList[!InputAction::kCastLH - !InputAction::kFirstAction].avCost = costLH;
					a_p->pam->paStatesList[!InputAction::kCastRH - !InputAction::kFirstAction].avCost = costRH;
					// Must be able to cast both spells at once, so the magicka check is the sum of both spells' costs.
					cost = costLH + costRH;
					canPerform = pam->currentMagicka - cost > 0.0f;
				}
				else if (a_p->em->HasLHStaffEquipped() && a_p->em->HasRHStaffEquipped())
				{
					auto lhWeap = a_p->em->GetLHWeapon();
					auto rhWeap = a_p->em->GetRHWeapon();
					if (lhWeap->formEnchanting && rhWeap->formEnchanting)
					{
						// Both staves must have enough charge.
						canPerform = lhWeap->amountofEnchantment > 0 && rhWeap->amountofEnchantment > 0;
					}
				}

				break;
			}
			case SpecialActionType::kDodge:
			{
				// WIP: Custom dodge cost that factors LS displacement, current equipped weight, and the player's carryweight.
				float carryWeightRatioMult = 
				(
					0.1f +
					min
					(
						0.9f, 
						sqrtf
						(
							a_p->coopActor->GetEquippedWeight() / a_p->coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight)
						)
					)
				);
				// Dash dodging costs more when the LS is displaced further from center (longer dodge).
				const float& lsMag = glob.cdh->GetAnalogStickState(a_p->controllerID, true).normMag;
				float dashDodgeCommitmentMult = 
				(
					(
						(lsMag == 0.0f) || 
						(!Settings::bUseDashDodgeSystem && ALYSLC::TKDodgeCompat::g_tkDodgeInstalled)
					) ?
					1.0f : 
					lsMag
				);
				cost = a_p->pam->baseStamina * carryWeightRatioMult * dashDodgeCommitmentMult;

				// Set Dodge action's cost.
				a_p->pam->paStatesList[!InputAction::kDodge - !InputAction::kFirstAction].avCost = cost;
				// Must not be on cooldown.
				canPerform = pam->secsTotalStaminaRegenCooldown == 0.0f;
				break;
			}
			case SpecialActionType::kDualCast:
			{
				if (a_p->em->HasLHSpellEquipped() && a_p->em->HasRHSpellEquipped())
				{
					auto lhSpell = a_p->em->GetLHSpell();
					auto rhSpell = a_p->em->GetRHSpell();
					// Cost multiplier from here:
					// https://en.uesp.net/wiki/Skyrim:Magic_Overview#Dual-Casting
					// Same spells in both hands, same cost.
					cost = 2.8f * lhSpell->CalculateMagickaCost(a_p->coopActor.get());
					canPerform = pam->currentMagicka - cost > 0.0f;
				}

				break;
			}
			case SpecialActionType::kQuickCast:
			{
				// Must have a quick slot spell equipped.
				if (auto qsSpell = a_p->em->equippedForms[!EquipIndex::kQuickSlotSpell]; qsSpell && qsSpell->Is(RE::FormType::Spell))
				{
					auto asSpell = qsSpell->As<RE::SpellItem>();
					cost = asSpell->CalculateMagickaCost(a_p->coopActor.get());
					// Cost per frame for concentration spells.
					cost *= asSpell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration ? *g_deltaTimeRealTime : 1.0f;

					// Set QuickSlotCast action's cost.
					a_p->pam->paStatesList[!InputAction::kQuickSlotCast - !InputAction::kFirstAction].avCost = cost;
					canPerform = pam->currentMagicka - cost > 0.0f;
				}

				break;
			}
			case SpecialActionType::kBlock:
			case SpecialActionType::kCycleOrPlayEmoteIdle:
			case SpecialActionType::kGetUp:
			case SpecialActionType::kNone:
			case SpecialActionType::kQuickItem:
			case SpecialActionType::kRagdoll:
			case SpecialActionType::kTransformation:
			{
				// No cost.
				canPerform = true;
				break;
			}
			default:
			{
				// Should not reach.
				break;
			}
			}

			// Set cost.
			paState.avCost = cost;
			return canPerform;
		}

		bool CheckForKillmove(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// If not using the killmoves system, nothing to do here.
			if (!Settings::bUseKillmovesSystem) 
			{
				return false;
			}

			bool performedKillmove = false;
			// Only check if not already in a killmove and not mounted.
			if (!a_p->coopActor->IsInKillMove() && !a_p->coopActor->IsOnMount())
			{
				// Get targeted actor.
				auto targetActorPtr = Util::GetActorPtrFromHandle(a_p->tm->selectedTargetActorHandle);
				int32_t otherPIndex = GlobalCoopData::GetCoopPlayerIndex(targetActorPtr);
				bool isOtherPlayer = otherPIndex != -1;
				// Must have a valid actor targeted.
				bool validState = 
				{ 
					targetActorPtr && 
					targetActorPtr->Is3DLoaded() && 
					!targetActorPtr->IsDead() && 
					!targetActorPtr->IsInKillMove() && 
					!targetActorPtr->IsOnMount() 
				};
				// If the targeted actor is another player, make sure they can be killmoved.
				bool canKillmoveOtherPlayer = 
				{
					isOtherPlayer && 
					Settings::bCanKillmoveOtherPlayers && 
					!glob.coopPlayers[otherPIndex]->isInGodMode && 
					Settings::vfDamageReceivedMult[glob.coopPlayers[otherPIndex]->playerID] > 0.0f
				};
				// Valid actor and is another killmove-able player or an NPC that isn't essential.
				if (validState && (canKillmoveOtherPlayer || (!isOtherPlayer && !targetActorPtr->IsEssential()))) 
				{
					// Ensure target is within actor's reach.
					float distToTarget = a_p->coopActor->data.location.GetDistance(targetActorPtr->data.location);
					float reach = a_p->em->GetMaxWeapReach();
					if (distToTarget <= reach) 
					{
						// Get skeleton name for target and check if it is a character.
						bool hasCharacterSkeleton = false;
						std::string skeleName = std::string("");
						if (auto targetRace = targetActorPtr->GetRace(); targetRace)
						{
							Util::GetSkeletonModelNameForRace(targetRace, skeleName);
							// No assigned racial killmoves if skeleton is not supported.
							hasCharacterSkeleton = Hash(skeleName) == "character"_h;
						}

						// Use generic killmoves on characters or on any race.
						bool canUseGenericKillmoves = hasCharacterSkeleton || Settings::bUseGenericKillmovesOnUnsupportedNPCs;
						// Get max health for the target.
						float baseHealth = targetActorPtr->GetBaseActorValue(RE::ActorValue::kHealth);
						float healthTempMod = targetActorPtr->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
						float maxHealth = baseHealth + healthTempMod;
						// Target actor's current health must be below the killmove health threshold.
						// Handle sneak killmoves later.
						if ((!a_p->coopActor->IsSneaking() || a_p->isTransformed) && targetActorPtr->GetActorValue(RE::ActorValue::kHealth) <= maxHealth * Settings::fKillmoveHealthFraction)
						{
							// Killmove chance check.
							// Generate random number from 0.0 to 1.0.
							std::mt19937 generator;
							generator.seed(SteadyClock::now().time_since_epoch().count());
							float rand = generator() / (float)((std::mt19937::max)());
							if (rand <= Settings::fKillmoveChance)
							{
								// Special cases (targets with character skeleton only).
								bool isVampireLord = Util::IsVampireLord(a_p->coopActor.get());
								bool isWerewolf = Util::IsWerewolf(a_p->coopActor.get());
								bool usingShield = (a_action == InputAction::kBash) && a_p->em->HasShieldEquipped();
								if (isVampireLord) 
								{
									if (canUseGenericKillmoves) 
									{
										// Can only perform if P1 or when not toggling levitation.
										if (a_p->isPlayer1 || !a_p->isTogglingLevitationStateTaskRunning) 
										{
											performedKillmove = PlayKillmoveFromList(a_p, targetActorPtr.get(), isOtherPlayer, hasCharacterSkeleton, glob.genericKillmoveIdles[!KillmoveType::kVampireLord]);
										}
									}
								}
								else if (isWerewolf)
								{
									if (canUseGenericKillmoves)
									{									
										performedKillmove = PlayKillmoveFromList(a_p, targetActorPtr.get(), isOtherPlayer, hasCharacterSkeleton, glob.genericKillmoveIdles[!KillmoveType::kWerewolf]);
									}
								}
								else if (usingShield)
								{
									if (canUseGenericKillmoves)
									{
										performedKillmove = PlayKillmoveFromList(a_p, targetActorPtr.get(), isOtherPlayer, hasCharacterSkeleton, glob.genericKillmoveIdles[!KillmoveType::kShield]);
									}
								}
								else
								{
									// All other weapon types' killmoves.
									auto weapType = RE::WEAPON_TYPE::kHandToHandMelee;
									bool physAttackAction = 
									{
										a_action == InputAction::kAttackRH || a_action == InputAction::kAttackLH ||
										a_action == InputAction::kPowerAttackRH || a_action == InputAction::kPowerAttackLH ||
										a_action == InputAction::kPowerAttackDual 
									};

									// Get weapon type based on triggering action.
									if (physAttackAction)
									{
										if (auto rhWeap = a_p->em->GetRHWeapon(); rhWeap && (a_action == InputAction::kAttackRH ||
											a_action == InputAction::kPowerAttackRH || a_action == InputAction::kPowerAttackDual))
										{
											weapType = rhWeap->GetWeaponType();
										}
										else if (auto lhWeap = a_p->em->GetLHWeapon(); lhWeap && (a_action == InputAction::kAttackLH || 
											a_action == InputAction::kPowerAttackLH))
										{
											weapType = lhWeap->GetWeaponType();
										}
									}

									// Weapon killmoves only.
									if (physAttackAction)
									{
										if (auto targetRace = targetActorPtr->GetRace(); targetRace)
										{
											auto killmoveType = KillmoveType::kNone;
											bool assignSkeletonBasedKillmoves = !skeleName.empty();
											// If skeleton is valid, check if any killmoves were assigned to it.
											if (assignSkeletonBasedKillmoves)
											{
												// Get killmoves list for this skeleton. List is then indexed with weap type.
												const auto& killmovesPerWeapTypeList = glob.skeletonKillmoveIdlesMap.at(Hash(skeleName));
												assignSkeletonBasedKillmoves = std::any_of(killmovesPerWeapTypeList.begin(), killmovesPerWeapTypeList.end(),
												[](const std::vector<RE::TESIdleForm*>& a_killmovesList) {
													return a_killmovesList.size() != 0;
												});
											}

											if (assignSkeletonBasedKillmoves)
											{
												// Choose killmove for this supported skeleton based on the current weapon type.
												bool is1HMWeap = 
												{
													weapType == RE::WEAPON_TYPE::kOneHandAxe ||
													weapType == RE::WEAPON_TYPE::kOneHandDagger ||
													weapType == RE::WEAPON_TYPE::kOneHandMace ||
													weapType == RE::WEAPON_TYPE::kOneHandSword
												};

												bool is2HMWeap = 
												{
													weapType == RE::WEAPON_TYPE::kStaff ||
													weapType == RE::WEAPON_TYPE::kTwoHandAxe ||
													weapType == RE::WEAPON_TYPE::kTwoHandSword
												};

												// Must use 1H or 2H melee weapons to perform race-specific killmoves.
												if (is1HMWeap || is2HMWeap)
												{
													killmoveType = is1HMWeap ? KillmoveType::k1H : KillmoveType::k2H;
													performedKillmove = PlayKillmoveFromList(a_p, targetActorPtr.get(), isOtherPlayer, hasCharacterSkeleton, glob.skeletonKillmoveIdlesMap.at(Hash(skeleName))[!killmoveType]);
												}
											}
											else if (canUseGenericKillmoves)
											{
												// Choose killmove from generic killmoves list if there is no supported skeleton.
												auto killmoveTypesList = GlobalCoopData::KILLMOVE_TYPES_FOR_WEAP_TYPE.at(weapType);
												// Check dual wield killmoves first if dual wielding.
												if (a_p->em->IsDualWielding())
												{
													killmoveTypesList.emplace_back(KillmoveType::kDW);
												}

												// Types list is given in ascending order of precedence.
												uint32_t typesListSize = killmoveTypesList.size();
												rand = generator() / (float)((std::mt19937::max)());
												// Randomize iteration direction.
												bool iterateForward = rand <= 0.5f;
												rand = std::clamp(generator() / (float)((std::mt19937::max)()), 0.0f, 0.99f);
												// Randomize starting index.
												uint32_t i = (uint32_t)(typesListSize * rand);
												uint8_t typesChecked = 0;
												// Keep checking until a killmove is performed or all types are checked.
												while (!performedKillmove && typesChecked < typesListSize)
												{
													killmoveType = killmoveTypesList[i];
													performedKillmove = PlayKillmoveFromList(a_p, targetActorPtr.get(), isOtherPlayer, hasCharacterSkeleton, glob.genericKillmoveIdles[!killmoveType]);
													
													// Wrap around to front/back depending on if iterating forwards/backwards.
													if (iterateForward)
													{
														if (i == typesListSize - 1)
														{
															i = 0;
														}
														else
														{
															++i;
														}
													}
													else
													{
														if (i == 0)
														{
															i = typesListSize - 1;
														}
														else
														{
															--i;
														}
													}

													++typesChecked;
												}
											}
										}
									}
									else if ((Settings::bUseUnarmedKillmovesForSpellcasting && canUseGenericKillmoves) && (a_action == InputAction::kCastLH || a_action == InputAction::kCastRH))
									{
										// Unarmed killmoves when casting spells at a close range target.
										auto killmoveType = KillmoveType::kH2H;
										// Stop casting before performing killmove because actor will otherwise continue casting for free afterwards, 
										// even if all cast binds are released.
										if (!a_p->isPlayer1) 
										{
											RemoveCastingPackage(a_p, true, true);
										}

										performedKillmove = PlayKillmoveFromList(a_p, targetActorPtr.get(), isOtherPlayer, hasCharacterSkeleton, glob.genericKillmoveIdles[!killmoveType]);
									}
								}
							}
						}
						else if (!isOtherPlayer && hasCharacterSkeleton && a_p->coopActor->IsSneaking() && Util::GetDetectionPercent(a_p->coopActor.get(), targetActorPtr.get()) != 100.0f)
						{
							// Choose sneak kill move on a non-player target that has not detected the player:
							// - No health threshold requirement.
							// - No sneak killmoves for non-humanoids.
							// - No shield sneak killmoves.
							// - And finally, if enabled, unarmed killmoves for spells.
							bool supportedAction = 
							{
								(a_action == InputAction::kAttackRH || a_action == InputAction::kAttackLH) ||
								((Settings::bUseUnarmedKillmovesForSpellcasting) && (a_action == InputAction::kCastRH || a_action == InputAction::kCastLH))
							};
							// Must be behind the target actor.
							float facingAngDiff = fabsf(Util::NormalizeAngToPi(targetActorPtr->GetHeading(true) - a_p->coopActor->GetHeading(true)));
							// 45 degree window.
							if (supportedAction && facingAngDiff <= PI / 4.0f)
							{
								auto weapType = RE::WEAPON_TYPE::kHandToHandMelee;
								// Get weapon type for the triggering action.
								if (a_action == InputAction::kAttackRH || a_action == InputAction::kAttackLH)
								{
									if (auto rhWeap = a_p->em->GetRHWeapon(); a_action == InputAction::kAttackRH && rhWeap)
									{
										weapType = rhWeap->GetWeaponType();
									}
									else if (auto lhWeap = a_p->em->GetLHWeapon(); a_action == InputAction::kAttackLH && lhWeap)
									{
										weapType = lhWeap->GetWeaponType();
									}
								}

								bool is1HMWeap = 
								{
									weapType == RE::WEAPON_TYPE::kOneHandAxe ||
									weapType == RE::WEAPON_TYPE::kOneHandDagger ||
									weapType == RE::WEAPON_TYPE::kOneHandMace ||
									weapType == RE::WEAPON_TYPE::kOneHandSword
								};

								bool is2HMWeap = 
								{
									weapType == RE::WEAPON_TYPE::kStaff ||
									weapType == RE::WEAPON_TYPE::kTwoHandAxe ||
									weapType == RE::WEAPON_TYPE::kTwoHandSword
								};

								auto killmoveType = KillmoveType::kSneakH2H;
								if (is1HMWeap)
								{
									killmoveType = KillmoveType::kSneak1H;
								}

								if (is2HMWeap)
								{
									killmoveType = KillmoveType::kSneak2H;
								}

								performedKillmove = PlayKillmoveFromList(a_p, targetActorPtr.get(), isOtherPlayer, hasCharacterSkeleton, glob.genericKillmoveIdles[!killmoveType]);
							}
						}
					}
				}
			}

			return performedKillmove;
		}

		const bool EnoughOfAVToPerformPA(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Does the player have enough health/magicka/stamina to perform the given action?
			const auto& pam = a_p->pam;
			// Total cost.
			float cost = 0.0f;
			// Cost once the player starts performing the action.
			float costToPerform = 0.0f;
			bool canPerform = false;
			if (a_action == InputAction::kActivate)
			{
				// Revive health cost.
				cost = GetPAHealthCost(a_p, a_action);
				costToPerform = cost * *g_deltaTimeRealTime;
				canPerform = a_p->isInGodMode || pam->currentHealth - costToPerform > Settings::fMinHealthWhileReviving;
			}
			else if ((a_action == InputAction::kCastLH && a_p->em->GetLHSpell()) || 
				     (a_action == InputAction::kCastRH && a_p->em->GetRHSpell()) || 
					 (a_action == InputAction::kQuickSlotCast && a_p->em->quickSlotSpell))
			{
				// Spellcast cost.
				cost = costToPerform = GetPAMagickaCost(a_p, a_action);
				bool isConcSpell = 
				{
					(
						a_action == InputAction::kCastLH && 
						a_p->em->GetLHSpell() && 
						a_p->em->GetLHSpell()->GetCastingType() == RE::MagicSystem::CastingType::kConcentration
					) ||
					(
						a_action == InputAction::kCastRH &&
						a_p->em->GetRHSpell() && 
						a_p->em->GetRHSpell()->GetCastingType() == RE::MagicSystem::CastingType::kConcentration
					) ||
					(
						a_action == InputAction::kQuickSlotCast && 
						a_p->em->quickSlotSpell && 
						a_p->em->quickSlotSpell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration
					)
				};

				// Per frame if a concentration spell.
				if (isConcSpell)
				{
					costToPerform *= *g_deltaTimeRealTime;
				}

				canPerform = a_p->isInGodMode || pam->currentMagicka - costToPerform > 0.0f;
			}
			else
			{
				cost = GetPAStaminaCost(a_p, a_action);
				// Check if the player has enough stamina if the cost is non-zero.
				if (cost != 0.0f) 
				{
					canPerform = 
					{
						(a_p->isInGodMode) ||
						(a_p->isPlayer1 && pam->currentStamina > 0.0f) ||
						(!a_p->isPlayer1 && pam->secsTotalStaminaRegenCooldown == 0.0f)
					};
				}
				else
				{
					// No cost, can perform.
					canPerform = true;
				}
			}

			// Cache cost so that it does not have to be recomputed later 
			// when expending health/magicka/stamina once the action's corresponding start animation triggers.
			pam->paStatesList[!a_action - !InputAction::kFirstAction].avCost = cost;
			return canPerform;
		}

		void EquipBoundWeapon(const std::shared_ptr<CoopPlayer>& a_p, RE::SpellItem* a_boundWeapSpell, RE::TESObjectWEAP* a_boundWeap, RE::BGSEquipSlot* a_slot, RE::MagicCaster* a_caster)
		{
			// Most cursed thing I've ever done, aside from this entire mod.
			// But nothing else works.
			// Console command equip funcs worked whereas the CLib ones did not for some reason.
			// Must also have a delay between executing console commands, otherwise the second command does not execute successfully.
			a_p->taskRunner->AddTask([a_p, a_boundWeap, a_boundWeapSpell, a_slot, a_caster]() \
			{
				const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
				const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
				auto aem = RE::ActorEquipManager::GetSingleton();
				if (script && aem)
				{
					// POTENTIAL THREADING ISSUE:
					// If the player attempts to equip another form while this code is executing,
					// the results can be unpredictable because the main thread and the task thread running this
					// code may conflict when modifying the desired equipped forms list.
					// Should not ever happen though since this code will not run if the player is already equipping/unequipping a form.

					// Equipped forms list indices where the LH and RH forms are equipped.
					std::vector<EquipIndex> lhEquipIndices{};
					std::vector<EquipIndex> rhEquipIndices{};
					auto lhForm = a_p->em->desiredEquippedForms[!EquipIndex::kLeftHand];
					auto rhForm = a_p->em->desiredEquippedForms[!EquipIndex::kRightHand];
					Util::AddSyncedTask([a_p, a_slot, lhForm, rhForm, &lhEquipIndices, &rhEquipIndices]() 
					{
						// Get equip indices.
						for (auto i = 0; i < a_p->em->desiredEquippedForms.size(); ++i)
						{
							auto form = a_p->em->desiredEquippedForms[i];
							if (lhForm && form == lhForm)
							{
								a_p->em->desiredEquippedForms[i] = nullptr;
								lhEquipIndices.emplace_back(static_cast<EquipIndex>(i));
							}

							if (rhForm && form == rhForm)
							{
								a_p->em->desiredEquippedForms[i] = nullptr;
								rhEquipIndices.emplace_back(static_cast<EquipIndex>(i));
							}
						}

						// Clear hands.
						a_p->em->UnequipHandForms(a_slot);
						// Set request flags and timers here after unequipping.
						// Also potentially a threading issue.
						if (a_slot == glob.bothHandsEquipSlot) 
						{
							a_p->pam->boundWeapReqLH = true;
							a_p->pam->boundWeapReqRH = true;
							a_p->lastBoundWeaponLHReqTP = a_p->lastBoundWeaponRHReqTP = SteadyClock::now();
						}
						else if (a_slot == glob.leftHandEquipSlot)
						{
							a_p->pam->boundWeapReqLH = true;
							a_p->lastBoundWeaponLHReqTP = SteadyClock::now();
						}
						else
						{
							a_p->pam->boundWeapReqRH = true;
							a_p->lastBoundWeaponRHReqTP = SteadyClock::now();
						}
					});

					std::string commandStr;
					bool isBow = a_boundWeap->IsBow();
					if (isBow)
					{
						// Equip bound bow directly with equip console command.
						// Spellcast-equip fails.
						Util::AddSyncedTask([&commandStr, &script, aem, a_boundWeap, a_p]() 
						{
							commandStr = fmt::format("equipitem {:X}", a_boundWeap->formID);
							script->SetCommand(commandStr.c_str());
							script->CompileAndRun(a_p->coopActor.get());
						});
					}
					else
					{
						// Otherwise, equip the bound weapon via spellcast.
						Util::AddSyncedTask([a_p, a_boundWeap, a_boundWeapSpell, a_slot, a_caster, aem]() 
						{
							a_caster->CastSpellImmediate(a_boundWeapSpell, false, a_p->coopActor.get(), 1.0f, false, 1.0f, a_p->coopActor.get());
						});
					}

					// Equip bound arrow by finding a bound arrow type that matches with the bound bow.
					if (isBow)
					{
						RE::FormID boundArrowFID = 0x10B0A7;
						// Get spell form based on casting source (either from hand or quick slot).
						RE::TESForm* boundBowForm = nullptr;
						if (a_caster == a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) 
						{
							boundBowForm = rhForm;
						}
						else if (a_caster == a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand))
						{
							boundBowForm = lhForm;
						}
						else
						{
							boundBowForm = a_p->em->desiredEquippedForms[!EquipIndex::kQuickSlotSpell];
						}

						if (auto boundBowSpell = boundBowForm ? boundBowForm->As<RE::SpellItem>() : nullptr; boundBowSpell && boundBowSpell->avEffectSetting && boundBowSpell->avEffectSetting->data.projectileBase) 
						{
							// Get the base projectile to match with the base projectile from one of the bound ammo choices.
							// Couldn't find a way to map from BGSProjectile to Projectile, 
							// so a linear (bleh) reverse of that mapping was required.
							auto baseProj = boundBowSpell->avEffectSetting->data.projectileBase;
							for (auto boundAmmo : glob.boundArrowAmmoList) 
							{
								if (boundAmmo->data.projectile == baseProj) 
								{
									boundArrowFID = boundAmmo->formID;
								}
							}
						}

						// Equip via console command.
						Util::AddSyncedTask([&commandStr, &script, aem, boundArrowFID, a_p]() 
						{
							commandStr = fmt::format("equipitem {:X}", boundArrowFID);
							script->SetCommand(commandStr.c_str());
							script->CompileAndRun(a_p->coopActor.get());
							a_p->pam->ReadyWeapon(true);
						});
					}

					// Wait until done equipping/unequipping before reducing magicka and restoring desired forms.
					bool isEquipping = true;
					bool isUnequipping = true;
					SteadyClock::time_point waitTP = SteadyClock::now();
					while (Util::GetElapsedSeconds(waitTP) < 2.0f && (isEquipping || isUnequipping))
					{
						std::this_thread::sleep_for(0.25s);
						a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
						a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
					}

					std::this_thread::sleep_for(0.5s);

					Util::AddSyncedTask([a_p, a_boundWeapSpell, lhForm, rhForm, &lhEquipIndices, &rhEquipIndices]() 
					{
						// Expend magicka.
						a_p->pam->ModifyAV(RE::ActorValue::kMagicka, -a_boundWeapSpell->CalculateMagickaCost(a_p->coopActor.get()));

						// Restore LH and RH forms to their previous indices.
						// Will be re-equipped by the equip manager's main task.
						for (const auto& equipIndex : lhEquipIndices)
						{
							a_p->em->desiredEquippedForms[!equipIndex] = lhForm;
						}

						for (const auto& equipIndex : rhEquipIndices)
						{
							a_p->em->desiredEquippedForms[!equipIndex] = rhForm;
						}
					});

					delete script;
				}
			});
		}

		void EquipHotkeyedForm(const std::shared_ptr<CoopPlayer>& a_p, RE::TESForm* a_hotkeyedForm, bool&& a_rightHand)
		{
			// Equip the previously selected hotkeyed form to the LH/RH equip
			// slot, if supported; otherwise, equip to the default corresponding slot,
			// or inform the player that the item is not equipable in the RH or default slots
			// (eg. shields/torches in the RH).

			// Refresh favorited forms, if necessary.
			// Refresh for P1 if the hotkey form is not favorited anymore.
			// Refresh for companion players if the form is physical and not favorited anymore.
			bool refreshFavorites = 
			{
				(a_hotkeyedForm) &&
				(
					(
						a_p->isPlayer1 &&
						!Util::IsFavorited(a_p->coopActor.get(), a_hotkeyedForm)
					) ||
					(
						!a_p->isPlayer1 &&
						a_hotkeyedForm->IsNot(RE::FormType::Spell, RE::FormType::Shout) &&
						!Util::IsFavorited(a_p->coopActor.get(), a_hotkeyedForm)
					)
				)
			};
			if (refreshFavorites) 
			{
				SPDLOG_DEBUG("[PAFH] {} is no longer favorited by {}. Refreshing all favorited forms data.",
					a_hotkeyedForm->GetName(),
					a_p->coopActor->GetName());

				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: {} is no longer favorited. Refreshing hotkeys.", a_p->playerID + 1, a_hotkeyedForm->GetName()),
					{
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kHotkeySelection, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
				);

				// Do not update magical favorites for companion player
				// because P1's magical favorites are active during regular gameplay.
				a_p->em->UpdateFavoritedFormsLists(!a_p->isPlayer1);
				return;
			}

			if (a_rightHand) 
			{
				if (a_hotkeyedForm)
				{
					// Equip the form in the appropriate slot.
					RE::BGSEquipSlot* slot = glob.rightHandEquipSlot;
					if (auto weap = a_hotkeyedForm->As<RE::TESObjectWEAP>(); weap)
					{
						if (weap->equipSlot == glob.bothHandsEquipSlot) 
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} in both hands", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
							slot = glob.bothHandsEquipSlot;
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} in the right hand", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState,
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
						}

						a_p->em->EquipForm(a_hotkeyedForm, EquipIndex::kRightHand, nullptr, 1, slot);
					}
					else if (auto spell = a_hotkeyedForm->As<RE::SpellItem>(); spell)
					{
						if (spell->equipSlot == glob.voiceEquipSlot) 
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} to the voice slot", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
							slot = glob.voiceEquipSlot;
						}
						else if (spell->equipSlot == glob.bothHandsEquipSlot)
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} in both hands", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection,
									CrosshairMessageType::kStealthState,
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
							slot = glob.bothHandsEquipSlot;
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} in the right hand", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
						}

						a_p->em->EquipSpell(a_hotkeyedForm, EquipIndex::kRightHand, slot);
					}
					else if (auto shout = a_hotkeyedForm->As<RE::TESShout>(); shout)
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Equipping {} to the voice slot", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);
						a_p->em->EquipShout(shout);
					}
					else if (a_hotkeyedForm->IsArmor())
					{
						// Cannot equip shield in the right hand.
						if (a_hotkeyedForm->As<RE::TESObjectARMO>()->IsShield())
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Cannot equip {} in the right hand", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
							return;
						}

						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Equipping {} to an armor slot", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);
						a_p->em->EquipArmor(a_hotkeyedForm);
					}
					else if (a_hotkeyedForm->As<RE::TESObjectLIGH>())
					{
						// Cannot equip torch in the right hand.
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Cannot equip {} in the right hand", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone,
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);
						return;
					}
					else if (auto magicItem = a_hotkeyedForm->As<RE::MagicItem>(); magicItem && magicItem->IsFood())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Consuming {}", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);
						a_p->em->EquipForm(a_hotkeyedForm, EquipIndex::kRightHand, nullptr, 1);
					}
					else if (a_hotkeyedForm->IsAmmo())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Equipping {} as ammo", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);
						a_p->em->EquipAmmo(a_hotkeyedForm);
					}
					else if (auto aem = RE::ActorEquipManager::GetSingleton(); aem && a_hotkeyedForm->As<RE::TESBoundObject>())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Equipping {}", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);

						aem->EquipObject(a_p->coopActor.get(), a_hotkeyedForm->As<RE::TESBoundObject>());
					}
				} 
				else if (auto rhForm = a_p->em->equippedForms[!EquipIndex::kRightHand]; rhForm)
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Unequipping {} in the right hand", a_p->playerID + 1, rhForm->GetName()),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
					);
					a_p->em->UnequipFormAtIndex(EquipIndex::kRightHand);
				}
			}
			else
			{
				if (a_hotkeyedForm)
				{
					// Equip the form in the appropriate slot.
					RE::BGSEquipSlot* slot = glob.leftHandEquipSlot;
					if (auto weap = a_hotkeyedForm->As<RE::TESObjectWEAP>(); weap)
					{
						if (weap->equipSlot == glob.bothHandsEquipSlot) 
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} in both hands", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
							slot = glob.bothHandsEquipSlot;
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} in the left hand", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
						}

						a_p->em->EquipForm(a_hotkeyedForm, EquipIndex::kLeftHand, nullptr, 1, slot);
					}
					else if (auto spell = a_hotkeyedForm->As<RE::SpellItem>(); spell)
					{
						if (spell->equipSlot == glob.voiceEquipSlot) 
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} to the voice slot", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection,
									CrosshairMessageType::kStealthState,
									CrosshairMessageType::kTargetSelection
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
							slot = glob.voiceEquipSlot;
						}
						else if (spell->equipSlot == glob.bothHandsEquipSlot)
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} in both hands", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone,
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
							slot = glob.bothHandsEquipSlot;
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} in the left hand", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
						}

						a_p->em->EquipSpell(a_hotkeyedForm, EquipIndex::kLeftHand, slot);
					}
					else if (auto shout = a_hotkeyedForm->As<RE::TESShout>(); shout)
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Equipping {} to the voice slot", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection,
								CrosshairMessageType::kStealthState,
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);
						a_p->em->EquipShout(shout);
					}
					else if (a_hotkeyedForm->IsArmor())
					{
						if (a_hotkeyedForm->As<RE::TESObjectARMO>()->IsShield())
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} in the left hand", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format("P{}: Equipping {} to an armor slot", a_p->playerID + 1, a_hotkeyedForm->GetName()),
								{
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection,
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
							);
						}

						a_p->em->EquipArmor(a_hotkeyedForm);
					}
					else if (a_hotkeyedForm->As<RE::TESObjectLIGH>())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Equipping {} in the left hand", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);
						a_p->em->EquipForm(a_hotkeyedForm, EquipIndex::kLeftHand, nullptr, 1);
					}
					else if (auto magicItem = a_hotkeyedForm->As<RE::MagicItem>(); magicItem && magicItem->IsFood())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Consuming {}", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection,
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);
						a_p->em->EquipForm(a_hotkeyedForm, EquipIndex::kLeftHand, nullptr, 1);
					}
					else if (a_hotkeyedForm->IsAmmo())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Equipping {} as ammo", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);
						a_p->em->EquipAmmo(a_hotkeyedForm);
					}
					else if (auto aem = RE::ActorEquipManager::GetSingleton(); aem && a_hotkeyedForm->As<RE::TESBoundObject>())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format("P{}: Equipping {}", a_p->playerID + 1, a_hotkeyedForm->GetName()),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
						);

						aem->EquipObject(a_p->coopActor.get(), a_hotkeyedForm->As<RE::TESBoundObject>());
					}
				}
				else if (auto lhForm = a_p->em->equippedForms[!EquipIndex::kLeftHand]; lhForm)
				{
					// Unequip the current hand form if no form was hotkeyed.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Unequipping {} in the left hand", a_p->playerID + 1, lhForm->GetName()),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState,
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f
					);
					a_p->em->UnequipFormAtIndex(EquipIndex::kLeftHand);
				}
			}
		}

		const float GetPAHealthCost(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Get health cost for reviving another player.
			float cost = 0.0f;
			switch (a_action)
			{
			case InputAction::kActivate:
			{
				// Reviver player will lose 50% of their base health if their Restoration level is at 15.
				// Health penalty will scale down to 10% at level 100 Restoration.
				auto resAV = a_p->coopActor->GetActorValue(RE::ActorValue::kRestoration);
				float resAVMult = std::lerp(0.5f, 0.1f, std::clamp((resAV - 15.0f) / (85.0f), 0.0f, 1.0f));
				cost = resAVMult * (a_p->coopActor->GetBaseActorValue(RE::ActorValue::kHealth) + a_p->coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth)) / max(1.0f, Settings::fSecsReviveTime);
			}
			default:
				break;
			}

			return cost;
		}

		const float GetPAMagickaCost(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Get magicka cost for spell cast action.
			float cost = 0.0f;
			switch (a_action)
			{
			case InputAction::kCastLH:
			{
				if (a_p->em->HasLHSpellEquipped())
				{
					auto lhSpell = a_p->em->GetLHSpell();
					cost = lhSpell->CalculateMagickaCost(a_p->coopActor.get());
				}

				break;
			}
			case InputAction::kCastRH:
			{
				if (a_p->em->HasRHSpellEquipped())
				{
					auto rhSpell = a_p->em->GetRHSpell();
					cost = rhSpell->CalculateMagickaCost(a_p->coopActor.get());
				}

				break;
			}
			case InputAction::kQuickSlotCast:
			{
				if (auto qsSpell = a_p->em->equippedForms[!EquipIndex::kQuickSlotSpell]; qsSpell && qsSpell->Is(RE::FormType::Spell))
				{
					auto asSpell = qsSpell->As<RE::SpellItem>();
					cost = asSpell->CalculateMagickaCost(a_p->coopActor.get());
				}

				break;
			}
			default:
				break;
			}

			return cost;
		}

		const float GetPAStaminaCost(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Get stamina cost for various movement actions.
			float cost = 0.0f;
			switch (a_action)
			{
			case InputAction::kDodge:
			{
				// Takes encumbrance into account. 
				// Dodging/rolling in heavy armor will consume more stamina.
				// The player will be able to dodge/roll at most once
				// with full stamina and worn weight equal to carryweight
				// and at most 10 times with full stamina and no worn weight.
				float carryWeightRatioMult = 
				(
					0.1f +
					min
					(
						0.9f, 
						sqrtf
						(
							a_p->coopActor->GetEquippedWeight() / a_p->coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight)
						)
					)
				);
				// Dash dodging costs more when the LS is displaced further from center (longer dodge).
				const float& lsMag = glob.cdh->GetAnalogStickState(a_p->controllerID, true).normMag;
				float dashDodgeCommitmentMult = 
				(
					(
						(lsMag == 0.0f) || 
						(!Settings::bUseDashDodgeSystem && ALYSLC::TKDodgeCompat::g_tkDodgeInstalled)
					) ? 
					1.0f : 
					lsMag
				);
				cost = a_p->pam->baseStamina * carryWeightRatioMult * dashDodgeCommitmentMult;
				break;
			}
			case InputAction::kBash:
			case InputAction::kPowerAttackDual:
			case InputAction::kPowerAttackLH:
			case InputAction::kPowerAttackRH:
			{
				// WORKAROUND NOTE: This non-zero cost is not used to reduce stamina, since the game already handles this.
				// Used instead to insert the action into the AV cost action queue for handling, 
				// which will then set the 'is bashing' or 'is power attacking' flag.
				cost = 0.1f;
				break;
			}
			case InputAction::kSprint:
			{
				if (a_p->coopActor->IsSneaking())
				{
					// Takes encumbrance into account.
					// Silent rolling in heavy armor will consume more stamina.
					// The player will be able to roll at most once
					// with full stamina and worn weight equal to carryweight
					// and at most 10 times with full stamina and no worn weight.
					float carryWeightRatio = a_p->em->GetWornWeight() / a_p->coopActor->GetActorValue(RE::ActorValue::kCarryWeight);
					cost = a_p->pam->baseStamina * (min(0.9f, sqrtf(carryWeightRatio)) + 0.1f);
				}
				else if (!a_p->mm->isParagliding && !a_p->tm->isMARFing)
				{
					// No cost for paragliding or M.A.R.F as of now.
					// https://en.uesp.net/wiki/Skyrim:Stamina
					// Cost for sprinting and shield charge is the same.
					cost = (7.0f + 0.02f * a_p->em->GetWornWeight());
				}

				break;
			}
			default:
				break;
			}

			return cost;
		}

		int32_t GetSelectedHotkeySlot(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Choose hotkey slot based on RS orientation.

			const auto& rsData = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
			// Must be moving RS.
			if (rsData.normMag == 0.0f)
			{
				return -1;
			}

			float realRSAng = atan2f(rsData.yComp, rsData.xComp);
			realRSAng = Util::ConvertAngle(Util::NormalizeAng0To2Pi(realRSAng));
			int32_t hotkeySlot = -1;
			if (realRSAng < PI / 8.0f || realRSAng > 15.0f * PI / 8.0f)
			{
				hotkeySlot = 0;
			}
			else if (realRSAng < 3.0f * PI / 8.0f)
			{
				hotkeySlot = 1;
			}
			else if (realRSAng < 5.0f * PI / 8.0f)
			{
				hotkeySlot = 2;
			}
			else if (realRSAng < 7.0f * PI / 8.0f)
			{
				hotkeySlot = 3;
			}
			else if (realRSAng < 9.0f * PI / 8.0f)
			{
				hotkeySlot = 4;
			}
			else if (realRSAng < 11.0f * PI / 8.0f)
			{
				hotkeySlot = 5;
			}
			else if (realRSAng < 13.0f * PI / 8.0f)
			{
				hotkeySlot = 6;
			}
			else
			{
				hotkeySlot = 7;
			}

			return hotkeySlot;
		}

		void HandleBoundWeaponEquip(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Casting bound weapons with a co-op companion is completely bugged and needs all the special handling it can get.
			// Additional info:
			// Bound bow equip via spellcast: 
			// Has a few glitches and weird equip slot issues once the spell spawning the bow is cast.
			// 1. The game tries to auto-equip gear, such as a shield in the LH, which unequips the bound bow
			// right as it spawns. Auto-equips are mostly blocked in the unequip object hook.
			// 2. The magic effect tied to the spell gets removed right after casting, so even without auto-equipping other
			// gear, the bound bow FX lingers in the LH, while the game treats the player's hands as empty, resulting in
			// the hilarious ability to punch with the bound bow "out". Bound arrows are also promptly unequipped at the same time.
			// 3. The bound FX lingers until another weapon is equipped in the LH.
			//
			// Other bound weapons: crash when unequipping/sheathing bound weapon and equipping a spell in the same
			// hand where the bound weapon was previously. Sometimes the bound weapon won't crash the game when unequipped from the RH
			// but it will always crash the game when unequipped from the LH.

			// Only equipped when the action starts.
			if (!HelperFuncs::ActionJustStarted(a_p, a_action))
			{
				return;
			}

			// If not triggered by a casting action, return early.
			bool notACastAction = 
			{
				(a_action != InputAction::kCastLH && a_action != InputAction::kCastRH && a_action != InputAction::kQuickSlotCast) &&
				((a_action != InputAction::kSpecialAction) || (a_p->pam->reqSpecialAction != SpecialActionType::kCastBothHands && a_p->pam->reqSpecialAction != SpecialActionType::kDualCast))
			};
			if (notACastAction)
			{
				return;
			}

			// Must not change gear while equipping the bound weapon.
			bool isEquipping = true;
			bool isUnequipping = true;
			a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			if (isEquipping || isUnequipping) 
			{
				return;
			}

			enum Source { kLeft, kRight, kQuick };
			auto source = Source::kRight;
			auto setupBoundWeaponEquip = [a_p](RE::SpellItem* a_spell, Source&& a_source) {
				if (a_spell)
				{
					if (auto assocWeap = Util::GetAssociatedBoundWeap(a_spell); assocWeap)
					{
						// If the spell has base effects, set the bound weapon duration to the duration of first base effect.
						// Otherwise, default to 120 seconds.
						RE::BGSEquipSlot* slot = nullptr;
						bool hasEffect = !a_spell->effects.empty();
						if (assocWeap->equipSlot == glob.bothHandsEquipSlot)
						{
							if (hasEffect)
							{
								a_p->pam->secsBoundWeaponLHDuration = a_p->pam->secsBoundWeaponRHDuration = a_spell->effects[0]->GetDuration();
							}
							else
							{
								a_p->pam->secsBoundWeaponLHDuration = a_p->pam->secsBoundWeaponRHDuration = 120.0f;
							}

							slot = glob.bothHandsEquipSlot;
						}
						else if (a_source == Source::kLeft)
						{
							if (hasEffect)
							{
								a_p->pam->secsBoundWeaponLHDuration = a_spell->effects[0]->GetDuration();
							}
							else
							{
								a_p->pam->secsBoundWeaponLHDuration = 120.0f;
							}

							slot = glob.leftHandEquipSlot;
						}
						else
						{
							if (hasEffect)
							{
								a_p->pam->secsBoundWeaponRHDuration = a_spell->effects[0]->GetDuration();
							}
							else
							{
								a_p->pam->secsBoundWeaponRHDuration = 120.0f;
							}

							slot = glob.rightHandEquipSlot;
						}

						// Get magic caster in the same slot and attempt to equip the bound weapon.
						auto caster = a_source == Source::kLeft ? a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand) :
									  a_source == Source::kRight ? a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand) :
									  a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
						if (slot && caster)
						{
							HelperFuncs::EquipBoundWeapon(a_p, a_spell, assocWeap, slot, caster);
						}
					}
				}
			};

			// NOTE: Must use copied spell since something seems to be missing after copying
			// a bound spell to a placeholder spell and then casting the placeholder spell.
			// Game will crash once something else is equipped to the bound weapon's hand.
			if (a_action == InputAction::kQuickSlotCast) 
			{
				// Equip voice slot spells directly, not a placeholder spell. 
				setupBoundWeaponEquip(a_p->em->quickSlotSpell, Source::kQuick);
			}
			else if (a_action == InputAction::kCastLH || a_action == InputAction::kCastRH)
			{
				// Cast copied spell in LH/RH.
				auto isLeftHand = a_action == InputAction::kCastLH;
				auto copiedSpellForm = a_p->em->copiedMagic[isLeftHand ? !PlaceholderMagicIndex::kLH : !PlaceholderMagicIndex::kRH];
				auto copiedSpell = copiedSpellForm->As<RE::SpellItem>();
				setupBoundWeaponEquip(copiedSpell, isLeftHand ? Source::kLeft : Source::kRight);
			}
			else
			{
				// Cast copied spell with both hands.
				auto copiedSpellLHForm = a_p->em->copiedMagic[!PlaceholderMagicIndex::kLH];
				auto copiedSpellRHForm = a_p->em->copiedMagic[!PlaceholderMagicIndex::kRH];
				auto copiedSpell = copiedSpellLHForm->As<RE::SpellItem>();
				setupBoundWeaponEquip(copiedSpell, Source::kLeft);
				copiedSpell = copiedSpellRHForm->As<RE::SpellItem>();
				setupBoundWeaponEquip(copiedSpell, Source::kRight);
			}
		}

		bool InitiateSpecialInteractionPackage(const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFR* a_interactionRefr, RE::TESBoundObject* a_interactionBaseObj)
		{
			// If the targeted interaction refr is furniture, set up the special interaction package and entry pos.
			// Return true if successful.
			// Also output the requested sit/sleep state as an out param.
			
			// Get furniture entry point to serve as interaction package entry position.
			bool asFurniture = a_interactionRefr->As<RE::TESFurniture>() || a_interactionBaseObj->As<RE::TESFurniture>();
			bool asIdleMarker = a_interactionRefr->As<RE::BGSIdleMarker>() || a_interactionBaseObj->As<RE::BGSIdleMarker>();
			if (asFurniture || asIdleMarker)
			{
				// Set initial interaction entry position to the object's position.
				// Will pick closest entry position to set below.
				a_p->mm->interactionPackageEntryPos = a_interactionRefr->data.location;
				if (asFurniture)
				{
					if (auto refr3D = Util::GetRefr3D(a_interactionRefr); refr3D)
					{
						RE::BSFurnitureMarkerNode* markerNode = refr3D->GetExtraData<RE::BSFurnitureMarkerNode>("FRN");
						if (markerNode)
						{
							std::optional<RE::NiPoint3> closestEntryPoint = std::nullopt;
							for (uint32_t i = 0; i < markerNode->markers.size(); ++i)
							{
								// NOTE: I know the offset math is not quite correct,
								// so this is an approximation for now until I have the time to fix it.
								const auto& marker = markerNode->markers[i];
								float localPitch, localYaw = 0.0f;
								if (fabsf(marker.offset.z) > 1e-5f)
								{
									localPitch = Util::DirectionToGameAngPitch(marker.offset);
								}

								if (fabsf(marker.offset.x) > 1e-5f || fabsf(marker.offset.y) > 1e-5f)
								{
									localYaw = Util::DirectionToGameAngYaw(marker.offset);
								}

								float worldPitch = -a_interactionRefr->data.angle.x - localPitch;
								float worldYaw = Util::ConvertAngle(Util::NormalizeAng0To2Pi(localYaw + a_interactionRefr->data.angle.z));
								auto entryPointOffset = Util::RotationToDirectionVect(worldPitch, worldYaw) * marker.offset.Length();
								auto entryPoint = refr3D->world.translate + entryPointOffset;
								// Set new closest entry point if there was no entry point previously set or this one is closer.
								if (!closestEntryPoint.has_value() ||
									entryPoint.GetDistance(a_p->coopActor->data.location) < closestEntryPoint.value().GetDistance(a_p->coopActor->data.location))
								{
									closestEntryPoint = entryPoint;
								}
							}

							// Set to refr's world pos if there's no closest entry point.
							if (!closestEntryPoint.has_value())
							{
								closestEntryPoint = refr3D->world.translate;
							}

							// Set interaction entry pos.
							a_p->mm->interactionPackageEntryPos = closestEntryPoint.value();
						}
					}
				}

				// Sheathe weapon and prep interaction package, and then evaluate it to initiate furniture use.
				a_p->pam->ReadyWeapon(false);
				a_p->coopActor->extraList.SetLinkedRef(a_interactionRefr, a_p->aimTargetKeyword);
				a_p->tm->aimTargetLinkedRefrHandle = a_p->tm->activationRefrHandle;
				auto interactionPackage = glob.coopPackages[!PackageIndex::kTotal * a_p->controllerID + !PackageIndex::kSpecialInteraction];
				a_p->pam->SetCurrentPackage(interactionPackage);
				a_p->pam->EvaluatePackage();
				// Turn to face when first activated.
				a_p->coopActor->SetHeading(Util::GetYawBetweenPositions(a_p->mm->interactionPackageEntryPos, a_p->coopActor->data.location));
				// Successfully initiated interaction.
				return true;
			}

			return false;
		}

		void LootAllItemsFromContainer(const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFRPtr a_containerPtr)
		{
			// Move all lootable items from the given container to the requesting player.

			// Only loot if the container is available, not locked, and has a base type of TESContainer.
			if (!a_containerPtr || !a_containerPtr.get() || a_containerPtr->IsLocked() || !a_containerPtr->HasContainer()) 
			{
				return;
			}

			auto p1 = RE::PlayerCharacter::GetSingleton();
			auto container = a_containerPtr->GetContainer();
			// Check base container first.
			container->ForEachContainerObject(
				[&a_p, &a_containerPtr, p1](RE::ContainerObject& a_object) {
					if (a_object.obj && Util::IsLootableObject(*a_object.obj)) 
					{
						a_containerPtr->RemoveItem(a_object.obj, a_object.count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, a_p->coopActor.get());
						// Show in TrueHUD recent loot widget by adding and removing the object from P1.
						if (p1 && ALYSLC::TrueHUDCompat::g_trueHUDInstalled && !a_p->isPlayer1) 
						{
							p1->AddObjectToContainer(a_object.obj, nullptr, a_object.count, a_p->coopActor.get());
							p1->RemoveItem(a_object.obj, a_object.count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
						}
					}

					return RE::BSContainer::ForEachResult::kContinue;
				}
			);

			// Check inventory next.
			auto inventory = a_containerPtr && a_containerPtr.get() ? a_containerPtr->GetInventory() : RE::TESObjectREFR::InventoryItemMap();
			if (!inventory.empty())
			{
				for (auto& [boundObj, countInvEntryDataPair] : inventory)
				{
					if (boundObj && countInvEntryDataPair.first > 0 && Util::IsLootableObject(*boundObj))
					{
						a_containerPtr->RemoveItem(boundObj, countInvEntryDataPair.first, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, a_p->coopActor.get());
						// Show in TrueHUD recent loot widget by adding and removing the object from P1.
						if (p1 && ALYSLC::TrueHUDCompat::g_trueHUDInstalled && !a_p->isPlayer1)
						{
							p1->AddObjectToContainer(boundObj, nullptr, countInvEntryDataPair.first, a_p->coopActor.get());
							p1->RemoveItem(boundObj, countInvEntryDataPair.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
						}
					}
				}
			}
		}

		void LootAllItemsFromCorpse(const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFRPtr a_corpseRefrPtr)
		{
			// Move all lootable items from the corpse to the requesting player.

			// Only loot if the corpse is available.
			if (!a_corpseRefrPtr || !a_corpseRefrPtr->As<RE::Actor>() || !a_corpseRefrPtr->As<RE::Actor>()->IsDead())
			{
				return;
			}

			auto asActor = a_corpseRefrPtr->As<RE::Actor>();
			auto p1 = RE::PlayerCharacter::GetSingleton();
			// Check inventory first.
			auto inventory = asActor->GetInventory();
			if (!inventory.empty())
			{
				for (auto& [boundObj, countInvEntryDataPair] : inventory)
				{
					if (boundObj && countInvEntryDataPair.first > 0 && Util::IsLootableObject(*boundObj))
					{
						asActor->RemoveItem(boundObj, countInvEntryDataPair.first, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, a_p->coopActor.get());
						// Show in TrueHUD recent loot widget by adding and removing the object from P1.
						if (p1 && ALYSLC::TrueHUDCompat::g_trueHUDInstalled && !a_p->isPlayer1)
						{
							p1->AddObjectToContainer(boundObj, nullptr, countInvEntryDataPair.first, a_p->coopActor.get());
							p1->RemoveItem(boundObj, countInvEntryDataPair.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
						}
					}
				}
			}

			// Check dropped inventory next.
			auto droppedInventory = asActor->GetDroppedInventory();
			if (!droppedInventory.empty())
			{
				for (const auto& [boundObj, countHandlePair] : droppedInventory)
				{
					if (boundObj && countHandlePair.first > 0 && Util::IsLootableObject(*boundObj))
					{
						for (const auto& refrHandle : countHandlePair.second)
						{
							if (auto refrPtr = Util::GetRefrPtrFromHandle(refrHandle); refrPtr)
							{
								// Loot loose refr.
								LootRefr(a_p, refrPtr);
							}
						}
					}
				}
			}
		}

		void LootRefr(const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFRPtr a_refrPtr)
		{
			// Loot a single refr or aggregation of the same refr.

			// Must be valid.
			if (!a_refrPtr || !a_refrPtr.get())
			{
				return;
			}

			// Get count.
			uint32_t count = 1;
			auto extraCount = a_refrPtr->extraList.GetByType<RE::ExtraCount>();
			if (extraCount)
			{
				count = extraCount->count;
			}

			// Get base object to obtain this refr's form type.
			auto baseObj = a_refrPtr->GetBaseObject();
			// Pickup instead of activating books and notes to prevent the BookMenu from opening.
			bool shouldPickup = false;
			if ((baseObj) && (baseObj->IsBook() || baseObj->IsNote())) 
			{
				shouldPickup = true;
			}

			if (a_p->isPlayer1)
			{
				// Activate or pickup right away with P1.
				if (shouldPickup) 
				{
					a_p->coopActor->PickUpObject(a_refrPtr.get(), count);
				}
				else
				{
					Util::ActivateRef(a_refrPtr.get(), a_p->coopActor.get(), 0, a_refrPtr.get()->GetBaseObject(), count, false);
				}
			}
			else
			{
				auto p1 = RE::PlayerCharacter::GetSingleton();
				bool shouldLootWithP1 = false;
				if (baseObj)
				{
					// Has extra data with an associated quest.
					// Might not necessarily be a quest item, but better safe than sorry.
					bool isQuestItem = 
					{
						a_refrPtr->extraList.HasType(RE::ExtraDataType::kAliasInstanceArray) ||
						a_refrPtr->extraList.HasType(RE::ExtraDataType::kFromAlias) /*||
						a_refrPtr->extraList.HasType(RE::ExtraDataType::kTextDisplayData)*/
					};
					// Move shared items to P1 for ease of access, especially if they are quest items.
					shouldLootWithP1 = isQuestItem || Util::IsPartyWideItem(baseObj);
					// Show in TrueHUD recent loot widget if P1 is not looting the item themselves.
					if (p1 && ALYSLC::TrueHUDCompat::g_trueHUDInstalled && !shouldLootWithP1)
					{
						p1->AddObjectToContainer(baseObj, nullptr, count, p1);
						p1->RemoveItem(baseObj, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					}
				}

				if (p1 && shouldLootWithP1)
				{
					// Have P1 loot the item by picking it up or through activation.
					if (shouldPickup) 
					{
						p1->PickUpObject(a_refrPtr.get(), count);
					}
					else
					{
						Util::ActivateRef(a_refrPtr.get(), p1, 0, a_refrPtr.get()->GetBaseObject(), count, false);
					}
				}
				else
				{
					// Have the player themselves loot the item by picking it up or through activation.
					if (shouldPickup)
					{
						a_p->coopActor->PickUpObject(a_refrPtr.get(), count);
					}
					else
					{
						Util::ActivateRef(a_refrPtr.get(), a_p->coopActor.get(), 0, a_refrPtr.get()->GetBaseObject(), count, false);
					}
				}
			}
		}

		void OpenMenuWithKeyboard(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Send an emulated keyboard input to open the menu associated with the given action.

			if (glob.paInfoHolder->DEF_ACTION_INDICES_TO_GROUPS.at(!a_action) == ActionGroup::kMenu)
			{
				// Attempt to obtain menu control before opening menu.
				bool succ = false;
				if (a_action == InputAction::kMagicMenu)
				{
					succ = glob.moarm->InsertRequest(a_p->controllerID, a_action, SteadyClock::now(), RE::MagicMenu::MENU_NAME);
				}
				else if (a_action == InputAction::kMapMenu)
				{
					succ = glob.moarm->InsertRequest(a_p->controllerID, a_action, SteadyClock::now(), RE::MapMenu::MENU_NAME);
				}
				else if (a_action == InputAction::kPause)
				{
					succ = glob.moarm->InsertRequest(a_p->controllerID, a_action, SteadyClock::now(), RE::JournalMenu::MENU_NAME);
				}
				else if (a_action == InputAction::kStatsMenu)
				{
					succ = glob.moarm->InsertRequest(a_p->controllerID, a_action, SteadyClock::now(), RE::StatsMenu::MENU_NAME);
				}
				else if (a_action == InputAction::kTweenMenu)
				{
					succ = glob.moarm->InsertRequest(a_p->controllerID, a_action, SteadyClock::now(), RE::TweenMenu::MENU_NAME);
				}
				else if (a_action == InputAction::kWaitMenu)
				{
					succ = glob.moarm->InsertRequest(a_p->controllerID, a_action, SteadyClock::now(), RE::SleepWaitMenu::MENU_NAME);
				}

				// If request was successfully inserted, open the requested menu.
				if (succ) 
				{
					a_p->pam->SendButtonEvent(a_action, RE::INPUT_DEVICE::kKeyboard, ButtonEventPressType::kInstantTrigger, 0.0f, true);
				}
			}
		}

		void PerformActivationCycling(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Check for downed players to revive before cycling through nearby refrs, 
			// highlighting each once within activation distance.

			const auto& pam = a_p->pam;
			const bool justStarted = HelperFuncs::ActionJustStarted(a_p, InputAction::kActivate);
			if (justStarted)
			{
				// Clear previous proximity refr and downed player target on action start.
				// Will check for a new proximity refr and nearby downed players to revive below.
				a_p->tm->ClearProximityRefr();
				if (pam->downedPlayerTarget) 
				{
					pam->downedPlayerTarget->isBeingRevived = false;
					pam->downedPlayerTarget = nullptr;
					a_p->isRevivingPlayer = false;
				}
			}

			if (!pam->downedPlayerTarget) 
			{
				const float& secsSinceActivationStarted = pam->paStatesList[!InputAction::kActivate - !InputAction::kFirstAction].secsPerformed;
				auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
				bool crosshairRefrValidity = crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get());
				// Choose the valid crosshair refr for activation if the action press time is within the initial window before cycling,
				// or if there is no valid crosshair refr.
				// Otherwise, look for a nearby refr to activate.
				const bool useProximityInteraction = secsSinceActivationStarted >= Settings::fSecsBeforeActivationCycling || !crosshairRefrValidity;
				a_p->tm->useProximityInteraction = useProximityInteraction;
				if (justStarted)
				{
					a_p->lastActivationCheckTP = SteadyClock::now();
					// Remove activation effect shader on previous activation refr, if any.
					if (auto activationRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle); activationRefrPtr && Util::IsValidRefrForTargeting(activationRefrPtr.get()))
					{
						Util::StopEffectShader(activationRefrPtr.get(), glob.activateHighlightShader);
					}

					// Get next object to activate after setting proximity interaction flag.
					auto targetedRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->GetNextObjectToActivate());
					if (targetedRefrPtr && Util::IsValidRefrForTargeting(targetedRefrPtr.get()))
					{
						// Highlight targeted refr.
						Util::StartEffectShader(targetedRefrPtr.get(), glob.activateHighlightShader, max(0.1f, useProximityInteraction ? Settings::fSecsBetweenActivationChecks : Settings::fSecsBeforeActivationCycling));
					}
				}
				else if (secsSinceActivationStarted >= Settings::fSecsBeforeActivationCycling)
				{
					// Cycling begins now, so remove activation effect shader on crosshair refr, if any.
					if (crosshairRefrValidity)
					{
						Util::StopEffectShader(crosshairRefrPtr.get(), glob.activateHighlightShader);
					}
				}

				// Cycle through nearby objects at regular intervals while the activate bind is held.
				if (a_p->tm->useProximityInteraction)
				{
					pam->secsSinceLastActivationCyclingCheck = Util::GetElapsedSeconds(a_p->lastActivationCheckTP);
					if (pam->secsSinceLastActivationCyclingCheck > max(0.1f, Settings::fSecsBetweenActivationChecks))
					{
						a_p->lastActivationCheckTP = SteadyClock::now();
						auto refrToActivatePtr = Util::GetRefrPtrFromHandle(a_p->tm->GetNextObjectToActivate());
						if (refrToActivatePtr && Util::IsValidRefrForTargeting(refrToActivatePtr.get()))
						{
							// Play highlight shader on cycled activation refr.
							Util::StartEffectShader(refrToActivatePtr.get(), glob.activateHighlightShader, max(0.1f, Settings::fSecsBetweenActivationChecks));
						}
					}
				}

				// Check if the crosshair-targeted/cycled refr is a downed player now.
				// Only a valid action if the revive system is enabled.
				// Also cannot initiate revive if ragdolling or staggered, so exit here.
				if (!Settings::bUseReviveSystem || a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal)
				{
					return;
				}

				auto targetRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle);
				auto targetRefrValidity = targetRefrPtr && Util::IsValidRefrForTargeting(targetRefrPtr.get());
				// No valid target or too far away to activate.
				// NOTE: Range check only done here on the initial check.
				if (!targetRefrValidity || !a_p->tm->RefrIsInActivationRange(a_p->tm->activationRefrHandle))
				{
					return;
				}

				// Find downed, unrevived player.
				auto downedPlayerIter = std::find_if(glob.coopPlayers.begin(), glob.coopPlayers.end(),
					[&targetRefrPtr](const auto& p) {
						return p->isActive && p->coopActor == targetRefrPtr && p->isDowned && !p->isRevived;
					});

				// Is a player.
				if (downedPlayerIter != glob.coopPlayers.end())
				{
					// Check if the player has enough health to revive a downed player.
					// If not, clear out revive data and return.
					if (!HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kActivate))
					{
						pam->downedPlayerTarget = (*downedPlayerIter);
						pam->downedPlayerTarget->isBeingRevived = false;
						a_p->isRevivingPlayer = false;
						a_p->lastReviveCheckTP = SteadyClock::now();
						return;
					}

					// Player can revive the downed player target.
					pam->downedPlayerTarget = (*downedPlayerIter);
					pam->downedPlayerTarget->isBeingRevived = true;
					a_p->isRevivingPlayer = true;
					a_p->lastReviveCheckTP = SteadyClock::now();
				}
				else
				{
					// No found player, nothing else to do.
					return;
				}

				// NOTE: Only will reach here if there is a revivable player target.
				// Play revive animation if grounded.
				if (!a_p->coopActor->IsOnMount() && !a_p->coopActor->IsSwimming() && !a_p->coopActor->IsFlying())
				{
					// Sheathe weapon first.
					a_p->coopActor->NotifyAnimationGraph("IdleForceDefaultState");
					a_p->coopActor->NotifyAnimationGraph("IdleKneeling");
				}

				// Play shaders and hit effects.
				const auto& downedPlayerTarget = (*downedPlayerIter);
				Util::StartEffectShader(a_p->coopActor.get(), glob.dragonHolesShader, -1.0f);
				Util::StartEffectShader(downedPlayerTarget->coopActor.get(), glob.dragonSoulAbsorbShader, -1.0f);
				Util::StartHitArt(downedPlayerTarget->coopActor.get(), glob.reviveDragonSoulEffect, downedPlayerTarget->coopActor.get(), -1.0f, false, true);
				Util::StartHitArt(downedPlayerTarget->coopActor.get(), glob.reviveHealingEffect, downedPlayerTarget->coopActor.get(), -1.0f, false, false);
			}
			else
			{
				// Continue reviving the downed player if not fully revived.
				// Clear the targeted downed player otherwise.
				if (!pam->downedPlayerTarget->isRevived) 
				{
					// Rotate to face downed player's torso.
					float yawToTarget = Util::GetYawBetweenPositions(a_p->coopActor->data.location, Util::GetTorsoPosition(pam->downedPlayerTarget->coopActor.get()));
					float angDiff = Util::NormalizeAngToPi(yawToTarget - a_p->coopActor->data.angle.z);
					a_p->coopActor->SetHeading(Util::NormalizeAng0To2Pi(a_p->coopActor->data.angle.z + angDiff));
					// Will set the is revived flag to true once the downed player is fully revived.
					pam->RevivePlayer();
				}
			}
		}

		void PlayEmoteIdle(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Play idle animation corresponding to current cycled emote if not already requested. Otherwise, stop the idle.

			if (!a_p->pam->isPlayingEmoteIdle && !a_p->em->currentCycledIdleIndexPair.first.empty())
			{
				// Cycled idle available.
				a_p->coopActor->NotifyAnimationGraph(a_p->em->currentCycledIdleIndexPair.first);
				a_p->pam->isPlayingEmoteIdle = true;
			}
			else if (!a_p->coopActor->IsOnMount() && !a_p->coopActor->IsSwimming() && !a_p->coopActor->IsFlying())
			{
				a_p->coopActor->NotifyAnimationGraph("IdleStop");
				a_p->coopActor->NotifyAnimationGraph("IdleForceDefaultState");
				a_p->pam->isPlayingEmoteIdle = false;
			}
		}

		void RemoveCastingPackage(const std::shared_ptr<CoopPlayer>& a_p, bool&& a_lhCast, bool&& a_rhCast)
		{
			// Stop companion player spell cast in the given hand(s).

			auto& pam = a_p->pam;
			// Reset casting global variables and caster durations.
			if (a_lhCast && a_rhCast) 
			{
				pam->castingGlobVars[!CastingGlobIndex::k2H]->value = 0.0f;
				pam->castingGlobVars[!CastingGlobIndex::kDual]->value = 0.0f;
				pam->castingGlobVars[!CastingGlobIndex::kLH]->value = 0.0f;
				pam->castingGlobVars[!CastingGlobIndex::kRH]->value = 0.0f;
				pam->lhCastDuration = 0.0f;
				pam->rhCastDuration = 0.0f;
			}
			else if (a_lhCast) 
			{
				pam->castingGlobVars[!CastingGlobIndex::kLH]->value = 0.0f;
				pam->lhCastDuration = 0.0f;
			}
			else if (a_rhCast)
			{
				pam->castingGlobVars[!CastingGlobIndex::kRH]->value = 0.0f;
				pam->rhCastDuration = 0.0f;
			}
			else
			{
				pam->castingGlobVars[!CastingGlobIndex::kShout]->value = 0.0f;
				pam->castingGlobVars[!CastingGlobIndex::kVoice]->value = 0.0f;
			}

			if (pam->castingGlobVars[!CastingGlobIndex::k2H]->value == 0.0f && 
				pam->castingGlobVars[!CastingGlobIndex::kLH]->value == 0.0f &&
				pam->castingGlobVars[!CastingGlobIndex::kRH]->value == 0.0f) 
			{
				// Reset to default package as well if both hands are no longer casting.
				pam->castingGlobVars[!CastingGlobIndex::kDual]->value = 0.0f;
				pam->ResetPackageCastingState();
			}
			else
			{
				// Re-evaluate the current package to allow the above changes 
				// to casting globals to take effect.
				a_p->pam->SetCurrentPackage(a_p->pam->GetDefaultPackage());
				a_p->pam->EvaluatePackage();
			}

			// Try to re-obtain magic casters if either is invalid.
			if (!pam->lhCaster || !pam->rhCaster)
			{
				pam->lhCaster = a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
				pam->rhCaster = a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
			}

			// Clear out casters' current spells once done casting to ensure the cast stops
			// if package evaluation failed.
			if (pam->lhCaster && pam->castingGlobVars[!CastingGlobIndex::kLH]->value == 0.0f)
			{
				pam->lhCaster->currentSpell = nullptr;
			}

			if (pam->rhCaster && pam->castingGlobVars[!CastingGlobIndex::kRH]->value == 0.0f)
			{
				pam->rhCaster->currentSpell = nullptr;
			}

			// Clear out linked refr target used by the casting package.
			a_p->tm->ClearTarget(TargetActorType::kLinkedRefr);
		}

		bool PlayKillmoveFromList(const std::shared_ptr<CoopPlayer>& a_p, RE::Actor* a_targetActor, const bool& a_isOtherPlayer, const bool& a_hasCharacterSkeleton, const std::vector<RE::TESIdleForm*>& a_killmoveIdlesList)
		{
			// Attempt to trigger each killmove in the given list.
			// Return true if a killmove was triggered successfully.

			bool performedKillmove = false;
			if (!a_killmoveIdlesList.empty())
			{
				float yawToTarget = Util::GetYawBetweenPositions(a_p->coopActor->data.location, a_targetActor->data.location);
				float facingAngDiff = fabsf(Util::NormalizeAngToPi(a_targetActor->GetHeading(false) - yawToTarget));

				// Have player face the target first.
				a_p->coopActor->SetHeading(yawToTarget);
				// Stop either actor from moving.
				Util::NativeFunctions::SetDontMove(a_targetActor, true);
				a_targetActor->StopMoving(*g_deltaTimeRealTime);
				a_targetActor->NotifyAnimationGraph("moveStop");
				Util::SetLinearVelocity(a_targetActor, RE::NiPoint3());
				Util::NativeFunctions::SetDontMove(a_p->coopActor.get(), true);
				a_p->coopActor->StopMoving(*g_deltaTimeRealTime);
				a_p->coopActor->NotifyAnimationGraph("moveStop");
				Util::SetLinearVelocity(a_p->coopActor.get(), RE::NiPoint3());

				// Allow both actors to move again post-killmove check.
				Util::NativeFunctions::SetDontMove(a_targetActor, false);
				Util::NativeFunctions::SetDontMove(a_p->coopActor.get(), false);

				// For positional validity checks.
				bool isBackstab = false;
				bool isDecap = false;
				bool facingCorrectDir = false;
				bool canPerform = false;

				// Seed random number generator and pick initial killmove index.
				std::mt19937 generator;
				generator.seed(SteadyClock::now().time_since_epoch().count());
				float rand = std::clamp(generator() / (float)((std::mt19937::max)()), 0.0f, 0.99f);

				// Keep track of how many killmoves were attempted while looping.
				uint8_t killmovesAttempted = 0;
				uint32_t killmoveIndex = (uint32_t)(a_killmoveIdlesList.size() * rand);
				RE::TESIdleForm* killmoveIdle = nullptr;
				// Loop until a killmove triggers or all killmoves are attempted.
				while (!performedKillmove && killmovesAttempted < a_killmoveIdlesList.size())
				{
					killmoveIdle = a_killmoveIdlesList[killmoveIndex];
					if (killmoveIdle) 
					{
						// Don't allow decapitation of other players if the revive system is in use.
						// Because, well, once revived, something will still be missing up top.
						isBackstab = glob.BACKSTAB_KILLMOVES_HASHES_SET.contains(Hash(killmoveIdle->formEditorID));
						isDecap = glob.DECAP_KILLMOVES_HASHES_SET.contains(Hash(killmoveIdle->formEditorID));
						facingCorrectDir = (isBackstab && facingAngDiff < PI / 2.0f) || (!isBackstab && facingAngDiff >= PI / 2.0f);
						canPerform = facingCorrectDir && (!isDecap || !a_isOtherPlayer || !Settings::bUseReviveSystem);
					}
					else
					{
						canPerform = false;
					}

					// Can attempt to trigger the killmove.
					if (canPerform)
					{
						// Choose front position killmoves if the target is not facing away from the player.
						// Choose back position killmoves otherwise.
						if (facingAngDiff >= PI / 2.0f)
						{
							// Face the player.
							a_targetActor->SetHeading(Util::NormalizeAngTo2Pi(yawToTarget + PI));
						}
						else
						{
							// Face 180 degrees away from the player.
							a_targetActor->SetHeading(yawToTarget);
						}

						performedKillmove = Util::PlayIdle(killmoveIdle, a_p->coopActor.get(), a_targetActor) && a_p->coopActor->IsInKillMove();
						// Update killmove index and num performed for the next iteration.
						killmoveIndex = killmoveIndex + 1 == a_killmoveIdlesList.size() ? 0 : killmoveIndex + 1;
						++killmovesAttempted;

						// Have to delay killmove check, as some killmoves, such as 'pa_KillMoveDLC02RipHeartOut' do not start right away.
						// Update delayed killmove request info for this player's player action manager to handle later.
						a_p->pam->killmoveTargetActorHandle = a_targetActor->GetHandle();
						a_p->lastKillmoveCheckTP = SteadyClock::now();
					}
					else
					{
						// Move on.
						killmoveIndex = killmoveIndex + 1 == a_killmoveIdlesList.size() ? 0 : killmoveIndex + 1;
						++killmovesAttempted;
					}
				}

				// Generic killmove fails often when targeting actors with a skeleton other than the character skeleton.
				// The killmove target may remain alive at 0 HP and gain temporary invulnerability for a bit
				// until the kill request is carried out.
				// Force kill and ragdoll here.
				// The player actor will also, more often than not, warp around during the killmove animation.
				if (Settings::bUseGenericKillmovesOnUnsupportedNPCs && !a_hasCharacterSkeleton && performedKillmove && 
					a_targetActor && !a_targetActor->IsInKillMove() && a_targetActor->currentProcess && a_targetActor->currentProcess->middleHigh)
				{
					a_targetActor->currentProcess->deathTime = 1.0f;
					a_targetActor->currentProcess->middleHigh->inDeferredKill = false;
					a_targetActor->KillImpl(a_p->coopActor.get(), FLT_MAX, true, true);
					a_targetActor->KillImmediate();
					a_targetActor->NotifyAnimationGraph("Ragdoll");
				}
			}

			return performedKillmove;
		}

		void PlayPowerAttackAnimation(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Play power attack animation based on the given player action.
			// Either triggered via action command or animation idle.
			// Both handle stamina expenditure automatically.
			// Any animation event-triggered attacks require manual stamina expenditure 
			// through the player's AV cost manager.

			auto& pam = a_p->pam;
			auto& em = a_p->em;
			bool isOnMount = a_p->coopActor->IsOnMount();
			bool isUnarmed = em->IsUnarmed();

			// NOTE: Have to start attacking with the LH/RH first
			// to trigger directional LH power attacks/some RH power attacks.
			// Also, LH/RH action command-triggered werewolf power attacks do not work for non-P1 players.
			// Have to play idles instead.
			if (a_action == InputAction::kPowerAttackDual) 
			{
				if (Util::IsWerewolf(a_p->coopActor.get()))
				{
					if (a_p->mm->lsMoved)
					{
						Util::PlayIdle("AttackStartDualSprinting", a_p->coopActor.get());
					}
					else
					{
						Util::PlayIdle("AttackStartDualBackHand", a_p->coopActor.get());
					}
				}
				else
				{
					// Not selected running the dual power attack action command, so perform via idle.
					if (a_p->em->IsUnarmed())
					{
						Util::PlayIdle("RightH2HComboPowerAttack", a_p->coopActor.get());
					}
					else
					{
						Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionDualPowerAttack, a_p->coopActor.get());
					}
				}
			}
			else if (a_action == InputAction::kPowerAttackLH)
			{
				if (!a_p->isPlayer1 && Util::IsWerewolf(a_p->coopActor.get())) 
				{
					if (a_p->mm->lsMoved) 
					{
						Util::PlayIdle("WerewolfLeftRunningPowerAttack", a_p->coopActor.get());
					}
					else
					{
						Util::PlayIdle("WerewolfLeftPowerAttack", a_p->coopActor.get());
					}
				}
				else
				{
					// Have to trigger a normal LH attack first so that the power attack command executes successfully.
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftPowerAttack, a_p->coopActor.get());
				}
			}
			else if (a_action == InputAction::kPowerAttackRH)
			{
				if (!a_p->isPlayer1 && Util::IsWerewolf(a_p->coopActor.get()))
				{
					if (a_p->mm->lsMoved)
					{
						Util::PlayIdle("WerewolfRightRunningPowerAttack", a_p->coopActor.get());
					}
					else
					{
						Util::PlayIdle("WerewolfRightPowerAttack", a_p->coopActor.get());
					}
				}
				else
				{
					// Unarmed right power attack fails to trigger via action command at times if using MCO.
					// Play the corresponding MCO idle instead.
					if (ALYSLC::MCOCompat::g_mcoInstalled) 
					{
						if (a_p->em->IsUnarmed()) 
						{
							Util::PlayIdle("ADXP_NPCPowerAttack_H2H", a_p->coopActor.get());
						}
						else
						{
							Util::PlayIdle("ADXP_NPCPowerAttack", a_p->coopActor.get());
						}
					}
					else
					{
						if (a_p->em->IsUnarmed())
						{
							// Have to trigger a normal RH attack before the H2H power attack idle;
							// otherwise, the power attack idle will fail to play.
							Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get());
							Util::PlayIdle("H2HRightHandPowerAttack", a_p->coopActor.get());
						}
						else
						{
							Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightPowerAttack, a_p->coopActor.get());
						}
					}
				}
			}
		}

		bool MovingBackward(const std::shared_ptr<CoopPlayer>& a_p, float a_facingToMovingAngDiff)
		{
			// Is moving backward relative to facing direction (90 degree window behind the player's facing direction).
			return 
			(
				(a_facingToMovingAngDiff <= (-3.0f / 4.0f * PI) && a_facingToMovingAngDiff >= (-5.0f / 4.0f * PI)) ||
				(a_facingToMovingAngDiff >= (3.0f / 4.0f * PI) && a_facingToMovingAngDiff <= (5.0f / 4.0f * PI)
			));
		}

		bool MovingForward(const std::shared_ptr<CoopPlayer>& a_p, float a_facingToMovingAngDiff)
		{
			// Is moving backward relative to facing direction (90 degree window in front of the player's facing direction).
			return 
			(
				(a_facingToMovingAngDiff >= (-1.0f / 4.0f * PI) && a_facingToMovingAngDiff <= (1.0f / 4.0f * PI)) ||
				(a_facingToMovingAngDiff >= (7.0f / 4.0f * PI) || a_facingToMovingAngDiff <= (-7.0f / 4.0f * PI))
			);
		}

		bool MovingLeft(const std::shared_ptr<CoopPlayer>& a_p, float a_facingToMovingAngDiff)
		{
			// Is moving left relative to facing direction (90 degree window to the left of the player's facing direction).
			return 
			(
				(a_facingToMovingAngDiff >= (-3.0f / 4.0f * PI) && a_facingToMovingAngDiff <= (-1.0f / 4.0f * PI)) ||
				(a_facingToMovingAngDiff >= (5.0f / 4.0f * PI) && a_facingToMovingAngDiff <= (7.0f / 4.0f * PI))
			);
		}
		bool MovingRight(const std::shared_ptr<CoopPlayer>& a_p, float a_facingToMovingAngDiff)
		{
			// Is moving right relative to facing direction (90 degree window to the right of the player's facing direction).
			return 
			(
				(a_facingToMovingAngDiff >= (1.0f / 4.0f * PI) && a_facingToMovingAngDiff <= (3.0f / 4.0f * PI)) ||
				(a_facingToMovingAngDiff <= (-5.0f / 4.0f * PI) && a_facingToMovingAngDiff >= (-7.0f / 4.0f * PI))
			);
		}

		bool RequestToUseParaglider(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Check if this player is requesting to paraglide.
			// Must have Skyrim's Paraglider installed:
			// https://www.nexusmods.com/skyrimspecialedition/mods/53256
			// Wish I could provide companion player compatibility for the slick paraglide animations. Sadge.
			if (ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled)
			{
				if (auto charController = a_p->coopActor->GetCharController(); charController)
				{
					bool justStarted = HelperFuncs::ActionJustStarted(a_p, InputAction::kActivate);
					if (justStarted)
					{
						// Reset request flag.
						a_p->pam->requestedToParaglide = false;
					}

					if (ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider)
					{
						bool isAirborne = charController->context.currentState == RE::hkpCharacterStateType::kInAir;
						// Indicate that this player would like to paraglide if P1 has one and the player is in the air.
						if (justStarted && isAirborne)
						{
							a_p->pam->requestedToParaglide = true;
							// Toggle paraglide state for co-op companion players.
							if (!a_p->isPlayer1)
							{
								a_p->mm->shouldParaglide = !a_p->mm->isParagliding;
							}
						}

						// Once requested, activation of objects is disabled until the player presses activate again on the ground.
						// Prevents activation of objects while paragliding or when landing after paragliding.
						return a_p->pam->requestedToParaglide;
					}
				}
			}

			return false;
		}

		void SetCameraAdjustmentMode(const int32_t& a_reqCID, const InputAction& a_action, bool&& a_set)
		{
			// Set or reset the co-op camera's adjustment mode (Rotate, Zoom, or None).

			const auto newAdjMode = a_action == InputAction::kRotateCam ? CamAdjustmentMode::kRotate : CamAdjustmentMode::kZoom;
			auto& controllingCID = glob.cam->controlCamCID;
			// Can only set if no other mode is set (another player is controlling cameras).
			if (controllingCID != a_reqCID && glob.cam->camAdjMode == CamAdjustmentMode::kNone)
			{
				controllingCID = a_reqCID;
			}

			// Controller with control over the camera can adjust cam mode freely.
			if (controllingCID == a_reqCID)
			{
				// Set if not already set, reset to none otherwise.
				if (a_set)
				{
					glob.cam->camAdjMode = newAdjMode;
				}
				else
				{
					glob.cam->camAdjMode = CamAdjustmentMode::kNone;
				}
			}
		}

		void SetCameraState(const int32_t& a_reqCID, const InputAction& a_action)
		{
			// Set camera state to 'LockOn' or 'ManualPositioning' or reset to 'AutoTrail'.

			const auto newCamState = a_action == InputAction::kCamLockOn ? CamState::kLockOn : CamState::kManualPositioning;
			auto& controllingCID = glob.cam->controlCamCID;
			// Set new cam control CID if no players are adjusting the camera.
			if (controllingCID != a_reqCID && glob.cam->camAdjMode == CamAdjustmentMode::kNone)
			{
				controllingCID = a_reqCID;
			}

			// Controller with control over the camera can adjust cam mode freely.
			if (controllingCID == a_reqCID)
			{
				if (glob.cam->camState != newCamState)
				{
					glob.cam->camState = newCamState;
				}
				else
				{
					// Pressing the same cam state bind twice resets to the default.
					glob.cam->camState = CamState::kAutoTrail;
				}

				// Notify the player of the cam state change.
				const auto& p = glob.coopPlayers[a_reqCID];
				if (glob.cam->camState == CamState::kAutoTrail) 
				{
					p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera auto trail mode", glob.coopPlayers[a_reqCID]->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
				}
				else if (glob.cam->camState == CamState::kLockOn)
				{
					p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera lock on mode", glob.coopPlayers[a_reqCID]->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
				}
				else
				{
					p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera manual positioning mode", glob.coopPlayers[a_reqCID]->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
				}
			}
		}

		void SetUpCastingPackage(const std::shared_ptr<CoopPlayer>& a_p, bool&& a_lhCast, bool&& a_rhCast)
		{
			// Prepare companion player ranged package for casting with the given hand(s).

			const auto& em = a_p->em;
			const auto& pam = a_p->pam;
			if (a_p->coopActor->IsWeaponDrawn())
			{
				pam->SetCurrentPackage(glob.coopPackages[!PackageIndex::kTotal * a_p->controllerID + !PackageIndex::kRangedAttack]);
				bool is2HSpellCast = a_lhCast && a_rhCast && em->GetLHSpell() && em->GetRHSpell() && em->GetRHSpell()->equipSlot == glob.bothHandsEquipSlot;
				bool isVoiceSpellCast = !a_lhCast && !a_rhCast && em->voiceForm && em->voiceForm->As<RE::SpellItem>();
				bool isShoutCast = !a_lhCast && !a_rhCast && em->voiceForm && em->voiceForm->As<RE::TESShout>();
				// Set casting global variables to match the requested hand/voice slots.
				bool globVarNotSetYet = 
				{ 
					(is2HSpellCast && pam->castingGlobVars[!CastingGlobIndex::k2H]->value != 1.0f) ||
					(isShoutCast && pam->castingGlobVars[!CastingGlobIndex::kShout]->value != 1.0f) ||
					(isVoiceSpellCast && pam->castingGlobVars[!CastingGlobIndex::kVoice]->value != 1.0f) ||
					(!is2HSpellCast &&
						(a_lhCast && pam->castingGlobVars[!CastingGlobIndex::kLH]->value != 1.0f) ||
						(a_rhCast && pam->castingGlobVars[!CastingGlobIndex::kRH]->value != 1.0f)) 
				};

				// Cast requested but the player actor is not casting yet.
				bool notHandCastingYet = (a_lhCast && !pam->isCastingLH) || (a_rhCast && !pam->isCastingRH);
				// Set target linked reference before setting and evaluating package.
				bool aimTargetRefrChanged = a_p->tm->UpdateAimTargetLinkedRefr(a_lhCast ? EquipIndex::kLeftHand : EquipIndex::kRightHand);

				// Set casting global variables that are used in the ranged attack package.
				// Also evaluate package if the aim target linked reference changed mid-execution.
				if (globVarNotSetYet || notHandCastingYet || aimTargetRefrChanged)
				{
					if (is2HSpellCast) 
					{
						pam->castingGlobVars[!CastingGlobIndex::kLH]->value = pam->castingGlobVars[!CastingGlobIndex::kRH]->value = 0.0f;
						pam->castingGlobVars[!CastingGlobIndex::k2H]->value = 1.0f;
					}
					else if (isShoutCast)
					{
						pam->castingGlobVars[!CastingGlobIndex::kShout]->value = 1.0f;
						pam->castingGlobVars[!CastingGlobIndex::kVoice]->value = 0.0f;
					}
					else if (isVoiceSpellCast)
					{
						pam->castingGlobVars[!CastingGlobIndex::kShout]->value = 0.0f;
						pam->castingGlobVars[!CastingGlobIndex::kVoice]->value = 1.0f;
					}
					else
					{
						pam->castingGlobVars[!CastingGlobIndex::kLH]->value = a_lhCast ? 1.0f : pam->castingGlobVars[!CastingGlobIndex::kLH]->value;
						pam->castingGlobVars[!CastingGlobIndex::kRH]->value = a_rhCast ? 1.0f : pam->castingGlobVars[!CastingGlobIndex::kRH]->value;
						pam->castingGlobVars[!CastingGlobIndex::k2H]->value = 0.0f;
						
						// TODO: Add dual casting support
						//pam->castingGlobVars[!CastingGlobIndex::kDual]->value = a_lhCast && a_rhCast ? 1.0f : pam->castingGlobVars[!CastingGlobIndex::kDual]->value;
					}

					// Evaluate ranged attack package with updated globals.
					pam->EvaluatePackage();
				}
			}
			else
			{
				// Ready weapon/spell/fists if not out already.
				pam->ReadyWeapon(true);
			}
		}

		void UseWeaponOnHold(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Use the equipped weapon in the hand corresponding to the requested action.

			const auto& pam = a_p->pam;
			const auto& em = a_p->em;
			bool isLeftHand = a_action == InputAction::kAttackLH;
			// Draw weapon/mag/fists if not out already.
			if (ActionJustStarted(a_p, a_action) && !a_p->coopActor->IsWeaponDrawn())
			{
				pam->ReadyWeapon(true);
				return;
			}

			if (a_p->isPlayer1) 
			{
				if (em->Has2HRangedWeapEquipped() && !isLeftHand)
				{
					// Need to toggle AI driven to draw bow.
					// Playing the right attack action on player 1 does not update
					// the draw timer, so all projectiles fire at the lowest release speed.
					if (ActionJustStarted(a_p, a_action))
					{
						auto currentAmmo = a_p->coopActor->GetCurrentAmmo();
						auto invCounts = a_p->coopActor->GetInventoryCounts();
						const int32_t ammoCount = invCounts.contains(currentAmmo) ? invCounts.at(currentAmmo) : -1;
						if (!currentAmmo || ammoCount <= 0)
						{
							// Notify the player that they do not have ammo equipped.
							a_p->tm->SetCrosshairMessageRequest(
								CrosshairMessageType::kGeneralNotification,
								fmt::format("P{}: No equipped ammo!", a_p->playerID + 1),
								{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
								Settings::fSecsBetweenDiffCrosshairMsgs);
						}
						else
						{
							// Notify the player of their ammo count before shooting the projectile.
							a_p->tm->SetCrosshairMessageRequest(
								CrosshairMessageType::kGeneralNotification,
								fmt::format("P{}: Ammo counter: {}", a_p->playerID + 1, max(0, ammoCount)),
								{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
								Settings::fSecsBetweenDiffCrosshairMsgs);
						}

						// Send directly without queuing.
						pam->SendButtonEvent(a_action, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 0.0f, true);
					}
					else
					{
						// Don't need to toggle AI driven after drawing the bow/crossbow.
						pam->QueueP1ButtonEvent(a_action, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 0.0f, false);
					}
				}
				else if ((isLeftHand && em->HasLHStaffEquipped()) || (!isLeftHand && em->HasRHStaffEquipped()))
				{
					// Cast with staff.
					auto staff = isLeftHand ? em->GetLHWeapon() : em->GetRHWeapon();
					a_p->pam->CastStaffSpell(staff, isLeftHand, true);
				}
			}
			else
			{
				if (ActionJustStarted(a_p, a_action) && !isLeftHand && em->Has2HRangedWeapEquipped())
				{
					auto currentAmmo = a_p->coopActor->GetCurrentAmmo();
					auto invCounts = a_p->coopActor->GetInventoryCounts();
					const int32_t ammoCount = invCounts.contains(currentAmmo) ? invCounts.at(currentAmmo) : -1;
					if (!currentAmmo || ammoCount <= 0)
					{
						// Notify the player that they do not have ammo equipped.
						a_p->tm->SetCrosshairMessageRequest(
							CrosshairMessageType::kGeneralNotification,
							fmt::format("P{}: No equipped ammo!", a_p->playerID + 1),
							{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
							Settings::fSecsBetweenDiffCrosshairMsgs);
					}
					else
					{
						// Notify the player of their ammo count before shooting the projectile.
						a_p->tm->SetCrosshairMessageRequest(
							CrosshairMessageType::kGeneralNotification,
							fmt::format("P{}: Ammo counter: {}", a_p->playerID + 1, max(0, ammoCount)),
							{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
							Settings::fSecsBetweenDiffCrosshairMsgs);
					}

					// Trigger via action command.
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get());
				}
				else
				{
					// Cast with staff.
					bool lhStaffCastReq = isLeftHand && em->HasLHStaffEquipped() && pam->usingLHStaff->value != 1.0f;
					bool rhStaffCastReq = !isLeftHand && em->HasRHStaffEquipped() && pam->usingRHStaff->value != 1.0f;
					if (lhStaffCastReq || rhStaffCastReq) 
					{
						auto staff = isLeftHand ? em->GetLHWeapon() : em->GetRHWeapon();
						a_p->pam->CastStaffSpell(staff, isLeftHand, true);
					}
				}
			}
		}

		void ValidateActivationRefr(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Check if the selected activation target refr is valid for activation,
			// and update the player's crosshair text to reflect that determination.
			
			// Set revive message if there is a downed player target.
			if (a_p->pam->downedPlayerTarget)
			{
				if (HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kActivate))
				{
					// Set revive player message.
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kReviveAlert,
						fmt::format("P{}: <font color=\"#1E88E5\">Revive {}</font>", a_p->playerID + 1, a_p->pam->downedPlayerTarget->coopActor->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
				}
				else
				{
					// Not enough health.
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kReviveAlert,
						fmt::format("P{}: <font color=\"#FF0000\">Not enough health to revive another player!</font>", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
				}
			}
			else
			{
				// Clear activation flag. Only set to true if valid below.
				a_p->tm->canActivateRefr = false;
				// Set activation message if activation refr is valid.
				const auto activationRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle);
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
					bool anotherPlayerControllingMenus = !GlobalCoopData::CanControlMenus(a_p->controllerID);
					// Activation will teleport P1.
					bool tryingToUseTeleportRefr = activationRefrPtr->extraList.HasType<RE::ExtraTeleport>();
					// Ensure that players cannot activate any refr that will teleport the party, 
					// and consequently auto-save, while a player is downed.
					bool otherPlayerDowned = std::any_of(glob.coopPlayers.begin(), glob.coopPlayers.end(), [](const auto& a_p) {
						if (a_p->isActive && a_p->isDowned)
						{
							return true;
						}

						return false;
					});

					// Other activation criteria.
					bool menusOnlyAlwaysOpen = true;
					if (auto ui = RE::UI::GetSingleton(); ui)
					{
						menusOnlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
					}

					bool isLocked = activationRefrPtr->IsLocked();
					// Is locked and P1 has the key.
					bool canUnlockWithKey = false;
					if (isLocked)
					{
						if (auto lockData = activationRefrPtr->extraList.GetByType<RE::ExtraLock>(); lockData && lockData->lock)
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
					bool hasLockpicks = Util::GetLockpicksCount(RE::PlayerCharacter::GetSingleton()) > 0;
					// A crime to activate.
					bool offLimits = activationRefrPtr->IsCrimeToActivate();
					// Object prevented from being activated (ex. door bars).
					bool activationBlocked = false;
					if (auto xFlags = activationRefrPtr->extraList.GetByType<RE::ExtraFlags>(); xFlags)
					{
						activationBlocked = xFlags && xFlags->flags.all(RE::ExtraFlags::Flag::kBlockPlayerActivate) && !activationRefrPtr->extraList.GetByType<RE::ExtraAshPileRef>();
					}

					// In activation range.
					bool isInRange = a_p->tm->RefrIsInActivationRange(a_p->tm->activationRefrHandle);
					// Is a lootable refr.
					bool isLootable = Util::IsLootableRefr(activationRefrPtr.get());
					// Player is sneaking.
					bool isSneaking = a_p->coopActor->IsSneaking();
					// Something to do with usability.
					bool isPlayable = activationRefrPtr->GetPlayable();
					// Player has LOS on the refr.
					bool passesLOSCheck = (Settings::bCamCollisions || (activationRefrPtr && Util::HasLOS(activationRefrPtr.get(), a_p->coopActor.get(), false, a_p->tm->crosshairRefrHandle == a_p->tm->activationRefrHandle, a_p->tm->crosshairWorldPos)));
					// Crosshair message to display.
					RE::BSFixedString activationMessage = ""sv;
					RE::BSString activationString = "";
					if (!isSneaking && offLimits)
					{
						// Must sneak to interact with off-limits refrs.
						if (!baseObj->GetActivateText(activationRefrPtr.get(), activationString))
						{
							activationMessage = fmt::format("P{}: Sneak to interact with {}", a_p->playerID + 1, activationRefrPtr->GetName());
						}
						else
						{
							activationMessage = fmt::format("P{}: Sneak to {}", a_p->playerID + 1, activationString);
						}
					}
					else if (!isPlayable || activationBlocked)
					{
						// Blocked from activating.
						activationMessage = fmt::format("P{}: {} cannot be activated", a_p->playerID + 1, activationRefrPtr->GetName());
					}
					else if (isLocked && !hasLockpicks && !canUnlockWithKey)
					{
						// No lockpicks or key.
						activationMessage = fmt::format("P{}: Out of lockpicks", a_p->playerID + 1);
					}
					else if (otherPlayerDowned && tryingToUseTeleportRefr)
					{
						// Can't leave the current cell with a player downed.
						activationMessage = fmt::format("P{}: Cannot leave downed teammates behind", a_p->playerID + 1);
					}
					else if (!menusOnlyAlwaysOpen && anotherPlayerControllingMenus && !isLootable)
					{
						// Another player is controlling menus and the target refr is not lootable.
						activationMessage = fmt::format("P{}: Another player is controlling menus.", a_p->playerID + 1);
					}
					else if (!passesLOSCheck)
					{
						// Player has no LOS.
						activationMessage = fmt::format("P{}: {} is not within player's line of sight", a_p->playerID + 1, activationRefrPtr->GetName());
					}
					else
					{
						// Is another player.
						if (GlobalCoopData::IsCoopPlayer(activationRefrPtr.get()))
						{
							activationMessage = fmt::format("P{}: Player {}", a_p->playerID + 1, activationRefrPtr->GetDisplayFullName());
						}
						else
						{
							if (isInRange)
							{
								// Success case: must be in range.
								// Set activation text to actor's name if no text is available.
								if (!baseObj->GetActivateText(activationRefrPtr.get(), activationString))
								{
									activationMessage = fmt::format("P{}: Interact with {}", a_p->playerID + 1, activationRefrPtr->GetName());
								}
								else
								{
									activationMessage = fmt::format("P{}: {}", a_p->playerID + 1, activationString);
								}

								a_p->tm->canActivateRefr = true;
							}
							else
							{
								// Not in range.
								activationMessage = fmt::format("P{}: {} is too far away", a_p->playerID + 1, activationRefrPtr->GetName());
							}
						}
					}

					// Set crosshair message.
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kActivationInfo,
						activationMessage,
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
				}
			}
		}
	};

	namespace ProgressFuncs
	{
		void Activate(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Perform activation cycling/validation if not using the paraglider.

			if (bool wantsToUseParaglider = HelperFuncs::RequestToUseParaglider(a_p); !wantsToUseParaglider)
			{
				HelperFuncs::PerformActivationCycling(a_p);
				HelperFuncs::ValidateActivationRefr(a_p);
			}
		}

		void AttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Use weapon if it is drawn.

			if (a_p->coopActor->IsWeaponDrawn())
			{
				HelperFuncs::UseWeaponOnHold(a_p, InputAction::kAttackLH);
			}
		}

		void AttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Use weapon if it is drawn.

			if (a_p->coopActor->IsWeaponDrawn())
			{
				HelperFuncs::UseWeaponOnHold(a_p, InputAction::kAttackRH);
			}
		}

		void Bash(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Power bash when the bash bind is held long enough,
			// the player has the power bash perk, and the player is not already bashing.

			if (a_p->pam->GetSecondsSinceLastStart(InputAction::kBash) > Settings::fSecsDefMinHoldTime && 
				a_p->coopActor->HasPerk(glob.powerBashPerk) && !a_p->pam->isAttacking) 
			{

				bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kBash);
				if (!performingKillmove)
				{
					// Continue blocking if the player was blocking before the bash; stop otherwise.
					bool wasBlocking = a_p->pam->IsPerforming(InputAction::kBlock);
					// Action command for both player types.
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightPowerAttack, a_p->coopActor.get());
					// Redundancy since block start/stop requests fail at times.
					if (wasBlocking)
					{
						Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
						a_p->coopActor->NotifyAnimationGraph("blockStart");
					}
					else
					{
						Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get());
						a_p->coopActor->NotifyAnimationGraph("attackStop");
						a_p->coopActor->NotifyAnimationGraph("blockStop");
					}
				}
			}
		}

		void CastLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cast spell in the LH.

			if (a_p->isPlayer1) 
			{
				// Draw weapon if not out already.
				// Send button event.
				if (HelperFuncs::ActionJustStarted(a_p, InputAction::kCastLH) && !a_p->coopActor->IsWeaponDrawn())
				{
					a_p->pam->ReadyWeapon(true);
				}

				a_p->pam->QueueP1ButtonEvent(InputAction::kCastLH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 0.0f, false);
			}
			else
			{
				// Setup LH casting variables, add ranged package to the top of the player's
				// package stack, and evaluate to start LH casting.
				auto lhSpell = a_p->em->GetLHSpell();
				// Has associated bound weapon.
				if (auto assocWeap = Util::GetAssociatedBoundWeap(lhSpell); assocWeap)
				{
					HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kCastLH);
				}
				else
				{
					bool is2HSpell = lhSpell && lhSpell->equipSlot == glob.bothHandsEquipSlot;
					if (is2HSpell)
					{
						HelperFuncs::SetUpCastingPackage(a_p, true, true);
					}
					else
					{
						HelperFuncs::SetUpCastingPackage(a_p, true, false);
					}

					// Cast with P1 if necessary.
					if (bool shouldCastWithP1 = Util::ShouldCastWithP1(lhSpell); shouldCastWithP1) 
					{
						a_p->pam->CastSpellWithMagicCaster(EquipIndex::kLeftHand, true, true, shouldCastWithP1);
					}
				}
			}
		}

		void CastRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cast with spell in the RH.

			if (a_p->isPlayer1)
			{
				// Draw weapon if not out already.
				// Send button event.
				if (HelperFuncs::ActionJustStarted(a_p, InputAction::kCastRH) && !a_p->coopActor->IsWeaponDrawn())
				{
					a_p->pam->ReadyWeapon(true);
				}

				a_p->pam->QueueP1ButtonEvent(InputAction::kCastRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 0.0f, false);
			}
			else
			{
				// Setup RH casting variables, add ranged package to the top of the player's
				// package stack, and evaluate to start RH casting.
				auto rhSpell = a_p->em->GetRHSpell();
				// Has associated bound weapon.
				if (auto assocWeap = Util::GetAssociatedBoundWeap(rhSpell); assocWeap)
				{
					HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kCastRH);
				}
				else
				{
					bool is2HSpell = rhSpell && rhSpell->equipSlot == glob.bothHandsEquipSlot;
					if (is2HSpell)
					{
						HelperFuncs::SetUpCastingPackage(a_p, true, true);
					}
					else
					{
						HelperFuncs::SetUpCastingPackage(a_p, false, true);
					}

					// Cast with P1 if necessary.
					if (bool shouldCastWithP1 = Util::ShouldCastWithP1(rhSpell); shouldCastWithP1)
					{
						a_p->pam->CastSpellWithMagicCaster(EquipIndex::kRightHand, true, true, shouldCastWithP1);
					}
				}
			}
		}

		void CycleAmmo(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player ammo on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleAmmo))
			{
				// Set last cycled form and then cycle ammo.
				a_p->em->lastCycledForm = a_p->em->currentCycledAmmo;
				a_p->em->CycleAmmo();
				if (RE::TESForm* ammo = a_p->em->currentCycledAmmo; ammo)
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip '{}'", a_p->playerID + 1, ammo->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: No favorited ammo", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		void CycleSpellCategoryLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player LH spell category on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleSpellCategoryLH))
			{
				// Set last cycled spell category and then cycle.
				a_p->em->lastCycledSpellCategory = a_p->em->lhSpellCategory;
				a_p->em->CycleHandSlotMagicCategory(false);
				const std::string_view newCategory = a_p->em->FavMagCyclingCategoryToString(a_p->em->lhSpellCategory);
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Left hand spell category: '{}'", a_p->playerID + 1, newCategory),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		void CycleSpellCategoryRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player RH spell category on first press and on hold at a certain interval.
			// If ammo cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleSpellCategoryRH))
			{
				// Set last cycled spell category and then cycle.
				a_p->em->lastCycledSpellCategory = a_p->em->rhSpellCategory;
				a_p->em->CycleHandSlotMagicCategory(true);
				const std::string_view newCategory = a_p->em->FavMagCyclingCategoryToString(a_p->em->rhSpellCategory);
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Right hand spell category: '{}'", a_p->playerID + 1, newCategory),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		void CycleSpellLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player LH spell on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleSpellLH))
			{
				// Set last cycled spell and then cycle.
				a_p->em->lastCycledForm = a_p->em->currentCycledLHSpellsList[!a_p->em->lhSpellCategory];
				// Update cycled spell from the current spell category.
				a_p->em->CycleHandSlotMagic(false);
				if (RE::TESForm* spellForm = a_p->em->currentCycledLHSpellsList[!a_p->em->lhSpellCategory]; spellForm)
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip '{}'", a_p->playerID + 1, spellForm->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: '{}' category is empty", a_p->playerID + 1, a_p->em->FavMagCyclingCategoryToString(a_p->em->lhSpellCategory)),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		void CycleSpellRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player RH spell on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleSpellRH))
			{
				// Set last cycled spell and then cycle.
				a_p->em->lastCycledForm = a_p->em->currentCycledRHSpellsList[!a_p->em->rhSpellCategory];
				// Update cycled spell from the current spell category.
				a_p->em->CycleHandSlotMagic(true);
				if (RE::TESForm* spellForm = a_p->em->currentCycledRHSpellsList[!a_p->em->rhSpellCategory]; spellForm)
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip '{}'", a_p->playerID + 1, spellForm->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: '{}' category is empty", a_p->playerID + 1, a_p->em->FavMagCyclingCategoryToString(a_p->em->rhSpellCategory)),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		void CycleVoiceSlotMagic(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player voice spell/shout on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleVoiceSlotMagic))
			{
				// Set last cycled spell and then cycle.
				a_p->em->lastCycledForm = a_p->em->currentCycledVoiceMagic;
				// Update cycled spell from the current category.
				a_p->em->CycleVoiceSlotMagic();
				if (RE::TESForm* voiceForm = a_p->em->currentCycledVoiceMagic; voiceForm)
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip '{}'", a_p->playerID + 1, voiceForm->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: No favorited powers/shouts", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		void CycleWeaponCategoryLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player LH weapon category on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleWeaponCategoryLH))
			{
				// Set last cycled weapon category and then cycle.
				a_p->em->lastCycledWeaponCategory = a_p->em->lhWeaponCategory;
				a_p->em->CycleWeaponCategory(false);
				const std::string_view newCategory = a_p->em->FavWeaponCyclingCategoryToString(a_p->em->lhWeaponCategory);
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Left hand weapon category: '{}'", a_p->playerID + 1, newCategory),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		void CycleWeaponCategoryRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player RH weapon category on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleWeaponCategoryRH)) 
			{
				// Set last cycled weapon category and then cycle.
				a_p->em->lastCycledWeaponCategory = a_p->em->rhWeaponCategory;
				a_p->em->CycleWeaponCategory(true);
				const std::string_view newCategory = a_p->em->FavWeaponCyclingCategoryToString(a_p->em->rhWeaponCategory);
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Right hand weapon category: '{}'", a_p->playerID + 1, newCategory),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		void CycleWeaponLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player LH weapon on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleWeaponLH)) 
			{
				// Set last cycled form and then cycle.
				a_p->em->lastCycledForm = a_p->em->currentCycledLHWeaponsList[!a_p->em->lhWeaponCategory];
				// Update cycled weapon from the current weapon category.
				a_p->em->CycleWeapons(false);
				if (RE::TESForm* form = a_p->em->currentCycledLHWeaponsList[!a_p->em->lhWeaponCategory]; form)
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip '{}'", a_p->playerID + 1, form->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else if (a_p->em->lhWeaponCategory == FavWeaponCyclingCategory::kAllFavorites && 
						 a_p->em->HasCyclableWeaponInCategory(FavWeaponCyclingCategory::kAllFavorites, false))
				{
					// Will empty both hands if no weapon is returned while cycling through all favorites.
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip 'Fists'", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: '{}' category is empty", a_p->playerID + 1, a_p->em->FavWeaponCyclingCategoryToString(a_p->em->lhWeaponCategory)),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		// Cycle weapon in RH on first press and on hold at a certain interval.
		void CycleWeaponRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player RH weapon on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleWeaponRH))
			{
				// Set last cycled form and then cycle.
				a_p->em->lastCycledForm = a_p->em->currentCycledRHWeaponsList[!a_p->em->rhWeaponCategory];
				// Update cycled weapon from the current weapon category.
				a_p->em->CycleWeapons(true);
				RE::TESForm* form = a_p->em->currentCycledRHWeaponsList[!a_p->em->rhWeaponCategory];
				if (form)
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip '{}'", a_p->playerID + 1, form->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else if (a_p->em->rhWeaponCategory == FavWeaponCyclingCategory::kAllFavorites &&
						 a_p->em->HasCyclableWeaponInCategory(FavWeaponCyclingCategory::kAllFavorites, true))
				{
					// Will empty both hands if no weapon is returned while cycling through all favorites.
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip 'Fists'", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: '{}' category is empty", a_p->playerID + 1, a_p->em->FavWeaponCyclingCategoryToString(a_p->em->rhWeaponCategory)),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}

		void GrabObject(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Collect nearby clutter when this bind is held and the auto-grab clutter setting is enabled.

			// Check for any clutter objects to auto-grab.
			if (bool shouldAutoGrab = Settings::bAutoGrabClutterOnHold; shouldAutoGrab)
			{
				// Set auto grab TP to action start TP initially.
				// Do not check yet.
				if (HelperFuncs::ActionJustStarted(a_p, InputAction::kGrabObject))
				{
					a_p->tm->rmm->isAutoGrabbing = false;
					a_p->lastAutoGrabTP = SteadyClock::now();
				}

				// Check for next object to grab at an interval.
				float secsSinceLastAutoGrab = Util::GetElapsedSeconds(a_p->lastAutoGrabTP);
				bool checkForGrabbableRefr = secsSinceLastAutoGrab > Settings::fSecsBeforeAutoGrabbingNextRefr;
				if (checkForGrabbableRefr) 
				{
					a_p->lastAutoGrabTP = SteadyClock::now();
				}

				auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
				bool crosshairRefrValidity = crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get());
				// Can auto-grab if the check interval has passed,
				// if not facing a target (face a target to throw instead), 
				// if no refr is targeted by the crosshair, 
				// and if another refr can be grabbed.
				shouldAutoGrab = checkForGrabbableRefr && !a_p->mm->shouldFaceTarget && !crosshairRefrValidity && a_p->tm->rmm->CanGrabAnotherRefr();
				if (shouldAutoGrab)
				{
					// On to the next check.
					shouldAutoGrab = false;
					// Check for neaby lootable refrs within activation range.
					RE::ObjectRefHandle refrToGrabHandle = RE::ObjectRefHandle();
					Util::ForEachReferenceInRange(Util::Get3DCenterPos(a_p->coopActor.get()), a_p->tm->GetMaxActivationDist(), true,
						[&a_p, &shouldAutoGrab, &refrToGrabHandle](RE::TESObjectREFR* a_refr) {
							// No refr, continue.
							if (!a_refr)
							{
								return RE::BSContainer::ForEachResult::kContinue;
							}

							auto handle = a_refr->GetHandle(); 
							// Continue if handle is invalid.
							if (!Util::HandleIsValid(handle))
							{
								return RE::BSContainer::ForEachResult::kContinue;
							}

							auto baseObj = a_refr->GetBaseObject();
							// Continue if object is invalid, not lootable, or managed already.
							// Is lootable and not managed already.
							if (!baseObj || !Util::IsLootableObject(*baseObj) || !a_p->tm->rmm->CanGrabRefr(handle))
							{
								return RE::BSContainer::ForEachResult::kContinue;
							}

							auto refr3D = Util::GetRefr3D(a_refr);
							// Continue if refr's 3D is unavailable.
							if (!refr3D)
							{
								return RE::BSContainer::ForEachResult::kContinue;
							}

							auto hkpRigidBody = Util::GethkpRigidBody(refr3D.get());
							// Continue if refr's havok rigid body is unavailable.
							if (!hkpRigidBody)
							{
								return RE::BSContainer::ForEachResult::kContinue;
							}

							bool grabbedByAnotherPlayer = false;
							for (const auto& p : glob.coopPlayers)
							{
								if (p->isActive && p != a_p)
								{
									if (!grabbedByAnotherPlayer && p->tm->rmm->IsManaged(handle, true))
									{
										grabbedByAnotherPlayer = true;
									}

									// If released by another player, remove this refr from that player's released refr manager,
									// since this player is about to grab it.
									if (p->tm->rmm->IsManaged(handle, false))
									{
										p->tm->rmm->ClearRefr(handle);
									}
								}
							}

							// Can only manipulate this object if it is not grabbed by another player and has a supported motion type.
							shouldAutoGrab = 
							{ 
								!grabbedByAnotherPlayer &&
								hkpRigidBody->motion.type != RE::hkpMotion::MotionType::kFixed &&
								hkpRigidBody->motion.type != RE::hkpMotion::MotionType::kInvalid 
							};
							if (shouldAutoGrab)
							{
								// Found an object to auto-grab.
								// Save handle and stop iterating through nearby refrs.
								refrToGrabHandle = handle;
								return RE::BSContainer::ForEachResult::kStop;
							}
							else
							{
								// On to the next refr.
								return RE::BSContainer::ForEachResult::kContinue;
							}
						});

					// Grab the valid refr and notify the player.
					if (shouldAutoGrab && Util::HandleIsValid(refrToGrabHandle))
					{
						auto targetRefrPtr = refrToGrabHandle.get();
						a_p->tm->SetIsGrabbing(true);
						a_p->tm->rmm->AddGrabbedRefr(glob.coopPlayers[a_p->controllerID], targetRefrPtr->GetHandle());
						a_p->tm->rmm->isAutoGrabbing = true;

						a_p->tm->SetCrosshairMessageRequest(
							CrosshairMessageType::kActivationInfo,
							fmt::format("P{}: Auto-grabbing {}", a_p->playerID + 1, targetRefrPtr->GetName()),
							{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
							Settings::fSecsBetweenDiffCrosshairMsgs);
					}
				}
			}
		}

		void HotkeyEquip(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Choose hotkey slot based on RS orientation,
			// then check the slot for an valid form to cache before equipping.
			// Update crosshair text to inform the player of which form 
			// they are about to equip when releasing the bind.

			if (HelperFuncs::ActionJustStarted(a_p, InputAction::kHotkeyEquip)) 
			{
				a_p->em->lastChosenHotkeyedForm = nullptr;
			}

			RE::TESForm* selectedHotkeyedForm = nullptr;
			auto hotkeySlot = HelperFuncs::GetSelectedHotkeySlot(a_p);
			if (hotkeySlot == -1)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kHotkeySelection,
					fmt::format("P{}: Invalid Hotkey ({})", a_p->playerID + 1, hotkeySlot + 1),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
				return;
			}

			if (selectedHotkeyedForm = a_p->em->hotkeyedForms[hotkeySlot]; selectedHotkeyedForm)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kHotkeySelection,
					fmt::format("P{}: Hotkey ({}): {}", a_p->playerID + 1, hotkeySlot + 1, selectedHotkeyedForm->GetName()),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kHotkeySelection,
					fmt::format("P{}: Hotkey ({}): NONE", a_p->playerID + 1, hotkeySlot + 1),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}

			// Update last chosen hotkeyed form.
			a_p->em->lastChosenHotkeyedForm = selectedHotkeyedForm;

			// First, check if the 'AttackRH'/'AttackLH' bind's composing inputs were just pressed,
			// which indicates that the player wishes to equip the selected hotkeyed form into a hand slot.
			//
			// Then, if the attack bind was just pressed, attempt to equip
			// the previously selected hotkeyed form to the hand slot.
			if (a_p->pam->GetPlayerActionInputJustReleased(InputAction::kAttackRH))
			{
				HelperFuncs::EquipHotkeyedForm(a_p, a_p->em->lastChosenHotkeyedForm, true);
			}
			else if (a_p->pam->GetPlayerActionInputJustReleased(InputAction::kAttackLH))
			{
				HelperFuncs::EquipHotkeyedForm(a_p, a_p->em->lastChosenHotkeyedForm, false);
			}
		}

		void QuickSlotCast(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cast QS spell if one is equipped, the player's instant caster is available,
			// and if the player has enough magicka to cast the spell.

			auto quickSlotSpell = a_p->em->quickSlotSpell;
			if (!quickSlotSpell ||
				!a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant) ||
				!HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kQuickSlotCast))
			{
				return;
			}

			// Quick slot bound weapon equip.
			// NOTE: For P1, not all bound weapons can be equipped with a quick slot cast.
			// If I recall correctly, the bound bow spell fails.
			if (auto assocWeap = Util::GetAssociatedBoundWeap(quickSlotSpell); assocWeap && !a_p->isPlayer1)
			{
				HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kQuickSlotCast);
			}
			else
			{
				if (auto magicCaster = a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant); magicCaster)
				{
					if (HelperFuncs::ActionJustStarted(a_p, InputAction::kQuickSlotCast))
					{
						// Set cast start TP for casting FNF spells at an interval.
						a_p->lastQSSCastStartTP = SteadyClock::now();
					}

					// Update ranged attack package target.
					a_p->tm->UpdateAimTargetLinkedRefr(EquipIndex::kQuickSlotSpell);
					// Perform the cast.
					a_p->pam->CastSpellWithMagicCaster(EquipIndex::kQuickSlotSpell, true, false, Util::ShouldCastWithP1(quickSlotSpell));
				}
			}
		}

		void Shout(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// TODO: For companion players: play shout anims and charge shout on hold, just like for P1.
			// Queue up emulated shout button press for P1 while the Shout bind is held.

			if (a_p->isPlayer1)
			{
				if (HelperFuncs::ActionJustStarted(a_p, InputAction::kShout)) 
				{
					// Only have to toggle AI driven on first press.
					a_p->pam->SendButtonEvent(InputAction::kShout, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kInstantTrigger);
				}
				else
				{
					// Queue event afterward.
					a_p->pam->QueueP1ButtonEvent(InputAction::kShout, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 0.0f, false);
				}
			}
		}

		void SpecialAction(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Perform the requested special action on press/on hold.

			bool justStarted = HelperFuncs::ActionJustStarted(a_p, InputAction::kSpecialAction);
			const auto& pam = a_p->pam;
			const auto& em = a_p->em;

			// Perform on-press/on press and release special actions.
			if (justStarted) 
			{
				if (pam->reqSpecialAction == SpecialActionType::kBlock)
				{				
					// On press and release. Do not clear here.
					HelperFuncs::BlockInstant(a_p, true);
				}
			}

			// On-hold special actions.
			float holdTime = a_p->pam->GetPlayerActionInputHoldTime(InputAction::kSpecialAction);
			if (pam->reqSpecialAction == SpecialActionType::kCastBothHands || pam->reqSpecialAction == SpecialActionType::kDualCast)
			{
				if (a_p->isPlayer1)
				{
					// Send button events to cast with both hands.
					// Togging AI driven not requird.
					if (justStarted)
					{
						a_p->pam->SendButtonEvent(InputAction::kCastLH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kInstantTrigger, 0.0f, false);
						a_p->pam->SendButtonEvent(InputAction::kCastRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kInstantTrigger, 0.0f, false);
					}
					else
					{
						a_p->pam->SendButtonEvent(InputAction::kCastLH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, std::move(holdTime), false);
						a_p->pam->SendButtonEvent(InputAction::kCastRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, std::move(holdTime), false);
					}
				}
				else if (em->HasLHSpellEquipped() && em->HasRHSpellEquipped())
				{
					auto lhSpell = em->GetLHSpell();
					auto rhSpell = em->GetRHSpell();
					auto assocWeapLH = Util::GetAssociatedBoundWeap(lhSpell);
					auto assocWeapRH = Util::GetAssociatedBoundWeap(rhSpell);
					bool shouldCastWithP1 = false;
					if (!assocWeapLH && !assocWeapRH) 
					{
						// No bound weapons to equip, so cast with both hands.
						HelperFuncs::SetUpCastingPackage(a_p, true, true);
						shouldCastWithP1 = Util::ShouldCastWithP1(lhSpell); 
						if (shouldCastWithP1)
						{
							a_p->pam->CastSpellWithMagicCaster(EquipIndex::kLeftHand, true, true, shouldCastWithP1);
						}

						shouldCastWithP1 = Util::ShouldCastWithP1(rhSpell);
						if (shouldCastWithP1)
						{
							a_p->pam->CastSpellWithMagicCaster(EquipIndex::kRightHand, true, true, shouldCastWithP1);
						}
					}
					else if (assocWeapLH && assocWeapRH)
					{
						// Bound weaps in both hands.
						HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kSpecialAction);
					}
					else if (assocWeapLH)
					{
						// Bound weap in LH, regular spell in RH.
						HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kSpecialAction);
						HelperFuncs::SetUpCastingPackage(a_p, false, true);

						shouldCastWithP1 = Util::ShouldCastWithP1(rhSpell);
						if (shouldCastWithP1)
						{
							a_p->pam->CastSpellWithMagicCaster(EquipIndex::kRightHand, true, true, shouldCastWithP1);
						}
					}
					else
					{
						// Bound weap in RH, regular spell in LH.
						HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kSpecialAction);
						HelperFuncs::SetUpCastingPackage(a_p, true, false);

						shouldCastWithP1 = Util::ShouldCastWithP1(lhSpell);
						if (shouldCastWithP1)
						{
							a_p->pam->CastSpellWithMagicCaster(EquipIndex::kLeftHand, true, true, shouldCastWithP1);
						}
					}
				}
				else if ((em->HasLHStaffEquipped() && em->HasRHStaffEquipped()) && (pam->usingLHStaff->value != 1.0f || pam->usingRHStaff->value != 1.0f))
				{
					// Cast with both staves at once.
					pam->CastStaffSpell(em->GetLHWeapon(), true, true);
					pam->CastStaffSpell(em->GetRHWeapon(), false, true);
				}
			}
			else if (pam->reqSpecialAction == SpecialActionType::kQuickCast)
			{
				// QS cast.
				ProgressFuncs::QuickSlotCast(a_p);
			}
			else if (pam->reqSpecialAction == SpecialActionType::kGetUp)
			{
				if (a_p->pam->GetPlayerActionInputHoldTime(InputAction::kSpecialAction) > Settings::fSecsDefMinHoldTime && 
					a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kGetUp) 
				{
					// Stop other player from grabbing this player or get up if ragdolled on the ground.
					auto isGrabbedIter = std::find_if(glob.coopPlayers.begin(), glob.coopPlayers.end(),
						[&a_p](const auto& p) {
							return p->isActive && p->coopActor != a_p->coopActor && p->tm->rmm->IsManaged(a_p->coopActor->GetHandle(), true);
						});

					if (isGrabbedIter != glob.coopPlayers.end())
					{
						const auto& grabbingP = *isGrabbedIter;
						grabbingP->tm->rmm->ClearRefr(a_p->coopActor->GetHandle());
					}

					// Reset fall time and height before attempting to get up.
					if (auto charController = a_p->coopActor->GetCharController(); charController)
					{
						charController->fallStartHeight = a_p->coopActor->data.location.z;
						charController->fallTime = 0.0f;
					}

					a_p->coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					a_p->coopActor->NotifyAnimationGraph("GetUpBegin");
					a_p->coopActor->PotentiallyFixRagdollState();
				}
			}
			else if (pam->reqSpecialAction == SpecialActionType::kCycleOrPlayEmoteIdle && holdTime > max(Settings::fSecsDefMinHoldTime, Settings::fSecsCyclingInterval))
			{
				// Must be held long enough to start cycling.
				if (HelperFuncs::CanCycleOnHold(a_p, InputAction::kSpecialAction))
				{
					// Set last cycled emote and index before cycling.
					a_p->em->lastCycledIdleIndexPair = a_p->em->currentCycledIdleIndexPair;
					a_p->em->CycleEmoteIdles();
					// Get new idle and notify player.
					const auto& emoteIdleToPlay = a_p->em->currentCycledIdleIndexPair.first;
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Selected emote idle '{}'", a_p->playerID + 1, emoteIdleToPlay),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);

					// Update cycling time point after cycling.
					a_p->lastCyclingTP = SteadyClock::now();
				}
			}
		}
	};

	namespace StartFuncs
	{
		// Get all nearby objects of the same type and activate each.

		void ActivateAllOfType(const std::shared_ptr<CoopPlayer>& a_p)
		{
			auto activationRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle);
			bool activationRefrValidity = Util::IsValidRefrForTargeting(activationRefrPtr.get());
			bool passesLOSCheck = 
			{
				(activationRefrValidity) && 
				(Settings::bCamCollisions || 
				 (Util::HasLOS(activationRefrPtr.get(), a_p->coopActor.get(), false, a_p->tm->crosshairRefrHandle == a_p->tm->activationRefrHandle, a_p->tm->crosshairWorldPos)))
			};
			if (passesLOSCheck) 
			{
				// If the targeted activation refr is grabbed, loot all lootable objects that the player is currently grabbing.
				bool isGrabbedRefr = a_p->tm->rmm->IsManaged(activationRefrPtr->GetHandle(), true);
				if (isGrabbedRefr) 
				{
					// Have to clear all grabbed objects before looting since looting and removing one at a time
					// causes a crash from accessing an invalid handle 
					// due to the grabbed refr info list changing in size during iteration.

					// Save grabbed refr handles before they're cleared.
					std::vector<RE::ObjectRefHandle> grabbedRefrHandles{ };
					for (auto& grabbedRefrInfo : a_p->tm->rmm->grabbedRefrInfoList)
					{
						grabbedRefrHandles.emplace_back(grabbedRefrInfo->refrHandle);
					}

					// No longer managed once looted.
					// Clear grabbed refrs.
					a_p->tm->rmm->ClearAll();
					for (const auto& grabbedRefrHandle : grabbedRefrHandles) 
					{
						auto grabbedRefrPtr = Util::GetRefrPtrFromHandle(grabbedRefrHandle);
						if (grabbedRefrPtr && Util::IsLootableRefr(grabbedRefrPtr.get()))
						{
							// Loot individual item/aggregation of the same item.
							HelperFuncs::LootRefr(a_p, grabbedRefrPtr);
						}
						else if (grabbedRefrPtr && grabbedRefrPtr->As<RE::Actor>() && grabbedRefrPtr->IsDead())
						{
							// Loot all items from grabbed corpse.
							HelperFuncs::LootAllItemsFromCorpse(a_p, grabbedRefrPtr);
						}
					}

					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kActivationInfo,
						fmt::format("P{}: Looted all grabbed objects", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
				}
				else
				{
					auto asActor = activationRefrPtr->As<RE::Actor>();
					bool isCorpse = asActor && asActor->IsDead();
					if (isCorpse)
					{
						// Is corpse.
						HelperFuncs::LootAllItemsFromCorpse(a_p, activationRefrPtr);
						a_p->tm->SetCrosshairMessageRequest(
							CrosshairMessageType::kActivationInfo,
							fmt::format("P{}: Looted everything from '{}'", a_p->playerID + 1, activationRefrPtr->GetName()),
							{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
							Settings::fSecsBetweenDiffCrosshairMsgs
						);
					}
					else if (!activationRefrPtr->IsLocked() && activationRefrPtr->HasContainer())
					{
						// Is unlocked container.
						HelperFuncs::LootAllItemsFromContainer(a_p, activationRefrPtr);
						a_p->tm->SetCrosshairMessageRequest(
							CrosshairMessageType::kActivationInfo,
							fmt::format("P{}: Looted everything from container '{}'", a_p->playerID + 1, activationRefrPtr->GetName()),
							{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
							Settings::fSecsBetweenDiffCrosshairMsgs
						);
					}
					else
					{
						// Is lootable refr, not a corpse or container.
						const auto& nearbyObjectsOfSameType = a_p->tm->GetNearbyRefrsOfSameType(a_p->tm->activationRefrHandle);
						for (const auto& handle : nearbyObjectsOfSameType)
						{
							auto objectPtr = Util::GetRefrPtrFromHandle(handle);
							auto objectValidity = objectPtr && Util::IsValidRefrForTargeting(objectPtr.get());
							if (!objectValidity)
							{
								continue;
							}

							// Clear released refr if activating it, as its 3D will be removed once picked up and
							// will no longer need to be tracked in the targeting manager.
							if (a_p->tm->rmm->IsManaged(handle, true) || a_p->tm->rmm->IsManaged(handle, false))
							{
								a_p->tm->rmm->ClearRefr(handle);
							}

							HelperFuncs::LootRefr(a_p, objectPtr);
						}

						// Notify player.
						if (!nearbyObjectsOfSameType.empty() && activationRefrPtr)
						{
							if (auto baseObj = activationRefrPtr->GetBaseObject(); baseObj) 
							{
								a_p->tm->SetCrosshairMessageRequest(
									CrosshairMessageType::kActivationInfo,
									fmt::format("P{}: Looted every '{}' in range", a_p->playerID + 1, baseObj->GetName()),
									{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
									Settings::fSecsBetweenDiffCrosshairMsgs);
							}
						}
					}
				}
			}
		}

		void ActivateCancel(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Do not activate any cached activation refr.
			// No activation data to clear.

			if (auto activationRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle); activationRefrPtr) 
			{
				// Notify the player.
				if (auto baseObj = activationRefrPtr->GetBaseObject(); baseObj)
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kActivationInfo,
						fmt::format("P{}: Cancelling activation", a_p->playerID + 1, baseObj->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
				}
			}
		}

		void Block(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Start blocking with weapon/shield.

			// Stop attacking first.
			a_p->coopActor->NotifyAnimationGraph("attackStop");
			if (a_p->isPlayer1) 
			{
				// For Valhalla Combat projectile deflection compatibility.
				// Requires propagation of Block input event.
				a_p->pam->QueueP1ButtonEvent(InputAction::kBlock, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kInstantTrigger, 0.0f, false);
				a_p->pam->QueueP1ButtonEvent(InputAction::kBlock, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 0.1f, false);
			}
			else if (!a_p->coopActor->IsWeaponDrawn())
			{
				a_p->pam->ReadyWeapon(true);
			}

			bool canShieldChargeAfter = 
			{
				a_p->mm->lsMoved &&
				a_p->em->HasShieldEquipped() &&
				a_p->coopActor->HasPerk(glob.shieldChargePerk) &&
				a_p->pam->IsPerforming(InputAction::kSprint)
			};

			Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
			if (canShieldChargeAfter)
			{
				Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionSprintStart, a_p->coopActor.get());
			}
		}

		void CamLockOn(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Switch to lock on state if a new valid actor is targeted by this player's crosshair.
			// Reset to auto trail otherwise.

			// Modify cam-controlling CID.
			auto& controllingCID = glob.cam->controlCamCID;
			if (controllingCID != a_p->controllerID && glob.cam->camAdjMode == CamAdjustmentMode::kNone)
			{
				controllingCID = a_p->controllerID;
			}

			// Same controller as one with control over camera can adjust the cam state freely.
			if (controllingCID == a_p->controllerID)
			{
				// If this player has a selected target actor, it is within LOS on the screen,
				// since actors are not selectable if they are not hit by the crosshair's raycasts.
				auto targetActorPtr = Util::GetActorPtrFromHandle(a_p->tm->selectedTargetActorHandle);
				auto targetActorValidity = targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get());
				auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(glob.cam->camLockOnTargetHandle);
				bool newTarget = targetActorValidity && (!camLockOnTargetPtr || targetActorPtr != camLockOnTargetPtr);
				if (newTarget)
				{
					// If the crosshair refr target is not the same as the current cam lock on target,
					// switch to the lock on state and send a request to update the cam lock on target.
					glob.cam->lockOnActorReq = a_p->tm->selectedTargetActorHandle;
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera lock on mode", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
					glob.cam->camState = CamState::kLockOn;
				}
				else
				{
					// Otherwise, send a request to clear the cam lock on target
					// and reset the cam state to auto trail.
					glob.cam->lockOnActorReq = RE::ActorHandle();
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera auto trail mode", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
					glob.cam->camState = CamState::kAutoTrail;
				}
			}
		}

		void CamManualPos(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Toggle camera state between auto trail and manual positioning if the player can
			// obtain control of the camera.

			auto& controllingCID = glob.cam->controlCamCID;
			if (controllingCID != a_p->controllerID && glob.cam->camAdjMode == CamAdjustmentMode::kNone)
			{
				controllingCID = a_p->controllerID;
			}

			// Same controller as one with control over camera can adjust the cam state freely.
			if (controllingCID == a_p->controllerID)
			{
				if (glob.cam->camState != CamState::kManualPositioning)
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera manual positioning mode", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
					glob.cam->camState = CamState::kManualPositioning;
				}
				else
				{
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera auto trail mode", a_p->playerID + 1),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs);
					glob.cam->camState = CamState::kAutoTrail;
				}
			}
		}

		void ChangeDialoguePlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// If this player is controlling dialogue, hand over dialogue control
			// to another player who has most recently made a request.
			// If this player is not controlling dialogue, send a request for dialogue control.

			if (bool controllingDialogue = glob.menuCID == a_p->controllerID; controllingDialogue)
			{
				// Check for dialogue control request and relinquish menu control to the requesting player.
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				{
					std::unique_lock<std::mutex> lock(glob.moarm->reqDialogueControlMutex, std::try_to_lock);
					if (lock)
					{
						if (glob.moarm->reqDialoguePlayerCID != -1)
						{
							SPDLOG_DEBUG("[PAFH] ChangeDialoguePlayer: {}: Dialogue req CID lock obtained. (0x{:X})",
								a_p->coopActor->GetName(), hash);
							const auto& reqP = glob.coopPlayers[glob.moarm->reqDialoguePlayerCID];
							// Teleport the other player to the speaker/this player before handing over dialogue control
							// if the requesting player is not close enough to engage in dialogue.
							if (auto menuTopicMgr = RE::MenuTopicManager::GetSingleton(); menuTopicMgr) 
							{
								auto speakerPtr = Util::GetRefrPtrFromHandle(menuTopicMgr->speaker);
								if (!speakerPtr) 
								{
									speakerPtr = Util::GetRefrPtrFromHandle(menuTopicMgr->lastSpeaker);
								}

								if (speakerPtr) 
								{
									if (reqP->coopActor->data.location.GetDistance(speakerPtr->data.location) > Settings::fAutoEndDialogueRadius) 
									{
										Util::TeleportToActor(reqP->coopActor.get(), a_p->coopActor.get());
									}
								}
								else if (reqP->coopActor->data.location.GetDistance(a_p->coopActor->data.location) > Settings::fAutoEndDialogueRadius)
								{
									Util::TeleportToActor(reqP->coopActor.get(), a_p->coopActor.get());
								}
							}

							// Assign req control CID and stop/start menu input manager, as needed.
							bool isP1Req = reqP->coopActor->IsPlayerRef();
							glob.mim->SetOpenedMenu(RE::DialogueMenu::MENU_NAME, !isP1Req);
							if (isP1Req)
							{
								// Modify menu CID and signal MIM to pause.
								GlobalCoopData::SetMenuCIDs(reqP->controllerID);
								glob.mim->ToggleCoopPlayerMenuMode(-1);
							}
							else
							{
								// Set directly and request MIM to start.
								GlobalCoopData::SetMenuCIDs(reqP->controllerID);
								glob.mim->ToggleCoopPlayerMenuMode(reqP->controllerID);
							}

							// Notify both players.
							reqP->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kGeneralNotification,
								fmt::format("P{}: <font color=\"#E66100\">Now controlling dialogue</font>", reqP->playerID + 1),
								{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
								Settings::fSecsBetweenDiffCrosshairMsgs
							);

							// Reset req dialogue menu CID to -1 because request was handled (fulfilled or not).
							glob.moarm->reqDialoguePlayerCID = -1;
						}
					}
				}
			}
			else
			{
				// Submit request for menu control if there isn't one already.
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				{
					std::unique_lock<std::mutex> lock(glob.moarm->reqDialogueControlMutex, std::try_to_lock);
					if (lock)
					{
						if (glob.moarm->reqDialoguePlayerCID == -1)
						{
							SPDLOG_DEBUG("[PAFH] ChangeDialoguePlayer: {}: Req CID NOT set and lock obtained (0x{:X}). Set to {}.",
								a_p->coopActor->GetName(), hash, a_p->controllerID);

							// Set requesting player CID.
							glob.moarm->reqDialoguePlayerCID = a_p->controllerID;
							// Notify player.
							a_p->tm->SetCrosshairMessageRequest(
								CrosshairMessageType::kGeneralNotification,
								fmt::format("P{}: <font color=\"#E66100\">Requesting dialogue control</font>", a_p->playerID + 1),
								{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
								Settings::fSecsBetweenDiffCrosshairMsgs
							);
						}
					}
				}
			}
		}

		void CoopDebugMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the co-op debug menu via script event, which will also set the menu CID.

			glob.onDebugMenuRequest.SendEvent(a_p->coopActor.get(), a_p->controllerID);
		}

		void CoopIdlesMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the co-op idles menu via script event, which will also set the menu CID.

			glob.onCoopHelperMenuRequest.SendEvent(a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kIdles);
		}

		void CoopMiniGamesMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the co-op mini games menu via script event, which will also set the menu CID.

			glob.onCoopHelperMenuRequest.SendEvent(a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kMiniGames);
		}

		void CoopSummoningMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the co-op summoning menu via script event, which will also set the menu CID.
			// Don't open if already open.

			if (glob.summoningMenuOpenGlob->value == 0.0f)
			{
				glob.onSummoningMenuRequest.SendEvent();
			}
		}

		void DebugReEquipHandForms(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Re-equip items in the player's hands.
			// Useful when those items are bugged out in some way.

			a_p->em->ReEquipHandForms();
			a_p->tm->SetCrosshairMessageRequest(
				CrosshairMessageType::kGeneralNotification,
				fmt::format("P{}: Re-equipped items in hands", a_p->playerID + 1),
				{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void DebugRefreshPlayerManagers(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Refresh player data stored in each manager.
			// Useful to reset player state if they are bugged out.
			
			// Only stop menu input manager when only "always-open" menus are showing.
			if (Util::MenusOnlyAlwaysOpen()) 
			{
				GlobalCoopData::StopMenuInputManager();
			}

			a_p->taskRunner->AddTask([a_p]() { a_p->RefreshPlayerManagersTask(); });
			a_p->tm->SetCrosshairMessageRequest(
				CrosshairMessageType::kGeneralNotification,
				fmt::format("P{}: Refreshing player managers", a_p->playerID + 1),
				{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void DebugResetPlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset the player entirely:
			// - Re-equip hand items/unequip all items, if requested.
			// - Revert form if transformed.
			// - Resurrect.
			// - Reset havok + ragdoll, for companion players, if requested.
			// - Disable and re-enable for companion players.
			// A 'nuclear option' if the player is still bugged out after trying other debug options.

			if (a_p->isPlayer1) 
			{
				a_p->ResetPlayer1();
			}	
			else
			{
				// Don't unequip all here.
				// The debug menu reset equip state option does unequip all, 
				// so if the player's equip state is still bugged, choose that reset option instead.
				a_p->taskRunner->AddTask([a_p]() { a_p->ResetCompanionPlayerStateTask(false, true); });
			}

			a_p->tm->SetCrosshairMessageRequest(
				CrosshairMessageType::kGeneralNotification,
				fmt::format("P{}: Reset player", a_p->playerID + 1),
				{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void DebugRagdollPlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Ragdoll the player.
			// Useful for when the player's movement is bugged;
			// for example, when they are stuck in place.

			// Ensure the player is not paralyzed.
			a_p->coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
			// Detach and re-attach havok.
			if (auto player3D = Util::GetRefr3D(a_p->coopActor.get()); player3D)
			{
				a_p->coopActor->DetachHavok(player3D.get());
				a_p->coopActor->InitHavok();
				a_p->coopActor->MoveHavok(true);
			}

			// Knock 'em down.
			Util::PushActorAway(a_p->coopActor.get(), a_p->coopActor->data.location, -1.0f);
			a_p->coopActor->PotentiallyFixRagdollState();
			a_p->tm->SetCrosshairMessageRequest(
				CrosshairMessageType::kGeneralNotification,
				fmt::format("P{}: Ragdolling", a_p->playerID + 1),
				{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void DisableCoopCam(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Disable co-op camera and reset to the normal Third Person cam which follows P1.
			
			// Clear lock on target and reset adjustment mode before waiting for toggle.
			glob.cam->camAdjMode = CamAdjustmentMode::kNone;
			glob.cam->ClearLockOnData();
			glob.cam->waitForToggle = true;
			glob.cam->RequestStateChange(ManagerState::kPaused);
		}

		void Dismount(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Dismount any mount instantly. 
			// Also stop running the special interaction package to free the companion player from occupied furniture.
			// NOTE: Jumping also unlinks the player from occupied furniture.

			if (a_p->isPlayer1)
			{
				// Activate to dismount for P1.
				a_p->pam->SendButtonEvent(InputAction::kActivate, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kInstantTrigger, 0.0f);
				a_p->pam->SendButtonEvent(InputAction::kActivate, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 1.0f);
				a_p->pam->SendButtonEvent(InputAction::kActivate, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease, 1.0f);
			}
			else
			{
				// Reset to default package first.
				auto defPackage = a_p->pam->GetDefaultPackage();
				a_p->pam->SetCurrentPackage(defPackage);
				a_p->pam->EvaluatePackage();
			}

			// Get off mount/stop interacting with furniture.
			a_p->coopActor->StopInteractingQuick(false);
		}

		void Dodge(const std::shared_ptr<CoopPlayer>& a_p) 
		{
			// Currently, a speedmult-based dash dodge with I-frames has been implemented.
			// This general-use dodge is almost always triggerable when the player is grounded or paragliding.
			// TODO: Dodge support for other popular dodge mods and an MCM option to choose either 
			// the mod's speedmult dodge or any installed custom dodge mod.
			// TKDodge is partially supported (I-frames do not trigger as of now).
			
			if (Settings::bUseDashDodgeSystem) 
			{
				// Signal movement manager to dash dodge.
				a_p->mm->isRequestingDashDodge = true;
			}
			else if (ALYSLC::TKDodgeCompat::g_tkDodgeInstalled) 
			{
				// Stop sprinting before dodging.
				Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionSprintStop, a_p->coopActor.get());
				auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
				// Targeting a world position or refr.
				bool hasTarget = a_p->mm->shouldFaceTarget || (crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get()));
				const float& lsGameAngle = a_p->mm->movementOffsetParams[!MoveParams::kLSGameAng];
				// Facing to moving angle difference.
				float facingToMovingAngDiff = 0.0f;
				float angToTarget = Util::GetYawBetweenPositions(a_p->coopActor->data.location, a_p->tm->crosshairWorldPos);
				if (hasTarget)
				{
					// Face the target first.
					a_p->coopActor->data.angle.z = Util::NormalizeAng0To2Pi(angToTarget);
					if (a_p->mm->lsMoved)
					{
						facingToMovingAngDiff = lsGameAngle - angToTarget;
					}
					else
					{
						// Dodge backward if LS is not moved.
						facingToMovingAngDiff = PI;
					}
				}
				else
				{
					// Roll forwards when moving, backwards otherwise.
					facingToMovingAngDiff = a_p->mm->lsMoved ? 0.0f : PI;
				}

				// Not working at the moment. Will investigate later.
				a_p->coopActor->SetGraphVariableFloat("TKDR_IframeDuration", 0.3f);
				if (HelperFuncs::MovingBackward(a_p, facingToMovingAngDiff))
				{
					a_p->coopActor->actorState1.movingBack = 1;
					a_p->coopActor->actorState1.movingForward = 0;
					a_p->coopActor->actorState1.movingLeft = 0;
					a_p->coopActor->actorState1.movingRight = 0;
					a_p->coopActor->NotifyAnimationGraph("TKDodgeBack");
				}
				else if (HelperFuncs::MovingForward(a_p, facingToMovingAngDiff))
				{
					a_p->coopActor->actorState1.movingBack = 0;
					a_p->coopActor->actorState1.movingForward = 1;
					a_p->coopActor->actorState1.movingLeft = 0;
					a_p->coopActor->actorState1.movingRight = 0;
					a_p->coopActor->NotifyAnimationGraph("TKDodgeForward");
				}
				else if (HelperFuncs::MovingLeft(a_p, facingToMovingAngDiff))
				{
					a_p->coopActor->actorState1.movingBack = 0;
					a_p->coopActor->actorState1.movingForward = 0;
					a_p->coopActor->actorState1.movingLeft = 1;
					a_p->coopActor->actorState1.movingRight = 0;
					a_p->coopActor->NotifyAnimationGraph("TKDodgeLeft");
				}
				else if (HelperFuncs::MovingRight(a_p, facingToMovingAngDiff))
				{
					a_p->coopActor->actorState1.movingBack = 0;
					a_p->coopActor->actorState1.movingForward = 0;
					a_p->coopActor->actorState1.movingLeft = 0;
					a_p->coopActor->actorState1.movingRight = 1;
					a_p->coopActor->NotifyAnimationGraph("TKDodgeRight");
				}
			}
		}

		void FaceTarget(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Toggle face target mode.
			// If enabled, the player will face the crosshair position in worldspace.
			// Otherwise, the player rotates to face their movement direction as usual.
			a_p->mm->shouldFaceTarget = !a_p->mm->shouldFaceTarget;
			if (Settings::vbAnimatedCrosshair[a_p->playerID]) 
			{
				// Signal targeting manager to rotate the crosshair into the 'X'
				// configuration to notify the player that they are facing the crosshair position now.
				a_p->tm->crosshairRotationData->SetTimeSinceUpdate(0.0f);
				a_p->tm->crosshairRotationData->ShiftEndpoints(a_p->mm->shouldFaceTarget ? PI / 4.0f : 0.0f);
			}
		}

		void Favorites(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the FavoritesMenu.

			bool succ = glob.moarm->InsertRequest(a_p->controllerID, InputAction::kFavorites, SteadyClock::now(), RE::FavoritesMenu::MENU_NAME);
			if (succ) 
			{
				a_p->pam->SendButtonEvent(InputAction::kFavorites, RE::INPUT_DEVICE::kKeyboard, ButtonEventPressType::kInstantTrigger);

				// FavoritesMenu is blocked from opening when P1 is a Werewolf -- 
				// (no input event sent or processed by MenuControls or InputEventDispatcher) --
				// which blocks other non-transformed players from accessing their favorites,
				// so attempt to open it directly via the message queue instead.
				if (!Util::IsWerewolf(a_p->coopActor.get()) && Util::IsWerewolf(RE::PlayerCharacter::GetSingleton())) 
				{
					if (auto msgQueue = RE::UIMessageQueue::GetSingleton(); msgQueue) 
					{
						msgQueue->AddMessage(RE::FavoritesMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
					}
				}
			}
		}

		void Inventory(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open player 1's inventory if the requesting player is player 1.
			// Otherwise, open the companion player's 'inventory', which is a ContainerMenu.
			
			// Reset player 1 damage multiplier so that the co-op player's inventory 
			// reports the correct damage for weapons, if perks are also imported.
			glob.player1Actor->SetActorValue(RE::ActorValue::kAttackDamageMult, 1.0f);
			bool succ = glob.moarm->InsertRequest(a_p->controllerID, InputAction::kInventory, SteadyClock::now(), RE::ContainerMenu::MENU_NAME, a_p->coopActor->GetHandle());
			if (succ) 
			{
				if (a_p->isPlayer1)
				{
					a_p->pam->SendButtonEvent(InputAction::kInventory, RE::INPUT_DEVICE::kKeyboard, ButtonEventPressType::kInstantTrigger);
				}
				else
				{
					Util::Papyrus::OpenInventory(a_p->coopActor.get());
				}
			}
		}

		void Jump(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Signal movement manager to start jump.
		
			// Stop sprinting before jump.
			Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionSprintStop, a_p->coopActor.get());
			// Reset to default package.
			if (!a_p->isPlayer1 && !a_p->coopActor->IsOnMount())
			{
				auto defPackage = a_p->pam->GetDefaultPackage();
				a_p->pam->SetCurrentPackage(defPackage);
				a_p->pam->EvaluatePackage();
			}

			a_p->mm->startJump = true;
		}

		void MagicMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the MagicMenu.

			HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kMagicMenu);
			// Magic menu is blocked from opening via player input when P1 is transformed into a Werewolf or Vampire Lord,
			// but if this player is not transformed, open the Magic Menu via the message queue.
			if (!a_p->isTransformed && glob.coopPlayers[glob.player1CID]->isTransformed)
			{
				if (auto msgQueue = RE::UIMessageQueue::GetSingleton(); msgQueue)
				{
					msgQueue->AddMessage(RE::MagicMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
				}
			}
		}

		void MapMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the MapMenu.
			HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kMapMenu);
			// Map menu is blocked from opening via player input when P1 is transformed into a Werewolf or Vampire Lord,
			// but if this player is not transformed, open the Map Menu via the message queue.
			if (!a_p->isTransformed && glob.coopPlayers[glob.player1CID]->isTransformed)
			{
				if (auto msgQueue = RE::UIMessageQueue::GetSingleton(); msgQueue)
				{
					msgQueue->AddMessage(RE::MapMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
				}
			}
		}

		void Pause(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the JournalMenu to pause the game.
			HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kPause);
		}

		void PowerAttackDual(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Play dual-wield power attack animation.

			// Attempt to perform a power attack if the player's weapon is drawn and not in a killmove.
			// Otherwise, draw weapons/magic if arms rotation is disabled.
			if (a_p->coopActor->IsWeaponDrawn())
			{
				// Check if a killmove should be played first.
				bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kAttackLH);
				if (!performingKillmove)
				{
					HelperFuncs::PlayPowerAttackAnimation(a_p, InputAction::kPowerAttackDual);
				}
			}
			else if (!Settings::bRotateArmsWhenSheathed)
			{
				// Unsheathe if not rotating arms.
				// Do nothing otherwise, since the player is rotating their arms.
				a_p->pam->ReadyWeapon(true);
			}
		}

		void PowerAttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Play LH power attack animation.
			
			// Attempt to perform power attack if the player's weapon is drawn and not in a killmove.
			// Otherwise, draw weapons/magic if arms rotation is disabled.
			if (a_p->coopActor->IsWeaponDrawn()) 
			{
				// Check if a killmove should be played first.
				bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kAttackLH);
				if (!performingKillmove)
				{
					HelperFuncs::PlayPowerAttackAnimation(a_p, InputAction::kPowerAttackLH);
				}
			}
			else if (!Settings::bRotateArmsWhenSheathed)
			{
				// Unsheathe if not rotating arms.
				// Do nothing otherwise, since the player is rotating their arms.
				a_p->pam->ReadyWeapon(true);
			}
		}

		void PowerAttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Play RH power attack animation.
			
			// Attempt to perform power attack if the player's weapon is drawn and not in a killmove.
			// Otherwise, draw weapons/magic if arms rotation is disabled.
			if (a_p->coopActor->IsWeaponDrawn())
			{
				// Check if a killmove should be played first.
				bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kAttackLH);
				if (!performingKillmove)
				{
					HelperFuncs::PlayPowerAttackAnimation(a_p, InputAction::kPowerAttackRH);
				}
			}
			else if (!Settings::bRotateArmsWhenSheathed)
			{
				// Unsheathe if not rotating arms.
				// Do nothing otherwise, since the player is rotating their arms.
				a_p->pam->ReadyWeapon(true);
			}
		}

		void QuickSlotItem(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Use the quickslot consumable item if the player has at least 1.
			// Remove quickslot item otherwise.

			const auto qsItem = a_p->em->equippedForms[!EquipIndex::kQuickSlotItem];
			const auto qsBoundObj = qsItem && qsItem->IsBoundObject() ? qsItem->As<RE::TESBoundObject>() : nullptr;
			if (qsBoundObj) 
			{
				const auto inventoryCounts = a_p->coopActor->GetInventoryCounts();
				if (inventoryCounts.at(qsBoundObj) > 0)
				{
					// Has at least 1, so use the item via equip.
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem) 
					{
						aem->EquipObject(a_p->coopActor.get(), qsItem->As<RE::TESBoundObject>(), nullptr, 1, nullptr, false, true, true, true);
					}
				}
				else
				{
					// Remove QS item since the player does not have the item anymore.
					a_p->em->quickSlotItem = nullptr;
					a_p->em->equippedQSItemIndex = -1;
					a_p->em->RefreshEquipState(RefreshSlots::kWeapMag);
				}
			}
		}

		void ResetAim(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Signal the movement manager to reset this player's aim pitch and node rotations.

			a_p->mm->shouldResetAimAndBody = true;
		}

		void RotateCam(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Set cam adjustment mode to rotate if this player can obtain control of the camera.

			HelperFuncs::SetCameraAdjustmentMode(a_p->controllerID, InputAction::kRotateCam, true);
		}

		void Sheathe(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Sheathe if weapons/magic is out, draw otherwise.

			const bool shouldSheathe = a_p->coopActor->IsWeaponDrawn();
			a_p->pam->ReadyWeapon(!shouldSheathe);
			// Pacify formerly-friendly actors after sheathing.
			if (shouldSheathe)
			{
				a_p->pam->StopCombatWithFriendlyActors();
			}
		}

		void Sneak(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Toggle sneak state for the player.
			
			// Sneaking toggles levitation for Vampire Lords.
			// Enter sneak mode to levitate, which will also equip the default LH and RH vampiric spells.
			if (!a_p->isPlayer1 && Util::IsVampireLord(a_p->coopActor.get()))
			{
				// Set to true while toggling levitation state.
				// Don't queue another task if in the process of changing the levitation state.
				if (!a_p->isTogglingLevitationStateTaskRunning) 
				{
					a_p->taskRunner->AddTask(
						[&a_p]() {
							a_p->ToggleVampireLordLevitationTask();
						}
					);
				}
			}
			else
			{
				// Toggle with action command.
				// Ensure package flags are in sync for co-op companions,
				// and also update the actor state to keep everything in sync.
				bool isSneaking = a_p->coopActor->IsSneaking();
				if (isSneaking)
				{
					if (!a_p->isPlayer1)
					{
						a_p->pam->SetPackageFlag(RE::PACKAGE_DATA::GeneralFlag::kAlwaysSneak, false);
					}

					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionSneak, a_p->coopActor.get());
					a_p->coopActor->actorState1.sneaking = 0;
					a_p->coopActor->actorState2.forceSneak = 0;
				}
				else
				{
					// Ensure package flags are in sync for co-op companions.
					if (!a_p->isPlayer1)
					{
						a_p->pam->SetPackageFlag(RE::PACKAGE_DATA::GeneralFlag::kAlwaysSneak, true);
					}

					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionSneak, a_p->coopActor.get());
					a_p->coopActor->actorState1.sneaking = 1;
					a_p->coopActor->actorState2.forceSneak = 1;
				}
			}
		}

		void Sprint(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Start sprinting if not paragliding or using M.A.R.F.
			// If sneaking with the silent roll perk, start rolling.
			// If blocking, start shield charge.
			// Otherwise, start sprint animation.
			// And finally, if paragliding or using M.A.R.F, trigger gale spell.

			if (!a_p->mm->isParagliding && !a_p->tm->isMARFing) 
			{
				if (a_p->coopActor->IsSneaking() || a_p->pam->IsPerforming(InputAction::kBlock)) 
				{
					// Trigger silent roll or shield charge animation.
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionSprintStart, a_p->coopActor.get());
				}
				else
				{
					// Stop attacking before sprinting/dodging.
					a_p->coopActor->NotifyAnimationGraph("attackStop");
					// Sprint start action fails to modify P1's mount speedmult.
					// Since the sprint start/stop actions do not function for P1's mount while motion driven,
					// we trigger the mounted player actor sprint animation (hunched over)
					// and modify the mount's speed separately in the Update() hook.
					// Running the action command fails sometimes, so we directly notify the animation graph instead.
					if (bool succ = a_p->coopActor->NotifyAnimationGraph("SprintStart"); succ)
					{
						// Turn to face the player's movement direction to prevent weird momentum effects
						// which occur during movement type switching when strafing and turning
						// to face the movement direction at the same time.
						a_p->coopActor->data.angle.z = Util::NormalizeAng0To2Pi(a_p->mm->movementOffsetParams[!MoveParams::kLSGameAng]);
					}
				}

				// Sprint stamina expenditure starts now.
				a_p->expendSprintStaminaTP = SteadyClock::now();
			}
			else if (glob.tarhielsGaleSpell && glob.player1Actor->HasSpell(glob.tarhielsGaleSpell))
			{
				// Conveniently cast gale spell (if known by P1) when paragliding/M.A.R.F-ing.
				if (auto instantCaster = a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant); instantCaster) 
				{
					instantCaster->CastSpellImmediate(glob.tarhielsGaleSpell, false, a_p->coopActor.get(), 1.0f, false, 1.0f, a_p->coopActor.get());
				}
			}
		}

		void SprintPowerAttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Power attack with LH object/fists while sprinting.

			// Doesn't trigger consistently unless already performing a LH attack.
			Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
			Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftPowerAttack, a_p->coopActor.get());
		}

		void SprintPowerAttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Power attack with RH object/fists while sprinting.

			// Same reasoning as with LH sprint power attack above.
			Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get());
			Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightPowerAttack, a_p->coopActor.get());
		}

		void StatsMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the StatsMenu if playing Skyrim.
			// Otherwise, open the Hero Menu or a UIExtensions stats menu if playing Enderal.

			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
			{
				// Check if Maxsu2017's 'Hero Menu Enhanced' mod is installed, 
				// and trigger the StatsMenu as usual to open the enhanced Hero Menu.
				if (bool enhancedHeroMenuAvailable = static_cast<bool>(GetModuleHandleA("EnderalHeroMenu.dll")); enhancedHeroMenuAvailable) 
				{
					auto ue = RE::UserEvents::GetSingleton();
					auto controlMap = RE::ControlMap::GetSingleton();
					if (!ue || !controlMap)
					{
						return;
					}

					auto keyCode = controlMap->GetMappedKey(glob.paInfoHolder->ACTIONS_TO_P1_UE_STRINGS.at(!InputAction::kStatsMenu), RE::INPUT_DEVICE::kKeyboard);
					if (keyCode != 255)
					{
						// Is bound, open enhanced Hero Menu.
						//HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kStatsMenu);
						bool succ = glob.moarm->InsertRequest(a_p->controllerID, InputAction::kStatsMenu, SteadyClock::now(), GlobalCoopData::ENHANCED_HERO_MENU);
						// If request was successfully inserted, open the requested menu.
						if (succ)
						{
							a_p->pam->SendButtonEvent(InputAction::kStatsMenu, RE::INPUT_DEVICE::kKeyboard, ButtonEventPressType::kInstantTrigger, 0.0f, true);
						}

						return;
					}
					else
					{
						// Quick Stats not bound -> open UIExtensions stats menu instead.
						glob.onCoopHelperMenuRequest.SendEvent(a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kStats);
					}
				}
				else
				{
					// NOTE: Commented out for now. Potential support for vanilla Hero Menu in the future.
					// Doesn't consistently open the vanilla Enderal Hero Menu at the moment.
					/*
					if (a_p->isPlayer1)
					{
						// Attempt to find the hotkey for opening the Hero menu by reading an Enderal MCM script property, 
						// and then open it using the hotkey.
						if (auto skyrimVM = RE::SkyrimVM::GetSingleton(); skyrimVM->impl && skyrimVM->impl.get())
						{
							const auto& vm = skyrimVM->impl;
							if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
							{
								auto levelSystemQuest = RE::TESForm::LookupByID(0x10AA2)->As<RE::TESQuest>();
								if (levelSystemQuest)
								{
									// Find player alias.
									RE::BGSBaseAlias* playerAlias = nullptr;
									for (const auto alias : levelSystemQuest->aliases) 
									{
										if (alias->aliasName == "Player"sv) 
										{
											playerAlias = alias;
											break;
										}
									}

									if (playerAlias && vm->GetObjectHandlePolicy()) 
									{
										auto handle = vm->GetObjectHandlePolicy()->GetHandleForObject(playerAlias->GetVMTypeID(), playerAlias);
										RE::BSTSmartPointer<RE::BSScript::Object> objectPtr;
										vm->FindBoundObject(handle, "_00E_Game_SkillmenuSC", objectPtr);
										if (objectPtr && objectPtr.get())
										{
											auto heroMenuDXSCVar = objectPtr->GetVariable("iHeroMenuKeycode");
											if (heroMenuDXSCVar)
											{
												uint32_t heroMenuDXSC = heroMenuDXSCVar->GetUInt();
												Util::SendButtonEvent(RE::INPUT_DEVICE::kKeyboard, "HeroMenu"sv, heroMenuDXSC, 1.0f, 0.0f, true, false);
												Util::SendButtonEvent(RE::INPUT_DEVICE::kKeyboard, "HeroMenu"sv, heroMenuDXSC, 0.0f, 1.0f, true, false);
											}
										}
									}
									
								}
							}
						}
					}
					else
					{
						glob.onCoopHelperMenuRequest.SendEvent(a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kStats);
					}
					*/

					// Signal script to open UIExtensions stats menu.
					glob.onCoopHelperMenuRequest.SendEvent(a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kStats);
				}
			}
			else
			{
				// Open the StatsMenu.
				// Tween Menu bind opens the Stats Menu directly when P1 is transformed.
				if (glob.coopPlayers[glob.player1CID]->isTransformed) 
				{
					HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kTweenMenu);
				}
				else
				{
					HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kStatsMenu);
				}
			}
		}

		void TeleportToPlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Teleport to another active player.
			
			// Only need to send menu-open request if there are more than two players.
			bool noPlayerInMenus = Util::MenusOnlyAlwaysOpen();
			if (glob.activePlayers > 2 && noPlayerInMenus) 
			{
				glob.onCoopHelperMenuRequest.SendEvent(a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kTeleport);
			}
			else if (glob.activePlayers > 2)
			{
				// Teleport to the player that this player is facing if another player is controlling menus.
				// (can't open player selection menu).
				RE::Actor* facingPlayer = nullptr;
				float minFacingDiff = FLT_MAX;
				for (const auto& p : glob.coopPlayers) 
				{
					if (p->isActive && p->coopActor != a_p->coopActor) 
					{
						float facingDiff = fabsf(Util::NormalizeAngToPi(Util::GetYawBetweenRefs(a_p->coopActor.get(), p->coopActor.get()) - a_p->coopActor->GetHeading(false)));
						if (facingDiff < minFacingDiff) 
						{
							facingPlayer = p->coopActor.get();
							minFacingDiff = facingDiff;
						}
					}
				}

				if (facingPlayer) 
				{
					a_p->taskRunner->AddTask(
						[facingPlayer, a_p]() 
						{
							a_p->TeleportTask(facingPlayer->GetHandle()); 
						}
					);
				}
			}
			else
			{
				// Only two active players.
				// Get the other active player to teleport to.
				RE::Actor* otherPlayer = nullptr;
				for (const auto& p : glob.coopPlayers)
				{
					if (p->isActive && p->coopActor != a_p->coopActor)
					{
						otherPlayer = p->coopActor.get();
						break;
					}
				}

				if (otherPlayer)
				{
					a_p->taskRunner->AddTask(
						[otherPlayer, a_p]() 
						{
							SPDLOG_DEBUG("[PAFH] PreStartTask: {}: Target P pos: ({}, {}, {}) in cell {}, {} pos: ({}, {}, {}) in cell {}.",
								otherPlayer->GetName(),
								otherPlayer->data.location.x, otherPlayer->data.location.y, otherPlayer->data.location.z,
								otherPlayer->GetParentCell() ? otherPlayer->GetParentCell()->GetName() : "NONE",
								a_p->coopActor->GetName(),
								a_p->coopActor->data.location.x, a_p->coopActor->data.location.y, a_p->coopActor->data.location.z,
								a_p->coopActor->GetParentCell() ? a_p->coopActor->GetParentCell()->GetName() : "NONE");
							a_p->TeleportTask(otherPlayer->GetHandle()); 
						}
					);
				}
			}
		}

		void TradeWithPlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// If there are more than 2 active players, the UIExtensions character selection menu will open,
			// allowing this player to choose a player to trade with.
			// Otherwise, directly open the GiftMenu to commence trading.

			glob.onCoopHelperMenuRequest.SendEvent(a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kTrade);
		}

		void TweenMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the TweenMenu.

			HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kTweenMenu);
		}

		void WaitMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the WaitMenu.

			HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kWaitMenu);
		}

		void ZoomCam(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Adjust camera zoom if camera control is given.

			HelperFuncs::SetCameraAdjustmentMode(a_p->controllerID, InputAction::kZoomCam, true);
		}
	};

	namespace CleanupFuncs
	{
		void Activate(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Activate the targeted/cycled refr on release of the Activate bind.
			
			// No refr activation if paragliding was requested earlier.
			if (a_p->pam->requestedToParaglide)
			{
				return;
			}

			if (a_p->pam->downedPlayerTarget)
			{
				// NOTE: Have to stop revive idle and re-equip weapons/magic to prevent bugged
				// animation state where weapons are in hand but unusable.
				if (!a_p->coopActor->IsOnMount() && !a_p->coopActor->IsSwimming() && !a_p->coopActor->IsFlying())
				{
					a_p->coopActor->NotifyAnimationGraph("IdleStop");
					a_p->coopActor->NotifyAnimationGraph("IdleForceDefaultState");
					if (a_p->coopActor->IsWeaponDrawn()) 
					{
						a_p->pam->ReadyWeapon(true);
					}
				}

				// Stop all revive effects.
				Util::StopEffectShader(a_p->coopActor.get(), glob.dragonHolesShader, 2.0f);
				Util::StopEffectShader(a_p->pam->downedPlayerTarget->coopActor.get(), glob.dragonSoulAbsorbShader, 2.0f);
				Util::StopHitArt(a_p->pam->downedPlayerTarget->coopActor.get(), glob.reviveDragonSoulEffect, 2.0f);
				Util::StopHitArt(a_p->pam->downedPlayerTarget->coopActor.get(), glob.reviveHealingEffect, 2.0f);

				// Set being revived to false, and then clear the downed player target. 
				// Done to allow for a new downed target when the action is restarted.
				a_p->pam->downedPlayerTarget->isBeingRevived = false;
				a_p->pam->downedPlayerTarget = nullptr;
				// No longer reviving.
				a_p->isRevivingPlayer = false;
			}
			else 
			{
				// Only attempt activation on input release, not if Activate was interrupted.
				const auto& perfStage = a_p->pam->paStatesList[!InputAction::kActivate - !InputAction::kFirstAction].perfStage;
				if (perfStage != PerfStage::kInputsReleased && perfStage != PerfStage::kSomeInputsReleased) 
				{
					return;
				}

				auto p1 = RE::PlayerCharacter::GetSingleton();
				auto activationRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle);
				auto activationRefrValidity = activationRefrPtr && Util::IsValidRefrForTargeting(activationRefrPtr.get());
				// Both P1 and the refr to activate must be valid. Return early otherwise.
				if (!p1 || !activationRefrValidity)
				{
					return;
				}

				// Get saved 'can activate' flag from cycling earlier.
				// If the player cannot activate the chosen refr, return.
				const auto& canActivate = a_p->tm->canActivateRefr;
				if (!canActivate)
				{
					return;
				}

				// Clear grabbed/released refr if activating it, as its 3D will be removed once picked up
				// and will no longer need to be tracked in the targeting manager.
				if (a_p->tm->rmm->IsManaged(a_p->tm->activationRefrHandle, true) ||
					a_p->tm->rmm->IsManaged(a_p->tm->activationRefrHandle, false))
				{
					a_p->tm->rmm->ClearRefr(a_p->tm->activationRefrHandle);
				}

				// Check if another player is controlling menus.
				// This influences what objects this player can activate --
				// nothing that will open a menu if another player is controlling menus.
				bool menusOnlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
				bool anotherPlayerControllingMenus = !GlobalCoopData::CanControlMenus(a_p->controllerID);

				// Get count.
				uint32_t count = 1;
				auto extraCount = activationRefrPtr->extraList.GetByType<RE::ExtraCount>();
				if (extraCount)
				{
					count = extraCount->count;
				}

				// Get base object to get refr type and for the activate refr call.
				auto baseObj = activationRefrPtr->GetBaseObject();
				if (a_p->isPlayer1)
				{
					bool lootable = Util::IsLootableRefr(activationRefrPtr.get());
					if (menusOnlyAlwaysOpen)
					{
						// No player is controlling menus.
						RE::Actor* asActor = activationRefrPtr->As<RE::Actor>();
						if (asActor && asActor->IsAMount() && !asActor->IsDead())
						{
							// Attempt to mount.
							a_p->SetTargetedMount(asActor->GetHandle());
							glob.moarm->InsertRequest(a_p->controllerID, InputAction::kActivate, SteadyClock::now(), "", a_p->tm->activationRefrHandle);
							a_p->taskRunner->AddTask(
								[a_p]() 
								{ 
									a_p->MountTask(); 
								}
							);
						}
						else
						{
							// Activate the refr after sending an activation request to give P1 control 
							// if any menus triggered by activation open afterward.
							// Toggle AI driven off while activating.
							Util::SetPlayerAIDriven(false);
							glob.moarm->InsertRequest(a_p->controllerID, InputAction::kActivate, SteadyClock::now(), "", a_p->tm->activationRefrHandle);
							Util::ActivateRef(activationRefrPtr.get(), p1, 0, baseObj, count, false);
							Util::SetPlayerAIDriven(true);
						}
					}
					else if (anotherPlayerControllingMenus && lootable)
					{
						// Can still loot refrs when another player is controlling menus.
						a_p->coopActor->PickUpObject(activationRefrPtr.get(), count);
					}
				}
				else
				{
					// Have P1 activate refrs that can trigger menus or shared items.
					bool p1Activate = false;
					// A crime to activate.
					bool offLimits = activationRefrPtr->IsCrimeToActivate();
					RE::Actor* asActor = activationRefrPtr->As<RE::Actor>();
					// Trying to pickpocket an actor.
					bool attemptingToPickpocket = asActor && asActor->CanPickpocket() && a_p->coopActor->IsSneaking();
					// Is this player running the special interaction package to interact with targeted furniture?
					bool inInteractionPackage = false;
					if (baseObj)
					{
						bool lootable = Util::IsLootableRefr(activationRefrPtr.get());
						bool isActivator = baseObj->Is(RE::FormType::Activator, RE::FormType::TalkingActivator);
						bool isBook = baseObj->IsBook();
						bool isPartyWideItem = Util::IsPartyWideItem(baseObj);
						// Has extra data with an associated quest.
						// Might not necessarily be a quest item, but better safe than sorry.
						bool isQuestItem = 
						{
							activationRefrPtr->extraList.HasType(RE::ExtraDataType::kAliasInstanceArray) ||
							activationRefrPtr->extraList.HasType(RE::ExtraDataType::kFromAlias)
						};

						// Unread skill/spell books are read right away by player 1 here, 
						// since, once read, all players' skills increase or the learned spell is usable by all players.
						// Other P1-activated items:
						// Gold, lockpicks, all regular books, notes, keys, activators, workbenches, 
						// non-lootable items that aren't furniture, doors, locked objects, dead objects, containers, and non-mount actors.
						bool isSkillBook = isBook && baseObj->As<RE::TESObjectBOOK>()->TeachesSkill();
						bool isSpellBook = isBook && baseObj->As<RE::TESObjectBOOK>()->TeachesSpell();
						bool isUnreadSkillbook = isSkillBook && !baseObj->As<RE::TESObjectBOOK>()->IsRead();
						bool isUnreadSpellbook = isSpellBook && !baseObj->As<RE::TESObjectBOOK>()->IsRead();
						bool isWorkBenchFurniture = 
						(
							baseObj->Is(RE::FormType::Furniture) && 
							baseObj->As<RE::TESFurniture>()->workBenchData.benchType.get() != RE::TESFurniture::WorkBenchData::BenchType::kNone
						);
						p1Activate = 
						(
							(isActivator || isPartyWideItem || isQuestItem || isWorkBenchFurniture || isUnreadSkillbook || isUnreadSpellbook || 
							(isBook && !isSkillBook && !isSpellBook) ||
							(!lootable && baseObj->IsNot(RE::FormType::Furniture) &&
								(baseObj->Is(RE::FormType::Door) ||
								activationRefrPtr->IsLocked() ||
								activationRefrPtr->IsDead() ||
								(!asActor && activationRefrPtr->HasContainer()) ||
								(asActor && !asActor->IsAMount())))) 
						);

						// Start furniture interaction, if needed.
						inInteractionPackage = HelperFuncs::InitiateSpecialInteractionPackage(a_p, activationRefrPtr.get(), baseObj);
					}

					if (asActor && asActor->IsAMount() && !asActor->IsDead())
					{
						// Attempt to mount.
						a_p->SetTargetedMount(asActor->GetHandle());
						glob.moarm->InsertRequest(a_p->controllerID, InputAction::kActivate, SteadyClock::now(), "", asActor->GetHandle());
						a_p->taskRunner->AddTask
						(
							[a_p]() 
							{ 
								a_p->MountTask(); 
							}
						);
					}
					else
					{
						SPDLOG_DEBUG("[PAFH] {} is activating {} ({}, form type 0x{:X}, base form type: 0x{:X}, count: {})",
							a_p->coopActor->GetName(), activationRefrPtr->GetName(),
							p1Activate ? "via P1" : "via self",
							activationRefrPtr->GetFormType(),
							*baseObj->formType,
							extraCount ? extraCount->count : -1);

						// Will get caught stealing when activating while fully detected. 
						// Alert nearby actors of P1.
						if (a_p->tm->detectionPct == 100.0f && (offLimits || attemptingToPickpocket))
						{
							p1->currentProcess->SetActorsDetectionEvent(p1, p1->data.location, 1, p1);
							p1->currentProcess->high->detectAlert = true;

							// Credits to Ryan for the original ConsoleUtilSSE source which has since been taken down.
							// Credits to lfrazer for the VR fork of that mod, source here:
							// https://github.com/lfrazer/ConsoleUtilVR/blob/master/src/Papyrus.cpp
							const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
							const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
							if (script)
							{
								script->SetCommand(fmt::format("SendStealAlarm {:X}", p1->formID));
								script->CompileAndRun(activationRefrPtr.get());
								delete script;
							}
						}

						if (p1Activate)
						{
							// Can only activate with player 1 if no player is controlling menus.
							if (menusOnlyAlwaysOpen)
							{
								// Trick game into thinking that P1 is sneaking during activation to trigger the pickpocketing menu when targeting NPCs.
								// Match P1 and this players' sneak states briefly before and during activation.
								bool toggleP1SneakOn = a_p->coopActor->IsSneaking() && !p1->IsSneaking();
								bool toggleP1SneakOff = !a_p->coopActor->IsSneaking() && p1->IsSneaking();
								auto savedP1SitSleepState = p1->GetSitSleepState();
								bool isFurniture = activationRefrPtr->As<RE::TESFurniture>() || (baseObj && baseObj->As<RE::TESFurniture>());
								RE::ObjectRefHandle prevFurniture{};

								if (toggleP1SneakOn)
								{
									p1->actorState1.sneaking = 1;
								}
								else if (toggleP1SneakOff)
								{
									p1->actorState1.sneaking = 0;
								}

								// Prevent Crafting/Smithing or other only-P1-triggerable menus from opening if P1 is too far away.
								if (inInteractionPackage && p1->data.location.GetDistance(a_p->coopActor->data.location) > a_p->tm->GetMaxActivationDist())
								{
									a_p->tm->SetCrosshairMessageRequest(
										CrosshairMessageType::kGeneralNotification,
										fmt::format("P{}: P1 is too far away to interact with {}.", a_p->playerID + 1, activationRefrPtr->GetName()),
										{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
										Settings::fSecsBetweenDiffCrosshairMsgs);
								}
								else
								{
									// Set P1 as motion driven before activating.
									Util::SetPlayerAIDriven(false);
									// Activate the refr after sending an activation request to give this player control
									// if any menus triggered by activation open afterward.
									glob.moarm->InsertRequest(a_p->controllerID, InputAction::kActivate, SteadyClock::now(), "", activationRefrPtr->GetHandle());
									Util::ActivateRef(activationRefrPtr.get(), p1, 0, baseObj, count, false);
									Util::SetPlayerAIDriven(true);
								}


								// Reset P1 sneak state to what it was before activation.
								if (toggleP1SneakOn)
								{
									p1->actorState1.sneaking = 0;
								}
								else if (toggleP1SneakOff)
								{
									p1->actorState1.sneaking = 1;
								}
							}
						}
						else
						{
							// Activate the refr after sending an activation request to give this player control
							// if any menus triggered by activation open afterward.
							glob.moarm->InsertRequest(a_p->controllerID, InputAction::kActivate, SteadyClock::now(), "", a_p->tm->activationRefrHandle);
							// Show in TrueHUD recent loot widget.
							if (Util::IsLootableObject(*baseObj) && ALYSLC::TrueHUDCompat::g_trueHUDInstalled)
							{
								p1->AddObjectToContainer(baseObj, nullptr, count, p1);
								p1->RemoveItem(baseObj, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
							}

							Util::ActivateRef(activationRefrPtr.get(), a_p->coopActor.get(), 0, baseObj, count, false);
						}
					}
				}
			}
		}

		void AttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Attack cleanup for LH weapon/fists.

			if (!a_p->coopActor->IsWeaponDrawn())
			{
				// Unsheathe if not rotating arms.
				// Do nothing otherwise.
				if (!Settings::bRotateArmsWhenSheathed) 
				{
					a_p->pam->ReadyWeapon(true);
				}

				return;
			}

			// Stop sneak animation if currently dash dodging so that sneak attacks
			// are not triggered when attacking here.
			if (a_p->mm->isDashDodging)
			{
				a_p->coopActor->NotifyAnimationGraph("SneakStop");
			}

			// Check if a killmove should be played first.
			bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kAttackLH);
			if (!performingKillmove) 
			{
				if (a_p->pam->isSprinting || a_p->coopActor->IsSprinting()) 
				{
					// Perform LH sprint attack.
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
				}
				else if (a_p->em->Has2HRangedWeapEquipped())
				{
					auto attackState = a_p->coopActor->actorState1.meleeAttackState;
					// Performing LH release or attack stop when a crossbow-type weapon 
					// is in the process of firing chambers the next bolt and forgoes the reload animation, 
					// which allows for a much faster firerate than intended.
					// Only cancel firing of bolt if not reloading.
					if (attackState == RE::ATTACK_STATE_ENUM::kNone)
					{
						Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
						Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get());
					}

					// If using a bow, un-nock drawn arrow.
					if (attackState == RE::ATTACK_STATE_ENUM::kBowDraw || attackState == RE::ATTACK_STATE_ENUM::kBowDrawn)
					{
						a_p->coopActor->NotifyAnimationGraph("attackStop");
					}
				}
				else if (a_p->em->HasLHStaffEquipped())
				{
					a_p->pam->CastStaffSpell(a_p->em->GetLHWeapon(), true, false);
					// TODO: Manually update charge for P1.
					// Currently gets overidden by the game once the inventory menu is opened,
					// and not all staves have their charges updated by the game when used.
				}
				else
				{
					// Generic attack and release.
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get());
				}
			}
		}

		void AttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Attack cleanup for RH weapon/fists.

			if (!a_p->coopActor->IsWeaponDrawn())
			{
				// Unsheathe if not rotating arms.
				// Do nothing otherwise.
				if (!Settings::bRotateArmsWhenSheathed)
				{
					a_p->pam->ReadyWeapon(true);
				}

				return;
			}

			// Stop sneak animation if currently dash dodging so that sneak attacks
			// are not triggered when attacking here.
			if (a_p->mm->isDashDodging)
			{
				a_p->coopActor->NotifyAnimationGraph("SneakStop");
			}

			// Check if a killmove should be played first.
			bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kAttackRH);
			if (!performingKillmove)
			{
				if (a_p->pam->isSprinting || a_p->coopActor->IsSprinting())
				{
					// Perform RH sprint attack.
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get());
				}
				else if (a_p->em->Has2HRangedWeapEquipped())
				{
					// Bows/crossbows require toggling AI driven and only sending on press and release.
					if (a_p->isPlayer1) 
					{
						// Actual held time used this time on release.
						a_p->pam->SendButtonEvent(InputAction::kAttackRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease);
					}
					else
					{
						Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightRelease, a_p->coopActor.get());
					}
				}
				else if (a_p->em->HasRHStaffEquipped())
				{
					a_p->pam->CastStaffSpell(a_p->em->GetRHWeapon(), false, false);
					// TODO: Manually update charge for P1.
					// Currently gets overidden by game once the inventory menu is opened,
					// and not all staves have their charges updated by the game when used.
				}
				else
				{
					// Generic attack and release.
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get());
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightRelease, a_p->coopActor.get());
				}
			}
		}

		void Bash(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Bash with the equipped LH weapon/shield.

			// Only bash if a power bash was not performed already while the bind was held.
			if (a_p->pam->GetSecondsSinceLastStart(InputAction::kBash) <= Settings::fSecsDefMinHoldTime ||
				!a_p->coopActor->HasPerk(glob.powerBashPerk) || !a_p->pam->isBashing)
			{
				// Check if a killmove should be played first.
				bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kBash);
				if (!performingKillmove)
				{
					if (a_p->em->Has2HRangedWeapEquipped())
					{
						// Bash only if not at some stage of an attack.
						auto attackState = a_p->coopActor->actorState1.meleeAttackState;
						if (attackState == RE::ATTACK_STATE_ENUM::kNone)
						{
							Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
							Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get());
						}
					}
					else
					{
						// Ensure that the player actor continues blocking afterward if they were already
						// blocking before the bash request.
						bool wasBlocking = a_p->pam->IsPerforming(InputAction::kBlock);
						
						if (a_p->isPlayer1)
						{
							// Hello Ed boys! Many input events, yes?
							// Emulate when happens when the player bashes with the default Skyrim controls:
							// Hold Block + press Right Attack + release Block.
							a_p->pam->QueueP1ButtonEvent(InputAction::kBlock, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kInstantTrigger);
							a_p->pam->QueueP1ButtonEvent(InputAction::kBlock, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 0.1f, true);
							a_p->pam->QueueP1ButtonEvent(InputAction::kAttackRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kInstantTrigger);
							a_p->pam->QueueP1ButtonEvent(InputAction::kAttackRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 0.1f, true);
							a_p->pam->QueueP1ButtonEvent(InputAction::kAttackRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease, 0.1f, true);
							a_p->pam->QueueP1ButtonEvent(InputAction::kBlock, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease, 0.1f, true);
							// Send straight away in one chain.
							a_p->pam->ChainAndSendP1ButtonEvents();
							// Ensure P1 is AI driven again after.
							a_p->pam->sendingP1MotionDrivenEvents = false;
							Util::SetPlayerAIDriven(true);

							// Redundancy since block start/stop requests fail at times.
							if (wasBlocking)
							{
								Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
								a_p->coopActor->NotifyAnimationGraph("blockStart");
							}
							else
							{
								Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get());
								a_p->coopActor->NotifyAnimationGraph("attackStop");
								a_p->coopActor->NotifyAnimationGraph("blockStop");
							}
						}
						else
						{
							// Triggered via action command.
							Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
							Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get());

							// Redundancy since block start/stop requests fail at times.
							if (wasBlocking)
							{
								Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get());
								a_p->coopActor->NotifyAnimationGraph("blockStart");
							}
							else
							{
								Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get());
								a_p->coopActor->NotifyAnimationGraph("attackStop");
								a_p->coopActor->NotifyAnimationGraph("blockStop");
							}
						}
					}
				}
			}
		}

		void Block(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Stop blocking.

			if (a_p->isPlayer1)
			{
				// For Valhalla Combat projectile deflection compatibility, which requires the block input event.
				a_p->pam->QueueP1ButtonEvent(InputAction::kBlock, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease, 0.1f, false);
			}

			Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get());
			// Extra animation event sent to ensure P1 stops blocking, since 
			// getting hit while blocking and then releasing block does not always work.
			a_p->coopActor->NotifyAnimationGraph("blockStop");
			// Stop sprinting to ensure any active shield charge also ends.
			Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionSprintStop, a_p->coopActor.get());
		}

		void CastLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset LH casting variables, remove ranged package, and evaluate defualt package to stop LH casting.
			
			// Check if a killmove should be played first.
			bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kCastLH);
			if (!performingKillmove)
			{
				if (a_p->isPlayer1)
				{
					// Release the LH cast bind.
					a_p->pam->QueueP1ButtonEvent(InputAction::kCastLH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease, 0.0f, false);
				}
				else
				{
					// Remove casting package after resetting casting state.
					auto lhSpell = a_p->em->GetLHSpell();
					bool is2HSpell = lhSpell && lhSpell->equipSlot == glob.bothHandsEquipSlot;
					if (is2HSpell)
					{
						HelperFuncs::RemoveCastingPackage(a_p, true, true);
					}
					else
					{
						HelperFuncs::RemoveCastingPackage(a_p, true, false);
					}

					// Quit casting with P1 if the spell should've been cast with P1.
					if (bool shouldCastWithP1 = Util::ShouldCastWithP1(lhSpell); shouldCastWithP1)
					{
						a_p->pam->CastSpellWithMagicCaster(EquipIndex::kLeftHand, false, false, shouldCastWithP1);
					}
				}
			}
		}

		void CastRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset RH casting variables, remove ranged package, and evaluate defualt package to stop RH casting.
			
			// Check if a killmove should be played first.
			bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kCastRH);
			if (!performingKillmove)
			{
				if (a_p->isPlayer1)
				{
					// Release the RH cast bind.
					a_p->pam->QueueP1ButtonEvent(InputAction::kCastRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease, 0.0f, false);
				}
				else
				{
					// Remove casting package after resetting casting state.
					auto rhSpell = a_p->em->GetRHSpell();
					bool is2HSpell = rhSpell && rhSpell->equipSlot == glob.bothHandsEquipSlot;
					if (is2HSpell)
					{
						HelperFuncs::RemoveCastingPackage(a_p, true, true);
					}
					else
					{
						HelperFuncs::RemoveCastingPackage(a_p, false, true);
					}

					// Quit casting with P1 if the spell should've been cast with P1.
					if (bool shouldCastWithP1 = Util::ShouldCastWithP1(rhSpell); shouldCastWithP1)
					{
						a_p->pam->CastSpellWithMagicCaster(EquipIndex::kRightHand, false, false, shouldCastWithP1);
					}
				}
			}
		}

		void CycleAmmo(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Equip cycled ammo on release.

			if (!Settings::bHoldToCycle)
			{
				// Cycle on release.
				a_p->em->CycleAmmo();
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}
			else
			{
				// Set to previous cycled item if this bind is released during the "grace period",
				// as the input release and crosshair text update aren't instant, and if the player
				// releases the bind very shortly after the last cycling, they'd likely expect
				// the previous cycled item to be applied, since the text would not have updated
				// within a human-reactable interval.
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledAmmo = a_p->em->lastCycledForm;
				}
			}

			if (RE::TESForm* ammo = a_p->em->currentCycledAmmo; ammo)
			{
				// Equip on release.
				a_p->em->EquipAmmo(ammo);
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, ammo->GetName()),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				// Nothing to equip.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: No favorited ammo", a_p->playerID + 1),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleSpellCategoryLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle LH spell category on release.

			if (!Settings::bHoldToCycle)
			{
				a_p->em->CycleHandSlotMagicCategory(false);
			}
			else
			{
				// Set to previous cycled item if this bind is released during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->lhSpellCategory = a_p->em->lastCycledSpellCategory;
				}
			}

			// Notify the player.
			const std::string_view newCategory = a_p->em->FavMagCyclingCategoryToString(a_p->em->lhSpellCategory);
			a_p->tm->SetCrosshairMessageRequest(
				CrosshairMessageType::kEquippedItem,
				fmt::format("P{}: Left hand spell category: '{}'", a_p->playerID + 1, newCategory),
				{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
				Settings::fSecsBetweenDiffCrosshairMsgs
			);;
		}

		void CycleSpellCategoryRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle RH spell category on release.

			if (!Settings::bHoldToCycle)
			{
				a_p->em->CycleHandSlotMagicCategory(true);
			}
			else
			{
				// Set to previous cycled item if this bind is released during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->rhSpellCategory = a_p->em->lastCycledSpellCategory;
				}
			}

			// Notify the player.
			const std::string_view newCategory = a_p->em->FavMagCyclingCategoryToString(a_p->em->rhSpellCategory);
			a_p->tm->SetCrosshairMessageRequest(
				CrosshairMessageType::kEquippedItem,
				fmt::format("P{}: Right hand spell category: '{}'", a_p->playerID + 1, newCategory),
				{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void CycleSpellLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle spell in LH on release.

			if (!Settings::bHoldToCycle) 
			{
				// Cycle on release.
				a_p->em->CycleHandSlotMagic(false);
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}
			else
			{
				// Set to previous cycled item if this bind is released during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledRHSpellsList[!a_p->em->lhSpellCategory] = a_p->em->lastCycledForm;
				}
			}

			// Get cycled spell from current category.
			if (RE::TESForm* spellForm = a_p->em->currentCycledLHSpellsList[!a_p->em->lhSpellCategory]; spellForm)
			{
				RE::SpellItem* spell = spellForm->As<RE::SpellItem>();
				bool is2HSpell = spell->equipSlot == glob.bothHandsEquipSlot;
				// Equip on release.
				a_p->em->EquipSpell(spell, EquipIndex::kLeftHand, is2HSpell ? glob.bothHandsEquipSlot : glob.leftHandEquipSlot);
				// Notify the player.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, spell->GetName()),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (!a_p->em->HasCyclableSpellInCategory(a_p->em->lhSpellCategory))
			{
				// No spell to equip.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: '{}' category is empty", a_p->playerID + 1, a_p->em->FavMagCyclingCategoryToString(a_p->em->lhSpellCategory)),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleSpellRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle spell in RH on release.

			if (!Settings::bHoldToCycle)
			{
				// Cycle on release.
				a_p->em->CycleHandSlotMagic(true);
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}
			else
			{
				// Set to previous cycled item if this bind is released during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledRHSpellsList[!a_p->em->rhSpellCategory] = a_p->em->lastCycledForm;
				}
			}

			// Get cycled spell from current category.
			if (RE::TESForm* spellForm = a_p->em->currentCycledRHSpellsList[!a_p->em->rhSpellCategory]; spellForm)
			{
				RE::SpellItem* spell = spellForm->As<RE::SpellItem>();
				bool is2HSpell = spell->equipSlot == glob.bothHandsEquipSlot;
				// Equip on release.
				a_p->em->EquipSpell(spell, EquipIndex::kRightHand, is2HSpell ? glob.bothHandsEquipSlot : glob.rightHandEquipSlot);
				// Notify the player.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, spell->GetName()),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (!a_p->em->HasCyclableSpellInCategory(a_p->em->rhSpellCategory))
			{
				// No spell to equip.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: '{}' category is empty", a_p->playerID + 1, a_p->em->FavMagCyclingCategoryToString(a_p->em->rhSpellCategory)),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleVoiceSlotMagic(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle voice magic (shouts + powers) on release.

			if (!Settings::bHoldToCycle)
			{
				// Cycle on release.
				a_p->em->CycleVoiceSlotMagic();
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}
			else
			{
				// Set to previous cycled item if this bind is released during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledVoiceMagic = a_p->em->lastCycledForm;
				}
			}

			// Get cycled spell from current category.
			if (RE::TESForm* voiceForm = a_p->em->currentCycledVoiceMagic; voiceForm)
			{
				// Equip on release.
				if (voiceForm->As<RE::SpellItem>()) 
				{
					a_p->em->EquipSpell(voiceForm, EquipIndex::kVoice, glob.voiceEquipSlot);
				}
				else if (voiceForm->As<RE::TESShout>())
				{
					a_p->em->EquipShout(voiceForm);
				}

				// Notify the player.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, voiceForm->GetName()),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				// No shout/power to equip.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: No favorited powers/shouts", a_p->playerID + 1),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleWeaponCategoryLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle LH weapon category on release.

			if (!Settings::bHoldToCycle)
			{
				a_p->em->CycleWeaponCategory(false);
			}
			else
			{
				// Set to previous cycled item if this bind is released during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->lhWeaponCategory = a_p->em->lastCycledWeaponCategory;
				}
			}

			// Notify the player.
			const std::string_view newCategory = a_p->em->FavWeaponCyclingCategoryToString(a_p->em->lhWeaponCategory);
			a_p->tm->SetCrosshairMessageRequest(
				CrosshairMessageType::kEquippedItem,
				fmt::format("P{}: Left hand weapon category: '{}'", a_p->playerID + 1, newCategory),
				{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void CycleWeaponCategoryRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle RH weapon category on release.
			if (!Settings::bHoldToCycle)
			{
				a_p->em->CycleWeaponCategory(true);
			}
			else
			{
				// Set to previous cycled item if this bind is released during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->rhWeaponCategory = a_p->em->lastCycledWeaponCategory;
				}
			}

			// Notify the player.
			const std::string_view newCategory = a_p->em->FavWeaponCyclingCategoryToString(a_p->em->rhWeaponCategory);
			a_p->tm->SetCrosshairMessageRequest(
				CrosshairMessageType::kEquippedItem,
				fmt::format("P{}: Right hand weapon category: '{}'", a_p->playerID + 1, newCategory),
				{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void CycleWeaponLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle weapon in LH on release.

			if (!Settings::bHoldToCycle)
			{
				// Cycle on release.
				a_p->em->CycleWeapons(false);
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}
			else
			{
				// Set to previous cycled item if this bind is released during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval / 2.0f, 0.5f))
				{
					a_p->em->currentCycledLHWeaponsList[!a_p->em->lhWeaponCategory] = a_p->em->lastCycledForm;
				}
			}

			// Get cycled spell from current category.
			if (RE::TESForm* form = a_p->em->currentCycledLHWeaponsList[!a_p->em->lhWeaponCategory]; form)
			{
				// Equip on release.
				// Shield and torch go in the left hand.
				if (form->As<RE::TESObjectARMO>() && form->As<RE::TESObjectARMO>()->IsShield()) 
				{
					a_p->em->EquipArmor(form);
				}
				else if (form->As<RE::TESObjectLIGH>() && form->As<RE::TESObjectLIGH>()->data.flags.all(RE::TES_LIGHT_FLAGS::kCanCarry))
				{
					a_p->em->EquipForm(form, EquipIndex::kLeftHand, (RE::ExtraDataList*)nullptr, 1, glob.leftHandEquipSlot);
				}
				else
				{
					RE::TESObjectWEAP* weap = form->As<RE::TESObjectWEAP>();
					bool is2HWeap = weap->equipSlot == glob.bothHandsEquipSlot;
					a_p->em->EquipForm(form, EquipIndex::kLeftHand, (RE::ExtraDataList*)nullptr, 1, is2HWeap ? glob.bothHandsEquipSlot : glob.leftHandEquipSlot);
				}

				// Notify the player.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, form->GetName()),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (a_p->em->lhWeaponCategory == FavWeaponCyclingCategory::kAllFavorites &&
					 a_p->em->HasCyclableWeaponInCategory(FavWeaponCyclingCategory::kAllFavorites, false))
			{
				// Empty both hands if no weapon is returned while cycling through all favorites.
				a_p->em->UnequipHandForms(glob.bothHandsEquipSlot);
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip 'Fists'", a_p->playerID + 1),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (!a_p->em->HasCyclableWeaponInCategory(a_p->em->lhWeaponCategory, false))
			{
				// No weapon to equip.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: '{}' category is empty", a_p->playerID + 1, a_p->em->FavWeaponCyclingCategoryToString(a_p->em->lhWeaponCategory)),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleWeaponRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle weapon in RH on release.

			if (!Settings::bHoldToCycle)
			{
				// Cycle on release.
				a_p->em->CycleWeapons(true);
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}
			else
			{
				// Set to previous cycled item if this bind is released during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledRHWeaponsList[!a_p->em->rhWeaponCategory] = a_p->em->lastCycledForm;
				}
			}

			// Get cycled spell from current category.
			if (RE::TESForm* form = a_p->em->currentCycledRHWeaponsList[!a_p->em->rhWeaponCategory]; form)
			{
				if (form->As<RE::TESObjectWEAP>()) 
				{
					// Equip on release.
					// Shield and torch cannot be cycled in right hand, so no need to handle those cases here.
					RE::TESObjectWEAP* weap = form->As<RE::TESObjectWEAP>();
					bool is2HWeap = weap->equipSlot == glob.bothHandsEquipSlot;
					a_p->em->EquipForm(form, EquipIndex::kRightHand, (RE::ExtraDataList*)nullptr, 1, is2HWeap ? glob.bothHandsEquipSlot : glob.rightHandEquipSlot);
					// Notify the player.
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip '{}'", a_p->playerID + 1, form->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					// Shield and torch cannot be cycled in right hand.
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Cannot equip '{}' in the right hand", a_p->playerID + 1, form->GetName()),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
			}
			else if (a_p->em->rhWeaponCategory == FavWeaponCyclingCategory::kAllFavorites &&
					 a_p->em->HasCyclableWeaponInCategory(FavWeaponCyclingCategory::kAllFavorites, true))
			{
				// Empty both hands if no weapon is returned while cycling through all favorites.
				a_p->em->UnequipHandForms(glob.bothHandsEquipSlot);
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip 'Fists'", a_p->playerID + 1),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (!a_p->em->HasCyclableWeaponInCategory(a_p->em->rhWeaponCategory, true))
			{
				// No weapon to equip.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: '{}' category is empty", a_p->playerID + 1, a_p->em->FavWeaponCyclingCategoryToString(a_p->em->rhWeaponCategory)),
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void GrabObject(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Grab last selected refr if it is valid,
			// the player is not facing the crosshair world position,
			// and the player can grab another refr.
			// Otherwise, release all grabbed refrs if not auto-grabbing.

			auto targetRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
			bool targetRefrValidity = targetRefrPtr && Util::IsValidRefrForTargeting(targetRefrPtr.get());
			bool shouldGrab = 
			{
				targetRefrValidity &&
				!a_p->mm->shouldFaceTarget &&
				a_p->tm->rmm->CanGrabAnotherRefr() &&
				a_p->tm->rmm->CanGrabRefr(a_p->tm->crosshairRefrHandle)
			};
			if (shouldGrab)
			{
				shouldGrab = false;
				// Additional checks for objects targeted by multiple players,
				// or if the targeted object is a actor or player.
				RE::Actor* targetPlayerActor = nullptr;
				bool targetPlayerIsDowned = false;
				bool grabbedByAnotherPlayer = false;
				bool targetActor = targetRefrPtr->As<RE::Actor>();

				for (const auto& p : glob.coopPlayers)
				{
					if (p->isActive && p != a_p)
					{
						// Grabbing another player.
						if (targetRefrPtr == p->coopActor)
						{
							targetPlayerActor = p->coopActor.get();
							targetPlayerIsDowned = p->isDowned;
						}

						// Refr is grabbed by another player.
						if (!grabbedByAnotherPlayer && p->tm->rmm->IsManaged(a_p->tm->crosshairRefrHandle, true))
						{
							grabbedByAnotherPlayer = true;
						}

						// If released by another player, remove this refr from that player's released refr manager,
						// since this player is about to grab it.
						if (p->tm->rmm->IsManaged(a_p->tm->crosshairRefrHandle, false))
						{
							p->tm->rmm->ClearRefr(a_p->tm->crosshairRefrHandle);
						}
					}
				}

				// Actor grab check.
				// Can grab if:
				// 1. Not grabbed by another player.
				// 2. Not an actor.
				// 3. Can grab actors and not targeting a player.
				// 4. Can grab other players and targeting a player -AND-
				//	a. Can grab other players and grabbing a player that is not trying to get up -OR-
				//	b. Can't grab players normally but the target player is downed.
				shouldGrab = 
				{
					(!grabbedByAnotherPlayer) &&
					((!targetActor) || 
						(
							(Settings::bCanGrabActors && !targetPlayerActor) || 
							(
								(targetPlayerActor) &&
								(
									(
										(Settings::bCanGrabOtherPlayers) && 
										(
											targetPlayerActor->IsInRagdollState() ||
											targetPlayerActor->actorState1.knockState != RE::KNOCK_STATE_ENUM::kGetUp
										)
									) || 
									(
										!Settings::bCanGrabOtherPlayers && targetPlayerIsDowned
									)
								)
							)
						)
					)
				};

				// Motion type check.
				if (shouldGrab)
				{
					shouldGrab = false;
					if (auto refr3D = Util::GetRefr3D(targetRefrPtr.get()); refr3D) 
					{
						if (auto hkpRigidBody = Util::GethkpRigidBody(refr3D.get()); hkpRigidBody) 
						{
							// Can only manipulate this object if it has a supported motion type.
							shouldGrab = (hkpRigidBody->motion.type != RE::hkpMotion::MotionType::kFixed &&
										  hkpRigidBody->motion.type != RE::hkpMotion::MotionType::kInvalid);
						}
					}
				}
			}

			// Passed grab checks, so the player will now grab the targeted object.
			if (shouldGrab)
			{
				RE::BSFixedString text = a_p->tm->crosshairTextEntry;
				if (a_p->tm->RefrIsInActivationRange(a_p->tm->crosshairRefrHandle))
				{
					// Notify the player that they are now grabbing the targeted object.
					text = fmt::format("P{}: Grabbing {}", a_p->playerID + 1, targetRefrPtr->GetName());
					a_p->tm->SetIsGrabbing(true);
					a_p->tm->rmm->AddGrabbedRefr(glob.coopPlayers[a_p->controllerID], targetRefrPtr->GetHandle());
				}
				else
				{
					// Notify the player that the targeted object is too far away.
					text = fmt::format("P{}: {} is too far away", a_p->playerID + 1, targetRefrPtr->GetName());
				}

				// Set grabbed object message.
				a_p->tm->SetCrosshairMessageRequest(
					CrosshairMessageType::kActivationInfo,
					text,
					{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
					Settings::fSecsBetweenDiffCrosshairMsgs
				);

				// Return here, since the release check is next.
				return;
			}

			// Check if all grabbed objects should be released 
			// now that it has been established that they cannot grab the targeted object.
			
			// If the player is already grabbing objects and is not auto-grabbing, release all grabbed objects.
			bool shouldRelease = a_p->tm->rmm->isGrabbing && !a_p->tm->rmm->isAutoGrabbing; 
			if (shouldRelease)
			{
				a_p->tm->SetIsGrabbing(false);
			}
		}

		void HotkeyEquip(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Nothing to do on release.
			return;
		}

		void QuickSlotCast(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Stop casting quick slot spell, if the player has one equipped.

			auto quickSlotSpell = a_p->em->quickSlotSpell;
			if (!quickSlotSpell) 
			{
				return;
			}

			// If not a bound weapon spell, stop casting.
			if (auto assocWeap = Util::GetAssociatedBoundWeap(quickSlotSpell); !assocWeap)
			{
				a_p->pam->CastSpellWithMagicCaster(EquipIndex::kQuickSlotSpell, false, false, Util::ShouldCastWithP1(quickSlotSpell));
			}

			// Clear out linked refr target used in ranged attack package.
			a_p->tm->ClearTarget(TargetActorType::kLinkedRefr);
		}

		void RotateCam(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset cam adjustment mode to None, and relinquish control of the camera.

			HelperFuncs::SetCameraAdjustmentMode(a_p->controllerID, InputAction::kRotateCam, false);
		}

		void Shout(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// For P1, release shout bind.
			// For other players, either release the shout not shouting already 
			// and if it isn't on cooldown, or leave a cooldown notification.
			// TODO: Have co-op companion players release their shouts on hold, instead of on release, just like P1.

			if (a_p->isPlayer1)
			{
				// Send input event to emulate pressing the shout key
				// for the same time as the shout bind was held.
				a_p->pam->QueueP1ButtonEvent(InputAction::kShout, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease);
			}
			else if (!a_p->pam->isVoiceCasting)
			{
				// Not already shouting.
				auto voiceForm = a_p->em->voiceForm;
				if (!voiceForm)
				{
					// No equipped voice magic.
					return;
				}

				// Shout/use power if the equipped voice slot form is valid, the player is in god mode,
				// the equipped form is a power, or if the player can shout (cooldown expired).
				if ((a_p->isInGodMode || voiceForm->IsNot(RE::FormType::Shout) || a_p->pam->canShout)) 
				{
					a_p->taskRunner->AddTask([a_p]() { a_p->ShoutTask(); });
				}
				else if (voiceForm->Is(RE::FormType::Shout) && !a_p->pam->canShout)
				{
					// TODO: Replace with bar fill progression and flash in a
					// way that doesn't conflict with player 1's shout cooldown
					// if multiple players shout at once.
					// Can use TrueHUD's special bar for this later on, but
					// updating the crosshair text will serve for now.
					a_p->pam->FlashShoutMeter();
					const RE::BSFixedString shoutCooldownText = 
						fmt::format("P{}: Shout cooldown: {}% ",
						a_p->playerID + 1, static_cast<uint32_t>(100.0f * (a_p->pam->secsSinceLastShout / a_p->pam->secsCurrentShoutCooldown)));

					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kShoutCooldown,
						shoutCooldownText,
						{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
			}
		}

		void SpecialAction(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Perform special actions that trigger on release.
			// Or perform cleanup for single tap/hold actions.

			// Cached requested action from earlier.
			const SpecialActionType& reqAction = a_p->pam->reqSpecialAction;

			// Single tap actions. Mostly cleanup, except for Bash and Transformation.
			if (reqAction == SpecialActionType::kBlock) 
			{
				// Stop blocking instantly by sending anim event request.
				HelperFuncs::BlockInstant(a_p, false);
			}
			else if (reqAction == SpecialActionType::kCastBothHands || reqAction == SpecialActionType::kDualCast)
			{
				if (a_p->isPlayer1)
				{
					// Stop casting with both hands.
					a_p->pam->SendButtonEvent(InputAction::kCastLH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease, 0.0f, false);
					a_p->pam->SendButtonEvent(InputAction::kCastRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease, 0.0f, false);
				}
				else
				{
					if (a_p->em->HasLHSpellEquipped() || a_p->em->HasRHSpellEquipped()) 
					{
						// At least one spell still equipped.
						// (other hand may have a bound weapon now after casting)
						HelperFuncs::RemoveCastingPackage(a_p, true, true);
					}
					else if (a_p->em->HasLHStaffEquipped() && a_p->em->HasRHStaffEquipped())
					{
						// Stop casting with both staves.
						a_p->pam->CastStaffSpell(a_p->em->GetLHWeapon(), true, false);
						a_p->pam->CastStaffSpell(a_p->em->GetRHWeapon(), false, false);
					}
				}
			}
			else if (reqAction == SpecialActionType::kQuickCast)
			{
				// Stop quick slot spell cast.
				CleanupFuncs::QuickSlotCast(a_p);
			}
			else if (reqAction == SpecialActionType::kBash)
			{
				// Start bashing on release.
				HelperFuncs::BashInstant(a_p);
			}
			else if (reqAction == SpecialActionType::kTransformation)
			{
				// Reference for meself.
				// Beast form: WerewolfChange (0x92C48)
				// Vampire lord: DLC1VampireChange (0x200283B)
				// Revert form: DLC1RevertForm (0x200CD5C)

				if (Util::IsWerewolf(a_p->coopActor.get()))
				{
					auto targetActorPtr = Util::GetActorPtrFromHandle(a_p->tm->selectedTargetActorHandle);
					// If no/invalid target or target is alive, show remaining transformation time.
					if (!targetActorPtr || !targetActorPtr->IsDead())
					{
						a_p->tm->SetCrosshairMessageRequest(
							CrosshairMessageType::kGeneralNotification,
							fmt::format("P{}: {:.1f} seconds left as a werewolf!", a_p->playerID + 1, (a_p->secsMaxTransformationTime - Util::GetElapsedSeconds(a_p->transformationTP))),
							{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
							Settings::fSecsBetweenDiffCrosshairMsgs);
						return;
					}
					
					if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) 
					{
						// Ensure target is a corpse within the player's reach and is of a targetable race.
						bool isNPC = targetActorPtr->HasKeyword(glob.npcKeyword);
						// Can target undead and other actors without the NPC keyword.
						auto savageFeedingPerk = dataHandler->LookupForm<RE::BGSPerk>(0x59A6, "Dawnguard.esm");
						bool hasSavageFeedingPerk = savageFeedingPerk && a_p->coopActor->HasPerk(savageFeedingPerk);
						// Is NPC or has savage feeding perk and target is withing range.
						bool validWerewolfFeedingTarget = 
						{
							(isNPC || hasSavageFeedingPerk) &&
							(a_p->coopActor->data.location.GetDistance(targetActorPtr->data.location) <= a_p->em->GetMaxWeapReach())
						};
						if (validWerewolfFeedingTarget)
						{
							// Begin feeding animation.
							Util::PlayIdle("SpecialFeeding", a_p->coopActor.get(), targetActorPtr.get());
							if (auto instantCaster = a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant); instantCaster)
							{
								// Cast victim feed spell to advance P1's Vampire Lord progression.
								bool firstTimeFeeding = false;
								if (auto victimFeedSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfFeedVictimSpell"); victimFeedSpell)
								{
									instantCaster->CastSpellImmediate(victimFeedSpell, false, targetActorPtr.get(), 1.0f, false, 1.0f, a_p->coopActor.get());

									// Check if the victim spell's magic effect was applied, and if so,
									// the corpse has not been fed upon yet and the player can gain health and
									// extend their transformation after feeding.
									if (auto activeEffects = targetActorPtr->GetActiveEffectList(); activeEffects)
									{
										for (auto effect : *activeEffects)
										{
											if (!effect)
											{
												continue;
											}

											if ((effect->spell && effect->spell == victimFeedSpell) || (effect->GetBaseObject() == victimFeedSpell->avEffectSetting))
											{
												firstTimeFeeding = true;
												break;
											}
										}
									}
								}

								if (firstTimeFeeding)
								{
									// Restore health and increment transform time.
									if (auto attackerFeedSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("WerewolfFeed"); attackerFeedSpell)
									{
										instantCaster->CastSpellImmediate(attackerFeedSpell, false, a_p->coopActor.get(), 1.0f, false, 1.0f, a_p->coopActor.get());

										float deltaTransformationTime = 0.0f;
										if (isNPC)
										{
											// Add 30 seconds to the transformation duration.
											deltaTransformationTime = 30.0f;
										}
										else if (hasSavageFeedingPerk)
										{
											// Allows for feeding with half as much transformation time restored on (un)dead, non-humanoid types.
											deltaTransformationTime = 15.0f;
										}

										// Doubles health restored on feeding.
										if (auto gorgingPerk = dataHandler->LookupForm<RE::BGSPerk>(0x59A7, "Dawnguard.esm"); gorgingPerk && a_p->coopActor->HasPerk(gorgingPerk))
										{
											a_p->pam->ModifyAV(RE::ActorValue::kHealth, 100.0f);
										}
										else
										{
											a_p->pam->ModifyAV(RE::ActorValue::kHealth, 50.0f);
										}

										// Update transformation time and display message.
										if (deltaTransformationTime != 0.0f)
										{
											a_p->secsMaxTransformationTime += deltaTransformationTime;
											a_p->tm->SetCrosshairMessageRequest(
												CrosshairMessageType::kGeneralNotification,
												fmt::format("P{}: {:.1f} seconds left as a werewolf after feeding <font color=\"#00FF00\">(+{:.0f})</font>!",
													a_p->playerID + 1, (a_p->secsMaxTransformationTime - Util::GetElapsedSeconds(a_p->transformationTP)), deltaTransformationTime),
												{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
												Settings::fSecsBetweenDiffCrosshairMsgs
											);
										}
									}
								}

								// Lastly, play blood FX.
								if (auto bleedingSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("BleedingSpell"); bleedingSpell)
								{
									instantCaster->CastSpellImmediate(bleedingSpell, false, targetActorPtr.get(), 1.0f, false, 1.0f, a_p->coopActor.get());
								}
							}
						}
					}
				}
				else if (a_p->coopActor->race && !a_p->coopActor->race->GetPlayable() && !Util::IsVampireLord(a_p->coopActor.get()))
				{
					// Show remaining transformation lifetime when transformed into a non-playable race
					// that is not the Werewolf or Vampire Lord races.
					a_p->tm->SetCrosshairMessageRequest(
						CrosshairMessageType::kGeneralNotification,
						fmt::format("P{}: Transformed: {:.1f} seconds left!", a_p->playerID + 1, (a_p->secsMaxTransformationTime - Util::GetElapsedSeconds(a_p->transformationTP))),
						{ CrosshairMessageType::kNone, CrosshairMessageType::kActivationInfo, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
					return;
				}
			}

			// Do not perform OnRelease actions if interrupted or blocked.
			const auto& perfStage = a_p->pam->paStatesList[!InputAction::kSpecialAction - !InputAction::kFirstAction].perfStage;
			if (perfStage != PerfStage::kInputsReleased && perfStage != PerfStage::kSomeInputsReleased)
			{
				return;
			}

			// Double-tap actions.
			if (reqAction == SpecialActionType::kQuickItem)
			{
				// Use quick slot item.
				StartFuncs::QuickSlotItem(a_p);
			}
			else if (reqAction == SpecialActionType::kRagdoll)
			{
				auto isGrabbedIter = std::find_if(glob.coopPlayers.begin(), glob.coopPlayers.end(),
					[&a_p](const auto& p) {
						return p->isActive && p->coopActor != a_p->coopActor && p->tm->rmm->IsManaged(a_p->coopActor->GetHandle(), true);
					});
				if (isGrabbedIter != glob.coopPlayers.end())
				{
					// If grabbed, have the other player release this player first before ragdolling.
					const auto& grabbingP = *isGrabbedIter;
					grabbingP->tm->rmm->ClearRefr(a_p->coopActor->GetHandle());
				}

				// Stop playing emote idle, in case one is playing.
				if (!a_p->coopActor->IsInRagdollState()) 
				{
					a_p->coopActor->NotifyAnimationGraph("SprintStop");
					a_p->coopActor->NotifyAnimationGraph("IdleStop");
					a_p->coopActor->NotifyAnimationGraph("IdleForceDefaultState");
					a_p->pam->isPlayingEmoteIdle = false;
				}

				// Reset fall time and height before ragdolling.
				if (auto charController = a_p->coopActor->GetCharController(); charController)
				{
					charController->fallStartHeight = a_p->coopActor->data.location.z;
					charController->fallTime = 0.0f;
				}

				// Drop on the deck and flop like a fish!
				// Set as grabbed refr to ragdoll the player, clear the grabbed refr, 
				// add as a released refr, and listen for collisions afterward.
				const auto handle = a_p->coopActor->GetHandle();
				a_p->tm->rmm->AddGrabbedRefr(a_p, handle);
				a_p->tm->rmm->ClearGrabbedRefr(handle);
				if (a_p->tm->rmm->GetNumGrabbedRefrs() == 0)
				{
					a_p->tm->SetIsGrabbing(false);
				}

				a_p->tm->rmm->AddReleasedRefr(a_p, handle);
			}
			else if (reqAction == SpecialActionType::kDodge)
			{
				// Start dodging.
				StartFuncs::Dodge(a_p);
			}
			else
			{
				// Only play emote idle if this bind was not held long enough to cycle emote idles, 
				// and if the player is not ragdolled and is staying still.
				if (reqAction == SpecialActionType::kCycleOrPlayEmoteIdle &&
					a_p->pam->GetPlayerActionInputHoldTime(InputAction::kSpecialAction) < max(Settings::fSecsDefMinHoldTime, Settings::fSecsCyclingInterval) &&
					!a_p->coopActor->IsInRagdollState() && a_p->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal && !a_p->mm->lsMoved)
				{
					HelperFuncs::PlayEmoteIdle(a_p);
				}
			}
		}

		void Sprint(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Stop silent roll/shield charge or sprinting if not paragliding and not using M.A.R.F.

			if (!a_p->mm->isParagliding && !a_p->tm->isMARFing) 
			{
				if (a_p->coopActor->IsSneaking() || a_p->pam->IsPerforming(InputAction::kBlock)) 
				{
					// Stop silent roll/shield charge animation.
					// Sprint stop.
					Util::RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionSprintStop, a_p->coopActor.get());
				}
				else 
				{
					// Sprint stop action isn't processed by P1's mount.
					// Stop the mounted player actor sprint animation (hunched over) here instead.
					// Mount's Update() hook will update its speedmult when not sprinting.
					a_p->coopActor->NotifyAnimationGraph("SprintStop");
				}
			}
		}

		void ZoomCam(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset adjustment mode to None and relinquish camera control.

			HelperFuncs::SetCameraAdjustmentMode(a_p->controllerID, InputAction::kZoomCam, false);
		}
	};
};

#include "PlayerActionFunctionsHolder.h"
#include <Compatibility.h>
#include <GlobalCoopData.h>
#include <Settings.h>

#undef PlaySound

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
		const auto paOffset = !InputAction::kFirstAction;
		// Populate with nullopt first before individually assigning defined funcs.
		// No actions will have a function in every category.
		_condFuncs = std::vector<CondFunc>
		(
			!InputAction::kActionTotal, CondFunc(std::nullopt)
		);
		_startFuncs = std::vector<StartFunc>
		(
			!InputAction::kActionTotal, StartFunc(std::nullopt)
		);
		_progFuncs = std::vector<ProgressFunc>
		(
			!InputAction::kActionTotal, ProgressFunc(std::nullopt)
		);
		_cleanupFuncs = std::vector<CleanupFunc>
		(
			!InputAction::kActionTotal, CleanupFunc(std::nullopt)
		);

		// clang-format off
	
		//=======================
		// [Condition Functions]:
		//=======================
		
		// Without conditions:
		// CycleSpellCategoryLH, CycleSpellCategoryRH, 
		// CycleWeaponCategoryLH, CycleWeaponCategoryRH, 
		// FaceTarget, GrabObject, 
		// MoveCrosshair, QuickSlotCast, QuickSlotItem, 
		// ResetAim, Shout

		// Unique conditions.
		_condFuncs[!InputAction::kActivate - paOffset] =
		ConditionFuncs::Activate;
		_condFuncs[!InputAction::kActivateCancel - paOffset] =
		ConditionFuncs::ActivateCancel;
		_condFuncs[!InputAction::kAdjustAimPitch - paOffset] =
		ConditionFuncs::AdjustAimPitch;
		_condFuncs[!InputAction::kAttackLH - paOffset] = 
		ConditionFuncs::AttackLH;
		_condFuncs[!InputAction::kAttackRH - paOffset] = 
		ConditionFuncs::AttackRH;
		_condFuncs[!InputAction::kBash - paOffset] = 
		ConditionFuncs::Bash;
		_condFuncs[!InputAction::kBlock - paOffset] = 
		ConditionFuncs::Block;
		_condFuncs[!InputAction::kCastLH - paOffset] = 
		ConditionFuncs::CastLH;
		_condFuncs[!InputAction::kCastRH - paOffset] = 
		ConditionFuncs::CastRH;
		_condFuncs[!InputAction::kChangeDialoguePlayer - paOffset] = 
		ConditionFuncs::ChangeDialoguePlayer;
		_condFuncs[!InputAction::kDismount - paOffset] = 
		ConditionFuncs::Dismount;
		_condFuncs[!InputAction::kDodge - paOffset] = 
		ConditionFuncs::Dodge;
		_condFuncs[!InputAction::kGrabRotateYZ - paOffset] = 
		ConditionFuncs::GrabRotateYZ;
		_condFuncs[!InputAction::kJump - paOffset] = 
		ConditionFuncs::Jump;
		_condFuncs[!InputAction::kPowerAttackDual - paOffset] = 
		ConditionFuncs::PowerAttackDual;
		_condFuncs[!InputAction::kPowerAttackLH - paOffset] =
		ConditionFuncs::PowerAttackLH;
		_condFuncs[!InputAction::kPowerAttackRH - paOffset] = 
		ConditionFuncs::PowerAttackRH;
		_condFuncs[!InputAction::kSheathe - paOffset] = 
		ConditionFuncs::Sheathe;
		_condFuncs[!InputAction::kSneak - paOffset] = 
		ConditionFuncs::Sneak;
		_condFuncs[!InputAction::kSpecialAction - paOffset] = 
		ConditionFuncs::SpecialAction;
		_condFuncs[!InputAction::kSprint - paOffset] =
		ConditionFuncs::Sprint;
		_condFuncs[!InputAction::kTeleportToPlayer - paOffset] =
		ConditionFuncs::TeleportToPlayer;
		
		// Adjusting the camera.
		_condFuncs[!InputAction::kCamLockOn - paOffset] = 
		_condFuncs[!InputAction::kCamManualPos - paOffset] = 
		_condFuncs[!InputAction::kDisableCoopCam - paOffset] = 
		_condFuncs[!InputAction::kResetCamOrientation - paOffset] = 
		ConditionFuncs::CamActive;
		_condFuncs[!InputAction::kRotateCam - paOffset] = 
		_condFuncs[!InputAction::kZoomCam - paOffset] = 
		ConditionFuncs::CanAdjustCamera;

		// Debug options.
		_condFuncs[!InputAction::kDebugRagdollPlayer - paOffset] = 
		ConditionFuncs::DebugRagdollPlayer;
		_condFuncs[!InputAction::kDebugReEquipHandForms - paOffset] = 
		ConditionFuncs::CycleEquipment;
		_condFuncs[!InputAction::kDebugRefreshPlayerManagers - paOffset] =
		ConditionFuncs::DebugRefreshPlayerManagers;
		_condFuncs[!InputAction::kDebugResetPlayer - paOffset] =
		ConditionFuncs::DebugResetPlayer;

		// Event-triggered custom menus.
		_condFuncs[!InputAction::kCoopDebugMenu - paOffset] =
		_condFuncs[!InputAction::kCoopIdlesMenu - paOffset] = 
		_condFuncs[!InputAction::kCoopMiniGamesMenu - paOffset] = 
		_condFuncs[!InputAction::kCoopSummoningMenu - paOffset] = 
		ConditionFuncs::PlayerCanOpenMenu;

		// Cycling/equipping hotkeyed gear/magic.
		_condFuncs[!InputAction::kCycleAmmo - paOffset] =
		_condFuncs[!InputAction::kCycleSpellLH - paOffset] =
		_condFuncs[!InputAction::kCycleSpellRH - paOffset] =
		_condFuncs[!InputAction::kCycleVoiceSlotMagic - paOffset] =
		_condFuncs[!InputAction::kCycleWeaponLH - paOffset] =
		_condFuncs[!InputAction::kCycleWeaponRH - paOffset] =
		_condFuncs[!InputAction::kHotkeyEquip - paOffset] = 
		ConditionFuncs::CycleEquipment;
		
		// Menus that allow players to access their inventory or magic and equip items.
		_condFuncs[!InputAction::kFavorites - paOffset] = 
		ConditionFuncs::Favorites;
		_condFuncs[!InputAction::kInventory - paOffset] = 
		_condFuncs[!InputAction::kMagicMenu - paOffset] = 
		ConditionFuncs::PlayerCanOpenItemMenu;

		// Menus that allow player to modify their perks and HMS AVs.
		_condFuncs[!InputAction::kStatsMenu - paOffset] = 
		_condFuncs[!InputAction::kTweenMenu - paOffset] = 
		ConditionFuncs::PlayerCanOpenPerksMenu;

		// Other menus.
		_condFuncs[!InputAction::kMapMenu - paOffset] = 
		_condFuncs[!InputAction::kPause - paOffset] = 
		_condFuncs[!InputAction::kTradeWithPlayer - paOffset] = 
		_condFuncs[!InputAction::kWaitMenu - paOffset] =
		ConditionFuncs::PlayerCanOpenMenu;

		// Arms rotation.
		_condFuncs[!InputAction::kRotateLeftForearm - paOffset] =
		_condFuncs[!InputAction::kRotateLeftHand - paOffset] =
		_condFuncs[!InputAction::kRotateLeftShoulder - paOffset] = 
		_condFuncs[!InputAction::kRotateRightForearm - paOffset] =
		_condFuncs[!InputAction::kRotateRightHand - paOffset] =
		_condFuncs[!InputAction::kRotateRightShoulder - paOffset] = 
		ConditionFuncs::CanRotateArms;

		//======================
		// [Progress Functions]:
		//======================

		_progFuncs[!InputAction::kActivate - paOffset] = 
		ProgressFuncs::Activate;
		_progFuncs[!InputAction::kAttackLH - paOffset] =
		ProgressFuncs::AttackLH;
		_progFuncs[!InputAction::kAttackRH - paOffset] = 
		ProgressFuncs::AttackRH;
		_progFuncs[!InputAction::kBash - paOffset] = 
		ProgressFuncs::Bash;
		_progFuncs[!InputAction::kCastLH - paOffset] = 
		ProgressFuncs::CastLH;
		_progFuncs[!InputAction::kCastRH - paOffset] = 
		ProgressFuncs::CastRH;
		_progFuncs[!InputAction::kCycleAmmo - paOffset] = 
		ProgressFuncs::CycleAmmo;
		_progFuncs[!InputAction::kCycleSpellCategoryLH - paOffset] = 
		ProgressFuncs::CycleSpellCategoryLH;
		_progFuncs[!InputAction::kCycleSpellCategoryRH - paOffset] = 
		ProgressFuncs::CycleSpellCategoryRH;
		_progFuncs[!InputAction::kCycleSpellLH - paOffset] = 
		ProgressFuncs::CycleSpellLH;
		_progFuncs[!InputAction::kCycleSpellRH - paOffset] =
		ProgressFuncs::CycleSpellRH;
		_progFuncs[!InputAction::kCycleVoiceSlotMagic - paOffset] = 
		ProgressFuncs::CycleVoiceSlotMagic;
		_progFuncs[!InputAction::kCycleWeaponCategoryLH - paOffset] = 
		ProgressFuncs::CycleWeaponCategoryLH;
		_progFuncs[!InputAction::kCycleWeaponCategoryRH - paOffset] = 
		ProgressFuncs::CycleWeaponCategoryRH;
		_progFuncs[!InputAction::kCycleWeaponLH - paOffset] = 
		ProgressFuncs::CycleWeaponLH;
		_progFuncs[!InputAction::kCycleWeaponRH - paOffset] = 
		ProgressFuncs::CycleWeaponRH;
		_progFuncs[!InputAction::kGrabObject - paOffset] =
		ProgressFuncs::GrabObject;
		_progFuncs[!InputAction::kHotkeyEquip - paOffset] = 
		ProgressFuncs::HotkeyEquip;
		_progFuncs[!InputAction::kQuickSlotCast - paOffset] = 
		ProgressFuncs::QuickSlotCast;
		_progFuncs[!InputAction::kShout - paOffset] =
		ProgressFuncs::Shout;
		_progFuncs[!InputAction::kSpecialAction - paOffset] = 
		ProgressFuncs::SpecialAction;

		//===================
		// [Start Functions]:
		//===================

		_startFuncs[!InputAction::kActivateAllOfType - paOffset] = 
		StartFuncs::ActivateAllOfType;
		_startFuncs[!InputAction::kActivateCancel - paOffset] = 
		StartFuncs::ActivateCancel;
		_startFuncs[!InputAction::kBlock - paOffset] = 
		StartFuncs::Block;
		_startFuncs[!InputAction::kCamLockOn - paOffset] = 
		StartFuncs::CamLockOn;
		_startFuncs[!InputAction::kCamManualPos - paOffset] = 
		StartFuncs::CamManualPos;
		_startFuncs[!InputAction::kChangeDialoguePlayer - paOffset] =
		StartFuncs::ChangeDialoguePlayer;
		_startFuncs[!InputAction::kCoopDebugMenu - paOffset] = 
		StartFuncs::CoopDebugMenu;
		_startFuncs[!InputAction::kCoopIdlesMenu - paOffset] = 
		StartFuncs::CoopIdlesMenu;
		_startFuncs[!InputAction::kCoopMiniGamesMenu - paOffset] = 
		StartFuncs::CoopMiniGamesMenu;
		_startFuncs[!InputAction::kCoopSummoningMenu - paOffset] = 
		StartFuncs::CoopSummoningMenu;
		_startFuncs[!InputAction::kDebugRagdollPlayer - paOffset] = 
		StartFuncs::DebugRagdollPlayer;
		_startFuncs[!InputAction::kDebugReEquipHandForms - paOffset] = 
		StartFuncs::DebugReEquipHandForms;
		_startFuncs[!InputAction::kDebugRefreshPlayerManagers - paOffset] = 
		StartFuncs::DebugRefreshPlayerManagers;
		_startFuncs[!InputAction::kDebugResetPlayer - paOffset] =
		StartFuncs::DebugResetPlayer;
		_startFuncs[!InputAction::kDisableCoopCam - paOffset] = 
		StartFuncs::DisableCoopCam;
		_startFuncs[!InputAction::kDismount - paOffset] = 
		StartFuncs::Dismount;
		_startFuncs[!InputAction::kDodge - paOffset] = 
		StartFuncs::Dodge;
		_startFuncs[!InputAction::kFaceTarget - paOffset] = 
		StartFuncs::FaceTarget;
		_startFuncs[!InputAction::kFavorites - paOffset] = 
		StartFuncs::Favorites;
		_startFuncs[!InputAction::kInventory - paOffset] = 
		StartFuncs::Inventory;
		_startFuncs[!InputAction::kJump - paOffset] = 
		StartFuncs::Jump;
		_startFuncs[!InputAction::kMagicMenu - paOffset] = 
		StartFuncs::MagicMenu;
		_startFuncs[!InputAction::kMapMenu - paOffset] =
		StartFuncs::MapMenu;
		_startFuncs[!InputAction::kPause - paOffset] = 
		StartFuncs::Pause;
		_startFuncs[!InputAction::kPowerAttackDual - paOffset] = 
		StartFuncs::PowerAttackDual;
		_startFuncs[!InputAction::kPowerAttackLH - paOffset] = 
		StartFuncs::PowerAttackLH;
		_startFuncs[!InputAction::kPowerAttackRH - paOffset] =
		StartFuncs::PowerAttackRH;
		_startFuncs[!InputAction::kQuickSlotItem - paOffset] =
		StartFuncs::QuickSlotItem;
		_startFuncs[!InputAction::kResetAim - paOffset] =
		StartFuncs::ResetAim;
		_startFuncs[!InputAction::kResetCamOrientation - paOffset] =
		StartFuncs::ResetCamOrientation;
		_startFuncs[!InputAction::kRotateCam - paOffset] = 
		StartFuncs::RotateCam;
		_startFuncs[!InputAction::kSheathe - paOffset] = 
		StartFuncs::Sheathe;
		_startFuncs[!InputAction::kSneak - paOffset] = 
		StartFuncs::Sneak;
		_startFuncs[!InputAction::kSprint - paOffset] = 
		StartFuncs::Sprint;
		_startFuncs[!InputAction::kStatsMenu - paOffset] = 
		StartFuncs::StatsMenu;
		_startFuncs[!InputAction::kTeleportToPlayer - paOffset] = 
		StartFuncs::TeleportToPlayer;
		_startFuncs[!InputAction::kTradeWithPlayer - paOffset] = 
		StartFuncs::TradeWithPlayer;
		_startFuncs[!InputAction::kTweenMenu - paOffset] = 
		StartFuncs::TweenMenu;
		_startFuncs[!InputAction::kWaitMenu - paOffset] = 
		StartFuncs::WaitMenu;
		_startFuncs[!InputAction::kZoomCam - paOffset] = 
		StartFuncs::ZoomCam;

		//=====================
		// [Cleanup Functions]:
		//=====================

		_cleanupFuncs[!InputAction::kActivate - paOffset] = 
		CleanupFuncs::Activate;
		_cleanupFuncs[!InputAction::kAttackLH - paOffset] = 
		CleanupFuncs::AttackLH;
		_cleanupFuncs[!InputAction::kAttackRH - paOffset] = 
		CleanupFuncs::AttackRH;
		_cleanupFuncs[!InputAction::kBash - paOffset] = 
		CleanupFuncs::Bash;
		_cleanupFuncs[!InputAction::kBlock - paOffset] = 
		CleanupFuncs::Block;
		_cleanupFuncs[!InputAction::kCastLH - paOffset] = 
		CleanupFuncs::CastLH;
		_cleanupFuncs[!InputAction::kCastRH - paOffset] =
		CleanupFuncs::CastRH;
		_cleanupFuncs[!InputAction::kCycleAmmo - paOffset] = 
		CleanupFuncs::CycleAmmo;
		_cleanupFuncs[!InputAction::kCycleSpellCategoryLH - paOffset] = 
		CleanupFuncs::CycleSpellCategoryLH;
		_cleanupFuncs[!InputAction::kCycleSpellCategoryRH - paOffset] = 
		CleanupFuncs::CycleSpellCategoryRH;
		_cleanupFuncs[!InputAction::kCycleSpellLH - paOffset] = 
		CleanupFuncs::CycleSpellLH;
		_cleanupFuncs[!InputAction::kCycleSpellRH - paOffset] = 
		CleanupFuncs::CycleSpellRH;
		_cleanupFuncs[!InputAction::kCycleVoiceSlotMagic - paOffset] = 
		CleanupFuncs::CycleVoiceSlotMagic;
		_cleanupFuncs[!InputAction::kCycleWeaponCategoryLH - paOffset] = 
		CleanupFuncs::CycleWeaponCategoryLH;
		_cleanupFuncs[!InputAction::kCycleWeaponCategoryRH - paOffset] = 
		CleanupFuncs::CycleWeaponCategoryRH;
		_cleanupFuncs[!InputAction::kCycleWeaponLH - paOffset] = 
		CleanupFuncs::CycleWeaponLH;
		_cleanupFuncs[!InputAction::kCycleWeaponRH - paOffset] = 
		CleanupFuncs::CycleWeaponRH;
		_cleanupFuncs[!InputAction::kGrabObject - paOffset] = 
		CleanupFuncs::GrabObject;
		_cleanupFuncs[!InputAction::kHotkeyEquip - paOffset] = 
		CleanupFuncs::HotkeyEquip;
		_cleanupFuncs[!InputAction::kQuickSlotCast - paOffset] = 
		CleanupFuncs::QuickSlotCast;
		_cleanupFuncs[!InputAction::kRotateCam - paOffset] = 
		CleanupFuncs::RotateCam;
		_cleanupFuncs[!InputAction::kShout - paOffset] = 
		CleanupFuncs::Shout;
		_cleanupFuncs[!InputAction::kSpecialAction - paOffset] = 
		CleanupFuncs::SpecialAction;
		_cleanupFuncs[!InputAction::kSprint - paOffset] = 
		CleanupFuncs::Sprint;
		_cleanupFuncs[!InputAction::kZoomCam - paOffset] =
		CleanupFuncs::ZoomCam;

		// clang-format on
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
			
			if (a_p->pam->downedPlayerTarget->isDowned && !a_p->pam->downedPlayerTarget->isRevived)
			{
				// Otherwise, attempt to revive the downed player 
				// if they are not fully revived 
				// and if this player has enough health to transfer.
				if (HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kActivate)) 
				{
					return true;
				}
				else
				{
					// Inform the player that they do not have enough health 
					// to revive another player.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kReviveAlert,
						fmt::format
						(
							"P{}: <font color=\"#FF0000\">"
							"Not enough health to revive another player!</font>",
							a_p->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kActivationInfo,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
					RE::PlaySound("UIActivateFail");
				}
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
			// Don't rotate torso while rotating arms 
			// or if some inputs are still pressed for the arm rotation binds.
			// Want to have independent arm and torso adjustment.
			bool isTryingToRotateArms = 
			{
				(Settings::bEnableArmsRotation && !a_p->coopActor->IsWeaponDrawn()) &&
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

			const auto& rsState = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
			// Must be moving the RS.
			return rsState.normMag > 0.0f;
		}

		bool AttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// If arms rotation is disabled, 
			// will draw weapons/magic/fists on release instead of attacking.
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
			(
				// Unmounted: 
				// 1. Has LH one-handed weapon equipped or is unarmed.
				// 2. Has 2H ranged weapon and has enough stamina to bash.
				// Mounted:
				// 1. Has a melee weapon equipped.
				(em->HasLHWeapEquipped() || em->IsUnarmed()) || 
				(
					(a_p->coopActor->IsOnMount()) && 
					(
						em->HasLHMeleeWeapEquipped() || 
						em->HasRHMeleeWeapEquipped() || 
						em->Has2HMeleeWeapEquipped()
					)
				) || 
				(
					em->Has2HRangedWeapEquipped() &&
					HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kAttackLH)
				)
			);
		}

		bool AttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// If arms rotation is disabled, 
			// will draw weapons/magic/fists on release instead of attacking.
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
				(!em->HasRHSpellEquipped() || em->IsUnarmed()) ||
				(
					(a_p->coopActor->IsOnMount()) && 
					(
						em->HasLHMeleeWeapEquipped() ||
						em->HasRHMeleeWeapEquipped() || 
						em->Has2HMeleeWeapEquipped()
					)
				)
			};
		}

		bool Bash(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// No weapon/magic out means no bash.
			if (!a_p->coopActor->IsWeaponDrawn())
			{
				return false;
			}

			// Must be blocking beforehand.
			// Then check stamina requirement.
			return 
			(
				a_p->pam->IsPerforming(InputAction::kBlock) &&
				HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kBash)
			);
		}

		bool Block(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can't block when weapons/magic aren't drawn or if transformed.
			if (!a_p->coopActor->IsWeaponDrawn() || a_p->isTransformed)
			{
				return false;
			}

			const auto& em = a_p->em;
			// Not mounted, LH must contain a shield/torch or a 2H weapon, 
			// or LH must be empty with anything but a staff in the RH.
			return 
			{
				(!a_p->coopActor->IsOnMount()) &&
				(
					(
						em->HasTorchEquipped() ||
						em->HasShieldEquipped() || 
						em->Has2HMeleeWeapEquipped()
					) ||
					(
						em->HandIsEmpty(false) && 
						!em->HandIsEmpty(true) && 
						!em->HasRHStaffEquipped()
					)
				)
			};
		}

		bool CastLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Spells are not out.
			if (!a_p->coopActor->IsWeaponDrawn())
			{
				return false;
			}

			// Casting does not function when mounted and can cause the game to automatically
			// dismount the player if the player presses this bind and then hits an enemy
			// with a melee weapon.
			if (a_p->coopActor->IsOnMount())
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

			if (a_p->isPlayer1)
			{
				// No need to check for adequate magicka, since the game
				// will prevent casting if P1 does not have enough magicka automatically.
				return a_p->em->HasLHSpellEquipped();
			}
			else
			{
				// Cast if LH contains a spell and the player has enough magicka to cast.
				return 
				(
					a_p->em->HasLHSpellEquipped() && 
					HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kCastLH)
				);
			}
		}

		bool CastRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Spells are not out.
			if (!a_p->coopActor->IsWeaponDrawn())
			{
				return false;
			}
			
			// Casting does not function when mounted and can cause the game to automatically
			// dismount the player if the player presses this bind and then hits an enemy
			// with a melee weapon.
			if (a_p->coopActor->IsOnMount())
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

			if (a_p->isPlayer1)
			{
				// No need to check for adequate magicka, since the game
				// will prevent casting if P1 does not have enough magicka automatically.
				return a_p->em->HasRHSpellEquipped();
			}
			else
			{
				// Cast if RH contains a spell, and the player has enough magicka to cast.
				return 
				(
					a_p->em->HasRHSpellEquipped() && 
					HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kCastRH)
				);
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
				// Also exit any occupied furniture.
				return 
				(
					a_p->coopActor->IsOnMount() || 
					Util::HandleIsValid(a_p->coopActor->GetOccupiedFurniture())
				);
			}
			else
			{
				// Dismount if mounted, 
				// or exit furniture if running the interaction package.
				return 
				(
					a_p->coopActor->IsOnMount() || 
					Util::HandleIsValid(a_p->coopActor->GetOccupiedFurniture()) || 
					a_p->mm->interactionPackageRunning
				);
			}
		}

		bool Dodge(const std::shared_ptr<CoopPlayer>& a_p) 
		{
			// Dodge if the player has enough stamina.
			// TODO: 
			// Support as many dodge mods as possible.
			// Defaulting to the mod's generic dash dodge for now.
			// Prospective compatibility:
			// - TK Dodge RE
			// - TUDM
			// - DMCO

			bool tkDodging = false;
			a_p->coopActor->GetGraphVariableBool("bIsDodging", tkDodging);
			auto charController = a_p->coopActor->GetCharController();
			// Can't dodge if already dodging, airborne, mounted, sneaking while not transformed,
			// blocking, flying, or not having enough stamina.
			return 
			(
				(!tkDodging && !a_p->mm->isDashDodging) && 
				(
					(a_p->mm->isParagliding) || 
					(a_p->coopActor->IsSwimming()) ||
					(
						!a_p->mm->isAirborneWhileJumping && 
						charController && 
						charController->context.currentState == 
						RE::hkpCharacterStateType::kOnGround &&
						charController->surfaceInfo.supportedState.all
						(
							RE::hkpSurfaceInfo::SupportedState::kSupported
						)
					)
				) &&
				(!a_p->coopActor->IsSneaking() || a_p->isTransformed) && 
				(
					!a_p->coopActor->IsOnMount() && 
					!a_p->pam->IsPerforming(InputAction::kBlock) &&
					!a_p->coopActor->IsFlying()
				) &&
				(HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kDodge))
			);
		}

		bool Favorites(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// No supported menus open or control is obtainable AND
			// Either the player is part of an NPC race and not (un)equipping OR
			// the player is a Vampire Lord (access vampiric spells through the Favorites Menu).
			bool isEquipping = false;
			bool isUnequipping = false;
			a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			return
			{
				(
					!glob.supportedMenuOpen.load() ||
					GlobalCoopData::CanControlMenus(a_p->controllerID)
				) &&
				(
					(
						a_p->coopActor->race && 
						a_p->coopActor->race->HasKeyword(glob.npcKeyword) &&
						!isEquipping && 
						!isUnequipping
					) || 
					(Util::IsVampireLord(a_p->coopActor.get()))
				)
			};
		}

		bool GrabRotateYZ(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Must be moving the RS and grabbing at least 1 object.
			const auto& rsState = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
			return 
			(
				rsState.normMag > 0.0f &&
				a_p->tm->rmm->isGrabbing && 
				a_p->tm->rmm->GetNumGrabbedRefrs() > 0
			);
		}

		bool Jump(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can only start jumping if not ragdolled and not getting up or staggered.
			if (a_p->coopActor->IsInRagdollState() || 
				a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal)
			{
				return false;
			}

			// NOTE: 
			// Ignore instances where P1 has occupied furniture
			// but is not locked into a furniture-use animation 
			// or the furniture does not have an associated idle form
			// (such as when near an interactable EVGAT marker).
			bool usingFurniture = 
			(
				(!a_p->isPlayer1 && a_p->mm->interactionPackageRunning) ||
				(
					(
						!a_p->coopActor->IsOnMount() &&
						a_p->coopActor->GetOccupiedFurniture()
					) && 
					(
						(a_p->mm->isAnimDriven) ||
						(
							a_p->coopActor->GetMiddleHighProcess() && 
							a_p->coopActor->GetMiddleHighProcess()->furnitureIdle
						)
					)
				)
			);
			// Can jump if using furniture (stops furniture use animations).
			if (usingFurniture)
			{
				return true;
			}

			auto charController = a_p->mm->movementActorPtr->GetCharController(); 
			if (!charController)
			{
				return false;
			}

			// Cannot start jumping if not on the ground.
			// NOTE: 
			// Since jumping will frequently fail to trigger when it visually should,
			// more stringent conditions commented out for now.
			bool stateAllowsJump = 
			(
				charController->context.currentState == RE::hkpCharacterStateType::kOnGround
			);
			if (!stateAllowsJump) 
			{
				return false;
			}

			// Check slope angle for the surface the player would like to jump off.
			// Must be less than the defined max jump surface slope angle.
			const float& zComp = charController->surfaceInfo.surfaceNormal.quad.m128_f32[2];
			// Default to allow the jump.
			float supportAng = 0.0f;
			// Vector along the surface is at 90 degrees to the normal, 
			// so subtract the normal's pitch from PI / 2.
			if (charController->surfaceInfo.surfaceNormal.Length3() != 0.0f) 
			{
				supportAng = 
				(
					PI / 2.0f - 
					fabsf
					(
						asinf(charController->surfaceInfo.surfaceNormal.quad.m128_f32[2])
					)
				);
			}
			else 
			{
				// Raycast hit result as a fallback option if the support surface normal data 
				// is unavailable (0).
				glm::vec4 start =
				{
					a_p->coopActor->data.location.x,
					a_p->coopActor->data.location.y,
					a_p->coopActor->data.location.z + a_p->coopActor->GetHeight(),
					0.0f
				};
				glm::vec4 end = 
				(
					start - glm::vec4(0.0f, 0.0f, 1.25f * a_p->coopActor->GetHeight(), 0.0f)
				);
				auto result = Raycast::hkpCastRay
				(
					start,
					end, 
					std::vector<RE::TESObjectREFR*>({ a_p->coopActor.get() }),
					std::vector<RE::FormType>
					(
						{ RE::FormType::Activator, RE::FormType::TalkingActivator }
					)
				);
				if (result.hit)
				{
					supportAng = PI / 2.0f - fabsf(asinf(result.rayNormal.z));
				}	
			}

			return supportAng < Settings::fJumpingMaxSlopeAngle;
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

		bool Sheathe(const std::shared_ptr<CoopPlayer>& a_p)
		{
			if (a_p->isPlayer1)
			{
				return true;
			}
			
			// Can only sheathe if the companion player is not in the process of attacking, 
			// since actions such as loading a crossbow while attempting to sheathe 
			// cause the crossbow to disappear.
			return !a_p->pam->isRangedWeaponAttack;
		}

		bool Sneak(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// On the ground and not mounted or using furniture.
			const auto charController = a_p->coopActor->GetCharController();
			return 
			(
				!a_p->coopActor->IsSwimming() &&
				!a_p->coopActor->IsOnMount() &&
				a_p->coopActor->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && 
				charController && 
				charController->context.currentState == RE::hkpCharacterStateType::kOnGround
			);
		}

		bool SpecialAction(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Perform special contextual actions based on what weapons/magic the player has
			// equipped in either hand.
			// Additional actions can also be performed on consecutive presses.
			// Possible TODO: 
			// Add config option to trigger a custom action with this bind.

			const auto& pam = a_p->pam;
			const auto& em = a_p->em;
			// Clear old requested action first.
			pam->reqSpecialAction = SpecialActionType::kNone;

			// [Single Tap/Hold Actions]:
			// - Get Up
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
			const bool doubleTapped = pam->PassesConsecTapsCheck(InputAction::kSpecialAction);
			// Check for double tap actions first.
			if (doubleTapped)
			{
				// 2+ consecutive taps.
				// Nothing to do right now when transformed.
				if (a_p->isTransforming)
				{
					return false;
				}

				// Actions to perform when not ragdolled or 
				// transformed and the player has drawn their weapon/magic and is not paragliding.
				if (!isRagdolled && 
					!a_p->isTransformed && 
					a_p->coopActor->IsWeaponDrawn() && 
					!a_p->mm->isParagliding)
				{
					if (em->quickSlotItem)
					{
						// Use quick slot item if it is equipped.
						pam->reqSpecialAction = SpecialActionType::kQuickItem;
					}
					else if (em->HasRHMeleeWeapEquipped() || 
							 em->IsUnarmed() || 
							 em->Has2HMeleeWeapEquipped() ||
							 em->Has2HRangedWeapEquipped())
					{
						// Bash if the RH contains a weapon,
						// if unarmed,
						// or if a 2H weapon is equipped.
						pam->reqSpecialAction = SpecialActionType::kBash;
					}
					else
					{
						// Dodge otherwise.
						pam->reqSpecialAction = SpecialActionType::kDodge;
					}
				}
				else if (Settings::bEnableFlopping && !a_p->coopActor->IsWeaponDrawn())
				{
					bool isGrabbed = std::any_of
					(
						glob.coopPlayers.begin(), glob.coopPlayers.end(),
						[&a_p](const auto& p) 
						{
							return
							(
								p->isActive && 
								p->coopActor != a_p->coopActor && 
								p->tm->rmm->IsManaged(a_p->coopActor->GetHandle(), true)
							);
						}
					);

					// Can flop if grabbed by another player or if not ragdolled 
					// and weapons/magic are sheathed or the player is paragliding.
					bool canFlop = 
					(
						(isGrabbed) || 
						(
							(!isRagdolled) && 
							(!a_p->coopActor->IsWeaponDrawn() || a_p->mm->isParagliding)
						)
					);
					if (canFlop)
					{
						pam->reqSpecialAction = SpecialActionType::kFlop;
					}
				}

				// No need to check single tap/on hold special actions 
				// if a double tap action was selected and the AV check passes.
				if (pam->reqSpecialAction != SpecialActionType::kNone && 
					HelperFuncs::CanPerformSpecialAction(a_p, pam->reqSpecialAction))
				{
					return true;
				} 
			}

			// Then check for single tap/hold actions if no double tap action request was set.
			// Clear old requested action again in case it was set to a double tap action
			// and conditions failed.+
			pam->reqSpecialAction = SpecialActionType::kNone;
			if (a_p->isTransforming)
			{
				return false;
			}

			// Toggle grabbed refr collision if the setting is enabled 
			// and the player has selected a grabbed refr.
			if (a_p->tm->rmm->isGrabbing && 
				a_p->tm->rmm->IsManaged(a_p->tm->crosshairRefrHandle, true) && 
				Settings::bToggleGrabbedRefrCollisions)
			{
				pam->reqSpecialAction = SpecialActionType::kToggleGrabbedRefrCollisions;
			}
			else if (isRagdolled)
			{
				// Hold to get up if grabbed or on the ground.
				bool isGrabbed = std::any_of
				(
					glob.coopPlayers.begin(), glob.coopPlayers.end(),
					[&a_p](const auto& p) 
					{
						return 
						(
							p->isActive && 
							p->coopActor != a_p->coopActor && 
							p->tm->rmm->IsManaged(a_p->coopActor->GetHandle(), true)
						);
					}
				);

				auto charController = a_p->coopActor->GetCharController();
				bool shouldAttemptGetUp = 
				{
					(isGrabbed) ||
					(
						charController &&
						charController->context.currentState == 
						RE::hkpCharacterStateType::kOnGround
					)
				};
				if (shouldAttemptGetUp)
				{
					pam->reqSpecialAction = SpecialActionType::kGetUp;
				}
			}
			else if (a_p->isTransformed)
			{
				// If transformed into a Werewolf, show transformation time remaining 
				// or feed on a targeted corpse.
				// Currently, nothing for Vampire Lords.
				// Revert transformation if transformed into another non-playable race.
				pam->reqSpecialAction = SpecialActionType::kTransformation;
			}
			else if (a_p->coopActor->IsWeaponDrawn())
			{
				if ((em->quickSlotSpell) && 
					(a_p->mm->isParagliding || a_p->coopActor->IsSwimming()))
				{
					// Quick slot cast while paragliding or swimming.
					pam->reqSpecialAction = SpecialActionType::kQuickCast;
				}
				else if ((em->HasRHMeleeWeapEquipped() || 
						 em->IsUnarmed() || 
						 em->Has2HMeleeWeapEquipped() ||
						 em->Has2HRangedWeapEquipped() ||
						 em->HasTorchEquipped()) ||
						 (em->HasLHMeleeWeapEquipped() && em->HandIsEmpty(true)))
				{
					// Block if the RH has a weapon,
					// if unarmed, 
					// if there's a 2H weapon equipped, 
					// if the LH has a torch,
					// or if the player has a LH weapon and an empty RH.
					pam->reqSpecialAction = SpecialActionType::kBlock;
				}
				/*
				else if (HelperFuncs::CanDualCast(a_p))
				{
					// TODO: 
					// Dual cast when LH and RH both have the same 1H spells equipped 
					// and the player has the dual cast perk.
					// Currently, the perks do not apply when casting with companion players, 
					// so this is commented out.
					pam->reqSpecialAction = SpecialActionType::kDualCast;
				}
				*/
				else if ((em->HasLHSpellEquipped() && em->HasRHSpellEquipped()) ||
						 (em->HasLHStaffEquipped() && em->HasRHStaffEquipped()))
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
			
			// If a new special action to perform was chosen, perform HMS AV checks.
			return 
			(
				pam->reqSpecialAction != SpecialActionType::kNone && 
				HelperFuncs::CanPerformSpecialAction(a_p, pam->reqSpecialAction)
			);
		}

		bool Sprint(const std::shared_ptr<CoopPlayer>& a_p)
		{
			bool canPerform = false;
			if (a_p->coopActor->IsSneaking()) 
			{
				// Must be moving while sneaking and have the silent roll perk.
				canPerform =  
				(
					a_p->mm->lsMoved &&
					a_p->coopActor->HasPerk(glob.sneakRollPerk)
				);
			}
			else if (a_p->pam->IsPerforming(InputAction::kBlock))
			{
				// Moving and blocking with a shield equipped + have the shield charge perk.
				canPerform =
				(
					a_p->mm->lsMoved &&
					a_p->em->HasShieldEquipped() &&
					a_p->coopActor->HasPerk(glob.shieldChargePerk)
				);
			}
			else
			{
				// Must be moving without jumping/paragliding, 
				// not attacking/bashing/blocking/casting,
				// and cannot be dash dodging
				// and not ragdolled or ragdolled while M.A.R.F-ing.
				canPerform =  
				(
					((a_p->mm->isParagliding) || (a_p->mm->lsMoved && !a_p->pam->isJumping)) &&
					(
						!a_p->pam->isAttacking && !a_p->pam->isBashing && 
						!a_p->pam->isBlocking && !a_p->pam->isInCastingAnim
					) && 
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

			return 
			(
				!Util::OpenMenuStopsMovement() && 
				GlobalCoopData::IsNotControllingMenus(a_p->controllerID)
			);
		}

		//=========================================================================================
		// [Multiple Use]
		//=========================================================================================
	
		bool CamActive(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Co-op camera manager is active.
			return glob.cam->IsRunning();
		}

		bool CanAdjustCamera(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Can't adjust something that isn't active.
			if (!CamActive(a_p)) 
			{
				return false;
			}

			// If there is a focal player and this player is not the focal player,
			// ignore requests to adjust the camera's rotation or zoom.
			if (glob.cam->focalPlayerCID != -1 && a_p->controllerID != glob.cam->focalPlayerCID)
			{
				return false;
			}

			// Can't adjust the camera if the player is trying to rotate their arms, 
			// since the RS input is used for that purpose.
			bool isTryingToRotateArms =
			{ 
				(Settings::bEnableArmsRotation && !a_p->coopActor->IsWeaponDrawn()) && 
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
			// Must move the RS while not adjusting arm rotation.
			return rsState.normMag > 0.0f && !isTryingToRotateArms;
		}

		bool CanPowerAttack(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// If arms rotation is disabled,
			// will draw weapons/magic/fists on release instead of attacking.

			bool drawn = a_p->coopActor->IsWeaponDrawn();
			if (!drawn && !a_p->isTransformed)
			{
				return true;
			}

			// Must have weapons/magic drawn, not be power attacking,
			// not mounted, and have enough stamina.
			bool baselineCheck = 
			{
				drawn && 
				!a_p->pam->isPowerAttacking && 
				!a_p->coopActor->IsOnMount() &&
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
			if (a_p->pam->isSprinting) 
			{
				// Next, check if a melee weapon is equipped in the appropriate hand 
				// and if the player has the appropriate perk.
				const auto& em = a_p->em;
				switch (a_action)
				{
				case InputAction::kPowerAttackLH:
				{
					// Must have the corresponding perk and a melee weapon in the LH.
					return 
					{
						(
							a_p->coopActor->HasPerk(glob.criticalChargePerk) && 
							em->HasLHMeleeWeapEquipped()
						) ||
						(
							a_p->coopActor->HasPerk(glob.greatCriticalChargePerk) && 
							em->Has2HMeleeWeapEquipped()
						)
					};
				}
				case InputAction::kPowerAttackRH:
				{
					// Must have the corresponding perk and a melee weapon in the RH.
					return 
					{
						(
							a_p->coopActor->HasPerk(glob.criticalChargePerk) && 
							em->HasRHMeleeWeapEquipped()
						) ||
						(
							a_p->coopActor->HasPerk(glob.greatCriticalChargePerk) && 
							em->Has2HMeleeWeapEquipped()
						)
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
					return 
					(
						a_p->em->IsUnarmed() ||
						a_p->em->LHEmpty() || 
						a_p->em->HasLHMeleeWeapEquipped()
					);
				}
				case InputAction::kPowerAttackRH:
				{
					// Either unarmed, RH is empty, or using a weapon in the RH.
					return 
					(
						a_p->em->IsUnarmed() || 
						a_p->em->RHEmpty() || 
						a_p->em->HasRHMeleeWeapEquipped() || 
						a_p->em->Has2HMeleeWeapEquipped()
					);
				}
				case InputAction::kPowerAttackDual:
				{
					// Either unarmed, has a 2H weapon equipped, 
					// or is dual wielding (two 1H weapons).
					return 
					(
						a_p->em->IsUnarmed() || 
						a_p->em->Has2HMeleeWeapEquipped() ||
						a_p->em->IsDualWielding()
					);
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
				Settings::bEnableArmsRotation &&
				inDialogueOrNotControllingMenus &&
				a_p->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal && 
				!a_p->coopActor->IsWeaponDrawn()
			);
		}

		bool CycleEquipment(const std::shared_ptr<CoopPlayer>& a_p)
		{
			bool isAttacking = false;
			bool isCasting = false;
			bool isCastingDual = false;
			bool isCastingLH = false;
			bool isCastingRH = false;
			a_p->coopActor->GetGraphVariableBool("IsAttacking", isAttacking);
			a_p->coopActor->GetGraphVariableBool("IsCastingDual", isCastingDual);
			a_p->coopActor->GetGraphVariableBool("IsCastingLeft", isCastingLH);
			a_p->coopActor->GetGraphVariableBool("IsCastingRight", isCastingRH);
			isCasting = isCastingDual || isCastingLH || isCastingRH;
			
			// NOTE: 
			// (Un)equipping when attempting to cycle equipment used to lead to crashes,
			// so if these issues resurface, I'll re-add the (un)equip restriction below.
			// Cannot change equipment when a companion player's inventory is copied over to P1.
			bool inventoryCopiedToP1 = glob.copiedPlayerDataTypes.all
			(
				CopyablePlayerDataTypes::kInventory
			);
			return !isAttacking && !isCasting && !inventoryCopiedToP1;
		}

		bool PlayerCanOpenMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Crash when equipping magic/weapons while opening the Magic/Favorites menus,
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
				(
					(Util::IsRaceWithTransformation(a_p->coopActor->race)) || 
					(!isEquipping && !isUnequipping)
				)
			);
		}

		bool PlayerCanOpenCoopMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Crash when equipping magic/weapons while opening the Magic/Favorites menus,
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
				(!glob.supportedMenuOpen.load()) && 
				(
					(Util::IsRaceWithTransformation(a_p->coopActor->race)) || 
					(!isEquipping && !isUnequipping)
				)
			);
		}

		bool PlayerCanOpenItemMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// No supported menus open or can obtain menu control and
			// is part of a race with the NPC keyword and not (un)equipping.
			// Equipping items when not a member of a playable race causes problems,
			// so to prevent issues, we don't open any item menus under these conditions.

			bool isEquipping = false;
			bool isUnequipping = false;
			a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			return 
			(
				(
					!glob.supportedMenuOpen.load() || 
					GlobalCoopData::CanControlMenus(a_p->controllerID)
				) &&
				a_p->coopActor->race && 
				a_p->coopActor->race->HasKeyword(glob.npcKeyword) &&
				!isEquipping && 
				!isUnequipping
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
				// Only open if P1 is transformed or if this companion player 
				// is not transformed.
				// Don't want to modify default skill tree perks/HMS AVs 
				// for co-op companions while transformed.
				// Will change once I figure out how to open the Stats Menu 
				// with the special Werewolf and Vampire Lord perk trees
				// when P1 is not transformed.
				return coopP1->isTransformed || !a_p->isTransformed;
			}
		}
	};

	namespace HelperFuncs
	{
		const bool ActionJustStarted
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		)
		{
			// Action stage is started and performed duration is at 0.

			const auto& paState = a_p->pam->paStatesList[!a_action - !InputAction::kFirstAction];
			return paState.perfStage == PerfStage::kStarted && paState.secsPerformed == 0.0f;
		}

		void BashInstant(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Play bash idle to bash instantly.

			bool wasBlocking = a_p->coopActor->IsBlocking();
			a_p->coopActor->NotifyAnimationGraph("attackStop");
			a_p->coopActor->NotifyAnimationGraph("blockStart");
			// 'BowBash' triggers the normal bash animation,
			// whereas the 'BashStart' idle does not (fails conditions).
			Util::PlayIdle("BowBash", a_p->coopActor.get());
			// Must play the bash release animation for P1,
			// otherwise, P1 will hold the block animation for a few seconds
			// before performing the bash.
			if (a_p->isPlayer1)
			{
				a_p->coopActor->NotifyAnimationGraph("bashRelease");
			}

			// Continue blocking afterward.
			if (wasBlocking)
			{
				a_p->coopActor->NotifyAnimationGraph("blockStart");
			}
		}

		void BlockInstant(const std::shared_ptr<CoopPlayer>& a_p, bool&& a_shouldStart)
		{
			// Send block start/stop animation events to block instantly.

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

		const bool CanCycleOnHold
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		)
		{
			// Return true if the player can cycle through options for the given action.

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
			return 
			(
				Util::GetElapsedSeconds(a_p->lastCyclingTP) >= 
				max(0.1f, Settings::fSecsCyclingInterval)
			);
		}

		bool CanDualCast(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// TODO:
			// Dual cast if the player has the same 1H spell equipped in both hands,
			// and has the required dual casting perk.
			// Overrides default single-hand double cast.
			
			// Not a P1 action, since single trigger buttons events must
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
			// Must have the same 1H spell equipped in both hands.
			bool sameSpellInBothHands = 
			( 
				lhSpell && 
				rhSpell &&
				em->copiedMagicFormIDs[!PlaceholderMagicIndex::kLH] == 
				em->copiedMagicFormIDs[!PlaceholderMagicIndex::kRH] &&
				lhSpell->equipSlot != glob.bothHandsEquipSlot 
			);
			if (!sameSpellInBothHands)
			{
				return false;
			}

			bool canDualCast = false;
			// Same spell in both hands, so we'll just check the LH one.
			auto lhSpellSchool = 
			(
				lhSpell->avEffectSetting ? 
				lhSpell->avEffectSetting->data.associatedSkill : 
				RE::ActorValue::kNone
			);
			// Must have the corresponding perk.
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

			return canDualCast;
		}

		const bool CanPerformSpecialAction
		(
			const std::shared_ptr<CoopPlayer>& a_p, const SpecialActionType& a_actionType
		)
		{
			// HMS AV cost calculation and conditions check for special actions.
			// Return true if the player can perform the special action.

			const auto& pam = a_p->pam;
			auto& paState = 
			(
				a_p->pam->paStatesList[!InputAction::kSpecialAction - !InputAction::kFirstAction]
			);
			float baseCost = 0.0f;
			float costToPerform = 0.0f;
			// Default to 'can perform' for all actions if in god mode.
			bool canPerform = a_p->isInGodMode;
			RE::ActorValue avToCheck = RE::ActorValue::kNone;
			switch (a_actionType)
			{
			case SpecialActionType::kBash:
			{
				avToCheck = RE::ActorValue::kStamina;
				// Don't need to handle the cost, 
				// since the player action commands will do that.
				// Only have to check if the player's stamina is on cooldown or not.
				canPerform |= pam->secsTotalStaminaRegenCooldown == 0.0f;
				break;
			}
			case SpecialActionType::kCastBothHands:
			{
				avToCheck = RE::ActorValue::kMagicka;
				// Hand casting magicka cost already handled by the game.
				if (a_p->em->HasLHStaffEquipped() && a_p->em->HasRHStaffEquipped())
				{
					auto lhWeap = a_p->em->GetLHWeapon();
					auto rhWeap = a_p->em->GetRHWeapon();
					if (lhWeap->formEnchanting && rhWeap->formEnchanting)
					{
						// Both staves must have enough charge.
						canPerform |= 
						(
							lhWeap->amountofEnchantment > 0 && rhWeap->amountofEnchantment > 0
						);
					}
				}
				else
				{
					// Hand casting magicka cost already handled by the game,
					// but store base cost for to grant XP upon cast, as needed.
					canPerform = true;
					auto lhSpell = a_p->em->GetLHSpell();
					auto rhSpell = a_p->em->GetRHSpell();
					// Cost multiplier from here:
					// https://en.uesp.net/wiki/Skyrim:Magic_Overview#Dual-Casting
					float baseCostLH = lhSpell->CalculateMagickaCost(a_p->coopActor.get());
					float baseCostRH = rhSpell->CalculateMagickaCost(a_p->coopActor.get());
					baseCost = baseCostLH + baseCostRH;
					// Set individual costs for magicka consumption for each casting action.
					a_p->pam->paStatesList
					[!InputAction::kCastLH - !InputAction::kFirstAction].avBaseCost = baseCostLH;
					a_p->pam->paStatesList
					[!InputAction::kCastRH - !InputAction::kFirstAction].avBaseCost = baseCostRH;
				}

				break;
			}
			case SpecialActionType::kDodge:
			{
				avToCheck = RE::ActorValue::kStamina;
				// WIP:
				// Custom dodge cost that factors LS displacement, 
				// current equipped weight, 
				// and the player's carryweight.
				float carryWeightRatioMult = 
				(
					0.1f +
					min
					(
						0.9f, 
						sqrtf
						(
							a_p->coopActor->GetEquippedWeight() / 
							Util::GetFullAVAmount
							(
								a_p->coopActor.get(), RE::ActorValue::kCarryWeight
							)
						)
					)
				);
				// Dash dodging costs more when the LS is displaced further from center 
				// (longer dodge).
				const float& lsMag = 
				(
					glob.cdh->GetAnalogStickState(a_p->controllerID, true).normMag
				);
				// Minimum dodge cost is half the max cost to prevent spamming of short dodges.
				float dashDodgeCommitmentMult = 
				(
					(lsMag == 0.0f) || 
					(
						!Settings::bUseDashDodgeSystem && 
						ALYSLC::TKDodgeCompat::g_tkDodgeInstalled
					) ?
					1.0f : 
					std::clamp(lsMag, 0.5f, 1.0f)
				);
				costToPerform = 
				baseCost = a_p->pam->baseStamina * carryWeightRatioMult * dashDodgeCommitmentMult;
				// Set the Dodge action's cost.
				a_p->pam->paStatesList
				[!InputAction::kDodge - !InputAction::kFirstAction].avBaseCost = baseCost;
				// Must not be on cooldown.
				canPerform |= pam->secsTotalStaminaRegenCooldown == 0.0f;
				break;
			}
			case SpecialActionType::kDualCast:
			{
				// Hand casting magicka cost already handled by the game,
				// but store base cost to grant XP upon casting the spells, as needed.
				canPerform = true;
				avToCheck = RE::ActorValue::kMagicka;
				// Cost multiplier from here:
				// https://en.uesp.net/wiki/Skyrim:Magic_Overview#Dual-Casting
				// Same spells in both hands, same cost.
				auto rhSpell = a_p->em->GetRHSpell();
				// Save base cost.
				baseCost = 2.8f * rhSpell->CalculateMagickaCost(a_p->coopActor.get());
				a_p->pam->paStatesList
				[!InputAction::kSpecialAction - !InputAction::kFirstAction].avBaseCost = baseCost;
				break;
			}
			case SpecialActionType::kQuickCast:
			{
				avToCheck = RE::ActorValue::kMagicka;
				// Must have a quick slot spell equipped.
				auto qsSpell = a_p->em->equippedForms[!EquipIndex::kQuickSlotSpell]; 
				if (qsSpell && qsSpell->Is(RE::FormType::Spell))
				{
					auto asSpell = qsSpell->As<RE::SpellItem>();
					baseCost = asSpell->CalculateMagickaCost(a_p->coopActor.get());
					costToPerform = baseCost * Settings::vfMagickaCostMult[a_p->playerID];
					if (asSpell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration)
					{
						costToPerform *= *g_deltaTimeRealTime;
					}

					// Set the QuickSlotCast action's cost.
					a_p->pam->paStatesList
					[
						!InputAction::kQuickSlotCast - !InputAction::kFirstAction
					].avBaseCost = baseCost;
					// Since the spell is cast with a magic caster directly,
					// there is no input event to send for P1, 
					// so we need to perform a magicka check here.
					canPerform |= pam->currentMagicka - costToPerform > 0.0f;
				}

				break;
			}
			case SpecialActionType::kBlock:
			case SpecialActionType::kCycleOrPlayEmoteIdle:
			case SpecialActionType::kGetUp:
			case SpecialActionType::kNone:
			case SpecialActionType::kQuickItem:
			case SpecialActionType::kFlop:
			case SpecialActionType::kToggleGrabbedRefrCollisions:
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
			paState.avBaseCost = baseCost;
			// Notify the player that they do not have sufficient magicka 
			// to cast with both hands, dual cast, or quick cast.
			if (!canPerform && 
				a_p->pam->GetPlayerActionInputJustPressed(InputAction::kSpecialAction) &&
				avToCheck == RE::ActorValue::kMagicka)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kGeneralNotification,
					fmt::format("P{}: Not enough magicka!", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);

				// Everyone's favorite sound.
				RE::PlaySound("MAGFailSD");
			}

			return canPerform;
		}

		bool CheckForKillmove(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Check if the player can perform a killmove with the given action,
			// and attempt to perform one if conditions hold.
			// Return true if a killmove was performed.
			
			// If not using the killmoves system, nothing to do here.
			if (!Settings::bUseKillmovesSystem) 
			{
				return false;
			}

			bool performedKillmove = false;
			// Only check if not already in a killmove and not mounted.
			if (a_p->coopActor->IsInKillMove() || a_p->coopActor->IsOnMount())
			{
				return false;
			}

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
			// Only perform if targeting a valid actor 
			// and is another killmove-able player or an NPC that isn't essential.
			if ((!validState) || 
				(!canKillmoveOtherPlayer && (isOtherPlayer || targetActorPtr->IsEssential()))) 
			{
				return false;
			}

			// Ensure target is within actor's reach.
			float distToTarget = 
			(
				a_p->coopActor->data.location.GetDistance(targetActorPtr->data.location)
			);
			float reach = a_p->em->GetMaxWeapReach();
			if (distToTarget > reach) 
			{
				return false;
			}

			// Get skeleton name for target and check if it is a character.
			bool hasCharacterSkeleton = false;
			std::string skeleName = std::string("");
			if (auto targetRace = targetActorPtr->GetRace(); targetRace)
			{
				Util::GetSkeletonModelNameForRace(targetRace, skeleName);
				// No assigned racial killmoves if skeleton is not supported.
				hasCharacterSkeleton = Hash(skeleName) == "character"_h;
			}

			// Use generic killmoves on characters or on any race, if the setting is active.
			bool canUseGenericKillmoves = 
			(
				hasCharacterSkeleton || Settings::bUseGenericKillmovesOnUnsupportedNPCs
			);
			// Get max health for the target.
			float maxHealth = Util::GetFullAVAmount(targetActorPtr.get(), RE::ActorValue::kHealth);
			// Target actor's current health must be below the killmove health threshold.
			bool canPerformRegularKillmove = 
			(
				(
					!a_p->coopActor->IsSneaking() || a_p->isTransformed
				) && 
				(
					targetActorPtr->GetActorValue(RE::ActorValue::kHealth) <= 
					maxHealth * Settings::fKillmoveHealthFraction
				)
			);
			// Can perform sneak killmove if sneaking and targeting another NPC
			// that does not fully detect this player.
			bool canPerformSneakKillmove = 
			(
				!canPerformRegularKillmove &&
				!isOtherPlayer &&
				hasCharacterSkeleton && 
				a_p->coopActor->IsSneaking() && 
				Util::GetDetectionPercent(a_p->coopActor.get(), targetActorPtr.get()) != 100.0f
			);
			if (canPerformRegularKillmove)
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
					bool usingShield = 
					(
						a_action == InputAction::kBash && a_p->em->HasShieldEquipped()
					);
					// Can only perform if P1 or when not toggling levitation.
					if ((isVampireLord && canUseGenericKillmoves) && 
						(a_p->isPlayer1 || !a_p->isTogglingLevitationStateTaskRunning))
					{
						performedKillmove = PlayKillmoveFromList
						(
							a_p, 
							targetActorPtr.get(), 
							isOtherPlayer, 
							hasCharacterSkeleton, 
							glob.genericKillmoveIdles[!KillmoveType::kVampireLord]
						);
					}
					else if (isWerewolf && canUseGenericKillmoves)
					{									
						performedKillmove = PlayKillmoveFromList
						(
							a_p,
							targetActorPtr.get(),
							isOtherPlayer,
							hasCharacterSkeleton,
							glob.genericKillmoveIdles[!KillmoveType::kWerewolf]
						);
					}
					else if (usingShield && canUseGenericKillmoves)
					{
						performedKillmove = PlayKillmoveFromList
						(
							a_p, 
							targetActorPtr.get(), 
							isOtherPlayer, 
							hasCharacterSkeleton, 
							glob.genericKillmoveIdles[!KillmoveType::kShield]
						);
					}
					else
					{
						// All other weapon types' killmoves.
						auto weapType = RE::WEAPON_TYPE::kHandToHandMelee;
						bool physAttackAction = 
						{
							a_action == InputAction::kAttackRH || 
							a_action == InputAction::kAttackLH ||
							a_action == InputAction::kPowerAttackRH || 
							a_action == InputAction::kPowerAttackLH ||
							a_action == InputAction::kPowerAttackDual 
						};

						// Get weapon type based on triggering action.
						if (physAttackAction)
						{
							if (auto rhWeap = a_p->em->GetRHWeapon(); 
								(rhWeap) && 
								(a_action == InputAction::kAttackRH ||
								a_action == InputAction::kPowerAttackRH || 
								a_action == InputAction::kPowerAttackDual))
							{
								weapType = rhWeap->GetWeaponType();
							}
							else if (auto lhWeap = a_p->em->GetLHWeapon(); 
									 (lhWeap) && 
									 (a_action == InputAction::kAttackLH || 
									 a_action == InputAction::kPowerAttackLH))
							{
								weapType = lhWeap->GetWeaponType();
							}
						}

						// Weapon killmoves only.
						bool playWeaponKillmove = (physAttackAction && targetActorPtr->GetRace());
						bool playMeleeSpellCastKillmove = 
						(
							(!playWeaponKillmove) &&
							(
								Settings::bUseUnarmedKillmovesForSpellcasting && 
								canUseGenericKillmoves
							) && 
							(a_action == InputAction::kCastLH || a_action == InputAction::kCastRH)
						);
						if (playWeaponKillmove)
						{
							auto killmoveType = KillmoveType::kNone;
							bool assignSkeletonBasedKillmoves = !skeleName.empty();
							// If skeleton is valid, check if any killmoves were assigned to it.
							if (assignSkeletonBasedKillmoves)
							{
								// Get killmoves list for this skeleton.
								// List is then indexed with weap type.
								const auto& killmovesPerWeapTypeList = 
								(
									glob.skeletonKillmoveIdlesMap.at(Hash(skeleName))
								);
								assignSkeletonBasedKillmoves = std::any_of
								(
									killmovesPerWeapTypeList.begin(), 
									killmovesPerWeapTypeList.end(),
									[](const std::vector<RE::TESIdleForm*>& a_killmovesList) 
									{
										return a_killmovesList.size() != 0;
									}
								);
							}

							if (assignSkeletonBasedKillmoves)
							{
								// Choose killmove for this supported skeleton 
								// based on the current weapon type.
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

								// Must use 1H or 2H melee weapons 
								// to perform race-specific killmoves.
								if (is1HMWeap || is2HMWeap)
								{
									killmoveType = 
									(
										is1HMWeap ? KillmoveType::k1H : KillmoveType::k2H
									);
									performedKillmove = PlayKillmoveFromList
									(
										a_p,
										targetActorPtr.get(), 
										isOtherPlayer, 
										hasCharacterSkeleton,
										glob.skeletonKillmoveIdlesMap.at
										(
											Hash(skeleName)
										)[!killmoveType]
									);
								}
							}
							else if (canUseGenericKillmoves)
							{
								// Choose killmove from generic killmoves list 
								// if there is no supported skeleton.
								auto killmoveTypesList = 
								(
									GlobalCoopData::KILLMOVE_TYPES_FOR_WEAP_TYPE.at(weapType)
								);
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
								rand = std::clamp
								(
									generator() / (float)((std::mt19937::max)()), 
									0.0f, 
									0.99f
								);
								// Randomize starting index.
								uint32_t i = (uint32_t)(typesListSize * rand);
								uint8_t typesChecked = 0;
								// Keep checking until a killmove is performed
								// or all types are checked.
								while (!performedKillmove && typesChecked < typesListSize)
								{
									killmoveType = killmoveTypesList[i];
									performedKillmove = PlayKillmoveFromList
									(
										a_p, 
										targetActorPtr.get(),
										isOtherPlayer,
										hasCharacterSkeleton, 
										glob.genericKillmoveIdles[!killmoveType]
									);
													
									// Wrap around to front/back 
									// depending on if iterating forwards/backwards.
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
						else if (playMeleeSpellCastKillmove)
						{
							// Unarmed killmoves when casting spells at a close range target.
							auto killmoveType = KillmoveType::kH2H;
							// Stop casting before performing the killmove because the player 
							// will otherwise continue casting for free afterwards, 
							// even if all cast binds are released.
							if (!a_p->isPlayer1) 
							{
								FinishCasting(a_p, true, true);
							}

							performedKillmove = PlayKillmoveFromList
							(
								a_p,
								targetActorPtr.get(),
								isOtherPlayer,
								hasCharacterSkeleton,
								glob.genericKillmoveIdles[!killmoveType]
							);

							// Set request for melee spellcast killmove.
							if (performedKillmove)
							{
								a_p->pam->reqMeleeSpellcastKillmove = true;
							}
						}
					}
				}
			}
			else if (canPerformSneakKillmove)
			{
				// Choose sneak kill move on a non-player target that has not detected the player:
				// - No health threshold requirement.
				// - No sneak killmoves for non-humanoids.
				// - No shield sneak killmoves.
				// - And finally, if enabled, unarmed killmoves for spells.
				bool supportedAction = 
				{
					(a_action == InputAction::kAttackRH || a_action == InputAction::kAttackLH) ||
					(
						(Settings::bUseUnarmedKillmovesForSpellcasting) && 
						(a_action == InputAction::kCastRH || a_action == InputAction::kCastLH)
					)
				};
				// Must be behind the target actor.
				float facingAngDiff = fabsf
				(
					Util::NormalizeAngToPi
					(
						targetActorPtr->GetHeading(true) - a_p->coopActor->GetHeading(true)
					)
				);
				// 45 degree window.
				if (supportedAction && facingAngDiff <= PI / 4.0f)
				{
					auto weapType = RE::WEAPON_TYPE::kHandToHandMelee;
					// Get weapon type for the triggering action.
					if (a_action == InputAction::kAttackRH || a_action == InputAction::kAttackLH)
					{
						if (auto rhWeap = a_p->em->GetRHWeapon(); 
							a_action == InputAction::kAttackRH && rhWeap)
						{
							weapType = rhWeap->GetWeaponType();
						}
						else if (auto lhWeap = a_p->em->GetLHWeapon(); 
								 a_action == InputAction::kAttackLH && lhWeap)
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

					performedKillmove = PlayKillmoveFromList
					(
						a_p,
						targetActorPtr.get(),
						isOtherPlayer,
						hasCharacterSkeleton, 
						glob.genericKillmoveIdles[!killmoveType]
					);
				}
			}

			return performedKillmove;
		}

		const bool EnoughOfAVToPerformPA
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		)
		{
			// Does the player have enough health/magicka/stamina to perform the given action?

			const auto& pam = a_p->pam;
			// Total base cost.
			float baseCost = 0.0f;
			// Cost, after player cost mult application, 
			// once the player starts performing the action.
			float costToPerform = 0.0f;
			// Default to 'can perform' for all actions if in god mode.
			bool canPerform = a_p->isInGodMode;
			RE::ActorValue avToCheck = RE::ActorValue::kNone;
			if (a_action == InputAction::kActivate)
			{
				avToCheck = RE::ActorValue::kHealth;
				// Revive health cost.
				baseCost = GetPABaseHealthCost(a_p, a_action);
				costToPerform = baseCost * *g_deltaTimeRealTime;
				canPerform |= 
				(
					pam->currentHealth - costToPerform > Settings::fMinHealthWhileReviving
				);
			}
			else if ((a_action == InputAction::kCastLH && a_p->em->GetLHSpell()) || 
				     (a_action == InputAction::kCastRH && a_p->em->GetRHSpell()) || 
					 (a_action == InputAction::kQuickSlotCast && a_p->em->quickSlotSpell))
			{
				avToCheck = RE::ActorValue::kMagicka;
				// Base cost.
				baseCost = GetPABaseMagickaCost(a_p, a_action);
				// NOTE:
				// Must scale by the player's magicka cost mult here.
				// This 'true' cost is applied later in the CheckClampDamageModifier() hook
				// by rescaling the base cost by this modifier again.
				bool isQuickslotCastCheck =  
				(
					a_action == InputAction::kQuickSlotCast && a_p->em->quickSlotSpell
				);
				// Only check for adequate magicka if performing a quick slot cast,
				// since the game does not charge magicka while the instant caster
				// preps for the cast and we'll have to directly expend magicka later. 
				// Also do not want to check for adequate magicka here for hand casts, 
				// as the full cost of the spell would be deducted from the current magicka amount, 
				// which is lowered by the game as the spell charges.
				costToPerform = 	
				(
					isQuickslotCastCheck ? 
					baseCost * Settings::vfMagickaCostMult[a_p->playerID] :
					0.0f
				);
				bool isConcSpell = 
				{
					(
						a_action == InputAction::kCastLH && 
						a_p->em->GetLHSpell() && 
						a_p->em->GetLHSpell()->GetCastingType() == 
						RE::MagicSystem::CastingType::kConcentration
					) ||
					(
						a_action == InputAction::kCastRH &&
						a_p->em->GetRHSpell() && 
						a_p->em->GetRHSpell()->GetCastingType() ==
						RE::MagicSystem::CastingType::kConcentration
					) ||
					(
						isQuickslotCastCheck && 
						a_p->em->quickSlotSpell->GetCastingType() ==
						RE::MagicSystem::CastingType::kConcentration
					)
				};

				// Per frame if a concentration spell.
				if (isConcSpell)
				{
					costToPerform *= *g_deltaTimeRealTime;
				}

				// The hand caster magicka check is done by the game for P1 
				// after input event processing, so we'll skip the magicka cost check 
				// and just send the button events. Still have to check if P1 has enough magicka
				// for quick slot cast request though.
				canPerform |= 
				(
					(a_p->isPlayer1 && !isQuickslotCastCheck) ||
					(pam->currentMagicka - costToPerform > 0.0f)
				);
				// Notify the player that they do not have sufficient magicka to quick slot cast.
				if (!canPerform && 
					pam->currentMagicka > 0.0f && 
					pam->GetPlayerActionInputJustPressed(a_action) &&
					isQuickslotCastCheck &&
					avToCheck == RE::ActorValue::kMagicka)
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kGeneralNotification,
						fmt::format("P{}: Not enough magicka!", a_p->playerID + 1),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kStealthState,
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);

					RE::PlaySound("MAGFailSD");
				}
			}
			else
			{
				avToCheck = RE::ActorValue::kStamina;
				baseCost = GetPABaseStaminaCost(a_p, a_action);
				// Check if the player has enough stamina if the cost is non-zero.
				if (baseCost != 0.0f) 
				{
					// Can perform if in god mode.
					// Check for sufficient stamina, if P1,
					// or 0 cooldown for companion players.
					canPerform |= 
					(
						(a_p->isPlayer1 && pam->currentStamina > 0.0f) ||
						(!a_p->isPlayer1 && pam->secsTotalStaminaRegenCooldown == 0.0f)
					);
				}
				else
				{
					// No cost, can perform.
					canPerform = true;
				}
			}

			// Cache base cost so that it does not have to be recomputed later 
			// when expending health/magicka/stamina 
			// once the action's corresponding start animation triggers.
			// Base cost is also used when determining the XP rewarded for a spellcast.
			pam->paStatesList[!a_action - !InputAction::kFirstAction].avBaseCost = baseCost;
			return canPerform;
		}

		void EquipBoundWeapon
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			RE::SpellItem* a_boundWeapSpell,
			RE::TESObjectWEAP* a_boundWeap,
			RE::BGSEquipSlot* a_slot, 
			RE::MagicCaster* a_caster
		)
		{
			// Equip the given bound weapon to the given slot,
			// either directly or by way of the provided bound weapon spell.
			// Set bound weapon timer and request data.

			SPDLOG_DEBUG("[PAFH] EquipBoundWeapon: {}: Equip {} to slot 0x{:X}.", 
				a_p->coopActor->GetName(),
				a_boundWeap ? a_boundWeap->GetName() : "NONE",
				a_slot->formID);

			if (!a_boundWeapSpell || !a_boundWeap || !a_slot)
			{
				return;
			}

			// Most cursed thing I've ever done, aside from this entire mod of course.
			// But nothing else works.
			// Console command equip funcs worked whereas the CLib ones did not for some reason
			// (likely due to having to equip both the underlying bound weapon form 
			// and its enchantment for the visuals).
			// Must also have a delay between executing console commands, 
			// otherwise the second command does not execute successfully.
			a_p->taskRunner->AddTask
			(
				[a_p, a_boundWeap, a_boundWeapSpell, a_slot, a_caster]()
				{
					const auto scriptFactory = 
					(
						RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
					);
					const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
					if (script)
					{
						// POTENTIAL THREADING ISSUE:
						// If the player attempts to equip another form
						// while this code is executing, the results can be unpredictable 
						// because the main thread and the task thread running this code 
						// may conflict when modifying the desired equipped forms list.
						// Should not ever happen though since this code will not run 
						// if the player is already equipping/unequipping a form.

						// Equipped forms list indices where the LH and RH forms are equipped.
						std::vector<EquipIndex> lhEquipIndices{ };
						std::vector<EquipIndex> rhEquipIndices{ };
						// Keep the original desired LH and RH forms.
						auto lhForm = a_p->em->desiredEquippedForms[!EquipIndex::kLeftHand];
						auto rhForm = a_p->em->desiredEquippedForms[!EquipIndex::kRightHand];
						Util::AddSyncedTask
						(
							[
								a_p, 
								a_slot, 
								a_boundWeap,
								lhForm, 
								rhForm,
								&lhEquipIndices,
								&rhEquipIndices
							]() 
							{
								// Get equip indices and clear out both hands' desired forms.
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
									SPDLOG_DEBUG
									(
										"[PAFH] EquipBoundWeapon: {}: 2H bound weap req.",
										a_p->coopActor->GetName()
									);
									a_p->pam->boundWeapReq2H = true;
									a_p->pam->boundWeapReqLH =
									a_p->pam->boundWeapReqRH = false;
									a_p->em->lastReqBoundWeapLH = 
									a_p->em->lastReqBoundWeapRH = a_boundWeap;
									a_p->lastBoundWeapon2HReqTP = SteadyClock::now();
								}
								else if (a_slot == glob.leftHandEquipSlot)
								{
									SPDLOG_DEBUG
									(
										"[PAFH] EquipBoundWeapon: {}: LH bound weap req.",
										a_p->coopActor->GetName()
									);
									a_p->pam->boundWeapReqLH = true;
									a_p->em->lastReqBoundWeapLH = a_boundWeap;
									a_p->lastBoundWeaponLHReqTP = SteadyClock::now();
								}
								else
								{
									SPDLOG_DEBUG
									(
										"[PAFH] EquipBoundWeapon: {}: RH bound weap req.",
										a_p->coopActor->GetName()
									);
									a_p->pam->boundWeapReqRH = true;
									a_p->em->lastReqBoundWeapRH = a_boundWeap;
									a_p->lastBoundWeaponRHReqTP = SteadyClock::now();
								}

								SPDLOG_DEBUG
								(
									"[PAFH] EquipBoundWeapon: {}: Bound weap reqs: {}, {}, {}.",
									a_p->coopActor->GetName(),
									a_p->pam->boundWeapReq2H,
									a_p->pam->boundWeapReqLH,
									a_p->pam->boundWeapReqRH
								);
							}
						);
						
						// Equip slot to restore after the weapon is fully equipped.
						// Only applies to 1H weapons.
						RE::BGSEquipSlot* originalEquipSlot = nullptr;
						std::string commandStr;
						bool isBow = a_boundWeap->IsBow();
						if (isBow)
						{
							// Equip bound bow directly with equip console command.
							// Equipping through casting the bound bow's spell does not work.
							Util::AddSyncedTask
							(
								[&commandStr, &script, a_boundWeap, a_p]() 
								{
									commandStr = fmt::format
									(
										"equipitem {:X}", a_boundWeap->formID
									);
									script->SetCommand(commandStr.c_str());
									script->CompileAndRun(a_p->coopActor.get());
								}
							);
						}
						else
						{
							// Otherwise, equip the bound weapon via spellcast.

							// Set equip slot for the weapon before equipping,
							// so the EquipObject hook can compare the request's equip slot
							// to the equip parameter's equip slot.

							Util::AddSyncedTask
							(
								[
									a_p, 
									a_boundWeapSpell,
									a_boundWeap,
									a_slot, 
									a_caster,
									&originalEquipSlot
								]() 
								{
									// If the requested slot differs from the current one,
									// save the current equip slot to restore later.
									if (a_slot != a_boundWeap->GetEquipSlot())
									{
										originalEquipSlot = a_boundWeap->GetEquipSlot();
										a_boundWeap->SetEquipSlot(a_slot);
									}

									a_caster->CastSpellImmediate
									(
										a_boundWeapSpell, 
										false,
										a_p->coopActor.get(), 
										1.0f, 
										false, 
										0.0f, 
										a_p->coopActor.get()
									);
								}
							);
						}

						// Equip bound arrow by finding a bound arrow type
						// that matches with the bound bow.
						if (isBow)
						{
							RE::FormID boundArrowFID = 0x10B0A7;
							// Get spell form based on casting source 
							// (either from hand or quick slot).
							RE::TESForm* boundBowForm = nullptr;
							if (a_caster == a_p->coopActor->GetMagicCaster
								(
									RE::MagicSystem::CastingSource::kRightHand
								)) 
							{
								boundBowForm = rhForm;
							}
							else if (a_caster == a_p->coopActor->GetMagicCaster
									 (
										RE::MagicSystem::CastingSource::kLeftHand
									 ))
							{
								boundBowForm = lhForm;
							}
							else
							{
								boundBowForm = 
								(
									a_p->em->desiredEquippedForms[!EquipIndex::kQuickSlotSpell]
								);
							}

							auto boundBowSpell = 
							(
								boundBowForm ? boundBowForm->As<RE::SpellItem>() : nullptr
							); 
							if (boundBowSpell && 
								boundBowSpell->avEffectSetting && 
								boundBowSpell->avEffectSetting->data.projectileBase) 
							{
								// Get the base projectile to match with the base projectile 
								// from one of the bound ammo choices.
								// Couldn't find a way to map from BGSProjectile to Projectile, 
								// so a linear (bleh) reverse of that mapping was required.
								auto baseProj = 
								(
									boundBowSpell->avEffectSetting->data.projectileBase
								);
								for (auto boundAmmo : glob.boundArrowAmmoList) 
								{
									if (boundAmmo->data.projectile == baseProj) 
									{
										boundArrowFID = boundAmmo->formID;
										break;
									}
								}
							}

							// Equip via console command.
							Util::AddSyncedTask
							(
								[&commandStr, script, boundArrowFID, a_p]() 
								{
									commandStr = fmt::format("equipitem {:X}", boundArrowFID);
									script->SetCommand(commandStr.c_str());
									script->CompileAndRun(a_p->coopActor.get());
									a_p->pam->ReadyWeapon(true);
								}
							);
						}

						// Wait until done equipping/unequipping before reducing magicka
						// and restoring desired forms.
						bool isEquipping = true;
						bool isUnequipping = true;
						SteadyClock::time_point waitTP = SteadyClock::now();
						while ((Util::GetElapsedSeconds(waitTP) < 2.0f) && 
							   (isEquipping || isUnequipping))
						{
							std::this_thread::sleep_for(0.25s);
							a_p->coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
							a_p->coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
						}

						std::this_thread::sleep_for(0.5s);

						Util::AddSyncedTask
						(
							[
								a_p, 
								a_boundWeapSpell, 
								a_boundWeap,
								lhForm, 
								rhForm,
								&lhEquipIndices,
								&rhEquipIndices,
								originalEquipSlot
							]() 
							{
								// Set equip slot to either hand, if a one-handed weapon,
								// or to both hands, if a two-handed weapon.
								if (a_boundWeap && originalEquipSlot)
								{
									if (originalEquipSlot == glob.bothHandsEquipSlot)
									{
										a_boundWeap->SetEquipSlot(glob.bothHandsEquipSlot);
									}
									else
									{
										a_boundWeap->SetEquipSlot(glob.eitherHandEquipSlot);
									}
								}

								// Expend magicka.
								a_p->pam->ModifyAV
								(
									RE::ActorValue::kMagicka,
									-a_boundWeapSpell->CalculateMagickaCost(a_p->coopActor.get()) *
									Settings::vfMagickaCostMult[a_p->playerID]
								);

								// Restore LH and RH desired forms to their previous indices.
								// Will be re-equipped by the equip manager's main task.
								for (const auto& equipIndex : lhEquipIndices)
								{
									a_p->em->desiredEquippedForms[!equipIndex] = lhForm;
								}

								for (const auto& equipIndex : rhEquipIndices)
								{
									a_p->em->desiredEquippedForms[!equipIndex] = rhForm;
								}
							}
						);

						// Cleanup.
						delete script;
					}
				}
			);
		}

		void EquipHotkeyedForm
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			RE::TESForm* a_hotkeyedForm, 
			EquipIndex&& a_equipIndex
		)
		{
			// Equip the previously selected hotkeyed form to the given equip index, if supported; 
			// otherwise, equip to the default corresponding index,
			// or inform the player that the item could not be equipped or is already equipped.

			// NOTE:
			// Four actions' composing inputs are used to signal which slot 
			// to equip the selected hotkeyed form into:
			// Left Trigger -> LH
			// Right Trigger -> RH
			// Left Bumper -> Item Quick Slot
			// Right Bumper -> Spell Quick Slot

			// Refresh favorited forms, if necessary.
			// Refresh for P1 if the hotkey form is not favorited anymore.
			// Refresh for companion players if the form is physical and not favorited anymore.
			bool refreshFavorites = 
			(
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
			);
			if (refreshFavorites) 
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: {} is no longer favorited. Refreshing hotkeys.", 
						a_p->playerID + 1, a_hotkeyedForm->GetName()
					),
					{
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kHotkeySelection, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
				);

				// Do not update magical favorites for a companion player
				// because P1's magical favorites are active during regular gameplay.
				a_p->em->UpdateFavoritedFormsLists(!a_p->isPlayer1);
				return;
			}

			// Notify the player that the hotkeyed form 
			// is already equipped, or the slot is already empty 
			// at the requested equip index and return.
			if (a_hotkeyedForm == a_p->em->equippedForms[!a_equipIndex])
			{
				if (a_hotkeyedForm)
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format
						(
							"P{}: {} is already equipped",
							a_p->playerID + 1,
							a_hotkeyedForm->GetName()
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
					);
				}
				else
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format
						(
							"P{}: {} is already empty",
							a_p->playerID + 1,
							a_equipIndex == EquipIndex::kRightHand ? 
							"Right hand" :
							a_equipIndex == EquipIndex::kLeftHand ?
							"Left hand" : 
							a_equipIndex == EquipIndex::kQuickSlotItem ?
							"Item quick slot" :
							"Spell quick slot"
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
					);
				}

				return;
			}

			auto aem = RE::ActorEquipManager::GetSingleton(); 
			if (a_equipIndex == EquipIndex::kRightHand) 
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
								fmt::format
								(
									"P{}: Equipping {} in both hands",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
							slot = glob.bothHandsEquipSlot;
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} in the right hand",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState,
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
						}

						a_p->em->EquipForm
						(
							a_hotkeyedForm, EquipIndex::kRightHand, nullptr, 1, slot
						);
					}
					else if (auto spell = a_hotkeyedForm->As<RE::SpellItem>(); spell)
					{
						if (spell->equipSlot == glob.voiceEquipSlot) 
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} to the voice slot",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
							slot = glob.voiceEquipSlot;
						}
						else if (spell->equipSlot == glob.bothHandsEquipSlot)
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} in both hands", 
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection,
									CrosshairMessageType::kStealthState,
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
							slot = glob.bothHandsEquipSlot;
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} in the right hand", 
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
						}

						a_p->em->EquipSpell(a_hotkeyedForm, EquipIndex::kRightHand, slot);
					}
					else if (auto shout = a_hotkeyedForm->As<RE::TESShout>(); shout)
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Equipping {} to the voice slot", 
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
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
								fmt::format
								(
									"P{}: Cannot equip {} in the right hand",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
							);
							return;
						}

						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Equipping {} to an armor slot", 
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
						);
						a_p->em->EquipArmor(a_hotkeyedForm);
					}
					else if (a_hotkeyedForm->As<RE::TESObjectLIGH>())
					{
						// Cannot equip torch in the right hand.
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Cannot equip {} in the right hand",
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone,
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
						);
						return;
					}
					else if (auto alchemyItem = a_hotkeyedForm->As<RE::AlchemyItem>(); alchemyItem)
					{
						if (alchemyItem->IsPoison())
						{
							// Apply poison if the corresponding hand form 
							// has inventory entry data.
							auto weapInvData = a_p->coopActor->GetEquippedEntryData(false);
							if (weapInvData)
							{
								weapInvData->PoisonObject(alchemyItem, 1);
								// Remove after applying the poison.
								a_p->coopActor->RemoveItem
								(
									alchemyItem, 
									1, 
									RE::ITEM_REMOVE_REASON::kRemove, 
									nullptr, 
									nullptr
								);

								a_p->tm->SetCrosshairMessageRequest
								(
									CrosshairMessageType::kEquippedItem,
									fmt::format
									(
										"P{}: Applying {} to {}", 
										a_p->playerID + 1, 
										a_hotkeyedForm->GetName(),
										weapInvData->object ?
										weapInvData->object->GetName() :
										"right hand weapon"
									),
									{ 
										CrosshairMessageType::kNone, 
										CrosshairMessageType::kHotkeySelection, 
										CrosshairMessageType::kStealthState, 
										CrosshairMessageType::kTargetSelection 
									},
									max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
								);
							}
						}
						else if (aem)
						{
							// Eat it. Yum.
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Consuming {}",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
							aem->EquipObject(a_p->coopActor.get(), alchemyItem, nullptr, 1);
						}
					}
					else if (a_hotkeyedForm->IsAmmo())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Equipping {} as ammo",
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
						);
						a_p->em->EquipAmmo(a_hotkeyedForm);
					}
					else if (aem && a_hotkeyedForm->As<RE::TESBoundObject>())
					{
						// Equip everything else.
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Equipping {}", a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
						);
						aem->EquipObject
						(
							a_p->coopActor.get(), a_hotkeyedForm->As<RE::TESBoundObject>()
						);
					}
				} 
				else if (auto rhForm = a_p->em->equippedForms[!EquipIndex::kRightHand]; rhForm)
				{
					// Unequip the current hand form if no chosen form in the hotkey slot.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format
						(
							"P{}: Emptied right hand", 
							a_p->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
					);
					a_p->em->UnequipFormAtIndex(EquipIndex::kRightHand);
				}
				else
				{
					// Notify the player that the right hand is already empty.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Right hand is already empty", a_p->playerID + 1),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
					);
				}
			}
			else if (a_equipIndex == EquipIndex::kLeftHand)
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
								fmt::format
								(
									"P{}: Equipping {} in both hands", 
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
							slot = glob.bothHandsEquipSlot;
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} in the left hand", 
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
						}

						a_p->em->EquipForm
						(
							a_hotkeyedForm, EquipIndex::kLeftHand, nullptr, 1, slot
						);
					}
					else if (auto spell = a_hotkeyedForm->As<RE::SpellItem>(); spell)
					{
						if (spell->equipSlot == glob.voiceEquipSlot) 
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} to the voice slot", 
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection,
									CrosshairMessageType::kStealthState,
									CrosshairMessageType::kTargetSelection
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
							slot = glob.voiceEquipSlot;
						}
						else if (spell->equipSlot == glob.bothHandsEquipSlot)
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} in both hands",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone,
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
							slot = glob.bothHandsEquipSlot;
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} in the left hand",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
						}

						a_p->em->EquipSpell(a_hotkeyedForm, EquipIndex::kLeftHand, slot);
					}
					else if (auto shout = a_hotkeyedForm->As<RE::TESShout>(); shout)
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Equipping {} to the voice slot",
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection,
								CrosshairMessageType::kStealthState,
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
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
								fmt::format
								(
									"P{}: Equipping {} in the left hand",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} to an armor slot",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection,
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
						}

						a_p->em->EquipArmor(a_hotkeyedForm);
					}
					else if (a_hotkeyedForm->As<RE::TESObjectLIGH>())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Equipping {} in the left hand",
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
						);
						a_p->em->EquipForm(a_hotkeyedForm, EquipIndex::kLeftHand, nullptr, 1);
					}
					else if (auto alchemyItem = a_hotkeyedForm->As<RE::AlchemyItem>(); alchemyItem)
					{
						if (alchemyItem->IsPoison())
						{
							// Apply poison if the corresponding hand form 
							// has inventory entry data.
							auto weapInvData = a_p->coopActor->GetEquippedEntryData(true);
							if (weapInvData)
							{
								weapInvData->PoisonObject(alchemyItem, 1);
								// Remove after applying the poison.
								a_p->coopActor->RemoveItem
								(
									alchemyItem, 
									1, 
									RE::ITEM_REMOVE_REASON::kRemove, 
									nullptr, 
									nullptr
								);
								a_p->tm->SetCrosshairMessageRequest
								(
									CrosshairMessageType::kEquippedItem,
									fmt::format
									(
										"P{}: Applying {} to {}", 
										a_p->playerID + 1, 
										a_hotkeyedForm->GetName(),
										weapInvData->object ?
										weapInvData->object->GetName() :
										"left hand weapon"
									),
									{ 
										CrosshairMessageType::kNone, 
										CrosshairMessageType::kHotkeySelection, 
										CrosshairMessageType::kStealthState, 
										CrosshairMessageType::kTargetSelection 
									},
									max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
								);
							}
						}
						else if (aem)
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Consuming {}",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
							aem->EquipObject(a_p->coopActor.get(), alchemyItem, nullptr, 1);
						}
					}
					else if (a_hotkeyedForm->IsAmmo())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Equipping {} as ammo", 
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
						);
						a_p->em->EquipAmmo(a_hotkeyedForm);
					}
					else if (aem && a_hotkeyedForm->As<RE::TESBoundObject>())
					{
						// Equip everything else.
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Equipping {}", a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
						);
						aem->EquipObject
						(
							a_p->coopActor.get(), a_hotkeyedForm->As<RE::TESBoundObject>()
						);
					}
				}
				else if (auto lhForm = a_p->em->equippedForms[!EquipIndex::kLeftHand]; lhForm)
				{
					// Unequip the current hand form if no chosen form in the hotkey slot.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format
						(
							"P{}: Emptied left hand", 
							a_p->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState,
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
					);
					a_p->em->UnequipFormAtIndex(EquipIndex::kLeftHand);
				}
				else
				{
					// Notify the player that the left hand is already empty.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Left hand is already empty", a_p->playerID + 1),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
					);
				}
			}
			else if (a_equipIndex == EquipIndex::kQuickSlotItem)
			{
				if (a_hotkeyedForm)
				{
					if (auto alchemyItem = a_hotkeyedForm->As<RE::AlchemyItem>(); alchemyItem)
					{
						if (alchemyItem->IsPoison())
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping poison {} to the item quick slot", 
									a_p->playerID + 1, 
									a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kEquippedItem,
								fmt::format
								(
									"P{}: Equipping {} to the item quick slot",
									a_p->playerID + 1, a_hotkeyedForm->GetName()
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kHotkeySelection, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
							);
						}

						// Set directly.
						a_p->em->quickSlotItem = a_hotkeyedForm;
						// Refresh equip state to apply changes.
						a_p->em->RefreshEquipState(RefreshSlots::kAll);
					}
					else
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Cannot equip {} to the item quick slot",
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
						);
					}
				}
				else if (a_p->em->quickSlotItem)
				{
					// Unequip the current quick slot item if no hotkeyed form is selected.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format
						(
							"P{}: Emptied item quick slot",
							a_p->playerID + 1, a_p->em->quickSlotItem->GetName()
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
					);
					// Just clear out directly.
					a_p->em->quickSlotItem = nullptr;
					// Refresh equip state to apply changes.
					a_p->em->RefreshEquipState(RefreshSlots::kAll);
				}
				else
				{
					// Notify the player that the item quick slot is already empty.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Item quick slot is already empty", a_p->playerID + 1),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
					);
				}
			}
			else if (a_equipIndex == EquipIndex::kQuickSlotSpell)
			{
				if (a_hotkeyedForm)
				{
					if (auto spell = a_hotkeyedForm->As<RE::SpellItem>(); spell)
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Equipping {} to the spell quick slot",
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
						);
						// Just set directly.
						a_p->em->quickSlotSpell = spell;
						// Refresh equip state to apply changes.
						a_p->em->RefreshEquipState(RefreshSlots::kAll);
					}
					else
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kEquippedItem,
							fmt::format
							(
								"P{}: Cannot equip {} to the spell quick slot",
								a_p->playerID + 1, a_hotkeyedForm->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kHotkeySelection, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
						);
					}
				}
				else if (a_p->em->quickSlotSpell)
				{
					// Unequip the current quick slot spell if no hotkeyed form is selected.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format
						(
							"P{}: Emptied spell quick slot",
							a_p->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.25f)
					);
					// Just clear out directly.
					a_p->em->quickSlotSpell = nullptr;
					// Refresh equip state to apply changes.
					a_p->em->RefreshEquipState(RefreshSlots::kAll);
				}
				else
				{
					// Notify the player that the spell quick slot is already empty.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Spell quick slot is already empty", a_p->playerID + 1),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kHotkeySelection, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
					);
				}
			}
		}

		void FinishCasting
		(
			const std::shared_ptr<CoopPlayer>& a_p, bool&& a_lhCast, bool&& a_rhCast
		)
		{
			// Finish companion player spell cast in the given hand(s).
			// NOTE: 
			// Ranged package cast data will be cleared in the PAM later on
			// if the player is casting a fire and forget spell,
			// since the package must continue executing after the cast bind is released
			// to cast the spell.

			const auto& pam = a_p->pam;
			auto lhCaster = 
			(
				a_p->coopActor->magicCasters[RE::Actor::SlotTypes::kLeftHand]
			);
			auto rhCaster = 
			(
				a_p->coopActor->magicCasters[RE::Actor::SlotTypes::kRightHand]
			);
			// Need valid hand casters.
			if ((a_lhCast && !lhCaster) || (a_rhCast && !rhCaster))
			{
				SPDLOG_DEBUG
				(
					"[PAFH] ERR: FinishCasting: {}: {} finished. "
					"LH caster invalid: {}, RH caster invalid: {}.", 
					a_p->coopActor->GetName(),
					a_lhCast && a_rhCast ? "2H cast" : a_lhCast ? "LH cast" : "RH cast",
					!lhCaster, !rhCaster
				);
				a_p->pam->ResetPackageCastingState();
				return;
			}
			
			// If casting a fire and forget spell, start the cast here;
			// otherwise, clear the casters state, 
			// which will signal the PAM to stop the cast and reset cast globals.
			// NOTE:
			// Must also check if the player has enough magicka to release the FNF spell,
			// since they may have held the fully-charged spell as their magicka 
			// dwindled below the original cost.
			// Interrupt the cast if this is the case.
			
			// Also, directly place down runes since casting runes
			// with any non-P1 magic caster does not work.
			// The player's ranged package still plays casting animations but no spell is released
			// and the caster never beings casting, 
			// so we cannot handle magicka expenditure via animation event post-cast.
			// As a result, directly deduct magicka here.

			auto lhSpell = a_p->em->GetLHSpell();
			auto rhSpell = a_p->em->GetRHSpell();
			bool is2HSpell = 
			(
				lhSpell &&
				rhSpell && 
				lhSpell == rhSpell &&
				rhSpell->equipSlot == glob.bothHandsEquipSlot
			);
			bool hasEnoughMagicka = false;
			if (is2HSpell && lhCaster && rhCaster)
			{
				// For 2H casts, even though we have the same spell equipped in both hands,
				// the cast bind used to trigger the cast determines 
				// which hand caster releases the spell.
				// The other hand's caster may be inactive or not have the 2H spell 
				// as its current spell, so we have to obtain the active caster
				// loaded with the 2H spell in order to start the cast.
				auto casterUsed = 
				(
					lhCaster->currentSpell == lhSpell ? lhCaster : rhCaster
				);
				// Could not get caster, so cannot start/finish casting.
				if (!casterUsed)
				{
					SPDLOG_DEBUG
					(
						"[PAFH] ERR: FinishCasting: {}: "
						"Neither LH nor RH caster has started casting 2H spell {}.",
						a_p->coopActor->GetName(),
						rhSpell->GetName()
					);
					return;
				}

				float chargedTime = 0.0f;
				float baseChargeTime = rhSpell->GetChargeTime();
				if (casterUsed == lhCaster)
				{
					chargedTime = Util::GetElapsedSeconds(a_p->lastLHCastChargeStartTP);
				}
				else
				{
					chargedTime = Util::GetElapsedSeconds(a_p->lastRHCastChargeStartTP);
				}
				SPDLOG_DEBUG
				(
					"[PAFH] FinishCasting: {}: 2H cast of {}. "
					"Time since charge start: {}. Spell charge time: {}.", 
					a_p->coopActor->GetName(),
					rhSpell->GetName(),
					chargedTime,
					rhSpell->GetChargeTime()
				);

				auto castingType = rhSpell->GetCastingType();
				if (castingType == RE::MagicSystem::CastingType::kFireAndForget)
				{
					if (chargedTime > baseChargeTime)
					{
						casterUsed->StartCastImpl();
						if (Util::HasRuneProjectile(rhSpell))
						{
							if (a_p->isInGodMode ||
								a_p->coopActor->GetActorValue(RE::ActorValue::kMagicka) >= 
								Settings::vfMagickaCostMult[a_p->playerID] * 
								rhSpell->CalculateMagickaCost(a_p->coopActor.get()))
							{
								a_p->pam->CastRuneProjectile(rhSpell);
							}
						}

						return;
					}
					
					pam->castingGlobVars[!CastingGlobIndex::kLH]->value = 0.0f;
					pam->castingGlobVars[!CastingGlobIndex::kRH]->value = 0.0f;
					pam->castingGlobVars[!CastingGlobIndex::k2H]->value = 0.0f;
					casterUsed->InterruptCast(true);
					pam->SetAndEveluatePackage(pam->GetCoopPackage(PackageIndex::kRangedAttack));
				}
			}
			else
			{
				if (lhSpell && lhCaster && a_lhCast)
				{
					SPDLOG_DEBUG
					(
						"[PAFH] FinishCasting: {}: LH cast of {}. "
						"Time since charge start: {}. Spell charge time: {}.", 
						a_p->coopActor->GetName(),
						lhSpell->GetName(),
						Util::GetElapsedSeconds(a_p->lastLHCastChargeStartTP),
						lhSpell->GetChargeTime()
					);

					auto castingType = lhSpell->GetCastingType();
					if (castingType == RE::MagicSystem::CastingType::kFireAndForget)
					{
						if (Util::GetElapsedSeconds(a_p->lastLHCastChargeStartTP) > 
							lhSpell->GetChargeTime()) 
						{
							lhCaster->StartCastImpl();
							if (Util::HasRuneProjectile(lhSpell)) 
							{
								if (a_p->isInGodMode ||
									a_p->coopActor->GetActorValue(RE::ActorValue::kMagicka) >= 
									Settings::vfMagickaCostMult[a_p->playerID] * 
									lhSpell->CalculateMagickaCost(a_p->coopActor.get()))
								{
									a_p->pam->CastRuneProjectile(lhSpell);
								}
							}
						}
						else
						{
							pam->castingGlobVars[!CastingGlobIndex::kLH]->value = 0.0f;
							pam->castingGlobVars[!CastingGlobIndex::k2H]->value = 0.0f;
							lhCaster->InterruptCast(true);
						}
					}
				}

				if (rhSpell && rhCaster && a_rhCast)
				{
					SPDLOG_DEBUG
					(
						"[PAFH] FinishCasting: {}: RH cast of {}. "
						"Time since charge start: {}. Spell charge time: {}.", 
						a_p->coopActor->GetName(),
						rhSpell->GetName(),
						Util::GetElapsedSeconds(a_p->lastRHCastChargeStartTP),
						rhSpell->GetChargeTime()
					);

					auto castingType = rhSpell->GetCastingType();
					if (castingType == RE::MagicSystem::CastingType::kFireAndForget)
					{
						if (Util::GetElapsedSeconds(a_p->lastRHCastChargeStartTP) > 
							rhSpell->GetChargeTime())
						{
							rhCaster->StartCastImpl();
							if (Util::HasRuneProjectile(rhSpell))
							{
								if (a_p->isInGodMode ||
									a_p->coopActor->GetActorValue(RE::ActorValue::kMagicka) >= 
									Settings::vfMagickaCostMult[a_p->playerID] * 
									rhSpell->CalculateMagickaCost(a_p->coopActor.get()))
								{
									a_p->pam->CastRuneProjectile(rhSpell);
								}
							}
						}
						else
						{
							pam->castingGlobVars[!CastingGlobIndex::kRH]->value = 0.0f;
							pam->castingGlobVars[!CastingGlobIndex::k2H]->value = 0.0f;
							rhCaster->InterruptCast(true);
						}
					}
				}
			}
		}

		const float GetPABaseHealthCost
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		)
		{
			// Get health cost for reviving another player.

			float cost = 0.0f;
			switch (a_action)
			{
			case InputAction::kActivate:
			{
				// Reviver player will lose 50% of their base health 
				// if their Restoration level is at 15.
				// Health penalty will scale down to 10% at level 100 Restoration.
				auto resAV = a_p->coopActor->GetActorValue(RE::ActorValue::kRestoration);
				float resAVMult = std::lerp
				(
					0.5f, 0.1f, std::clamp((resAV - 15.0f) / (85.0f), 0.0f, 1.0f)
				);
				cost = 
				(
					(resAVMult) * 
					(
						Util::GetFullAVAmount(a_p->coopActor.get(), RE::ActorValue::kHealth)
					) / (max(1.0f, Settings::fSecsReviveTime))
				);

				break;
			}
			default:
				break;
			}

			return cost;
		}

		const float GetPABaseMagickaCost
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		)
		{
			// Get magicka cost for the spell cast action.

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
				auto qsSpell = a_p->em->equippedForms[!EquipIndex::kQuickSlotSpell]; 
				if (qsSpell && qsSpell->Is(RE::FormType::Spell))
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

		const float GetPABaseStaminaCost
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		)
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
							a_p->coopActor->GetEquippedWeight() / 
							Util::GetFullAVAmount
							(
								a_p->coopActor.get(), RE::ActorValue::kCarryWeight
							)
						)
					)
				);
				// Dash dodging costs more when the LS is displaced further from center 
				// (longer dodge).
				// Minimum dodge cost is half the max cost to prevent spamming of short dodges.
				const float& lsMag = 
				(
					glob.cdh->GetAnalogStickState(a_p->controllerID, true).normMag
				);
				float dashDodgeCommitmentMult = 
				(
					(lsMag == 0.0f) || 
					(
						!Settings::bUseDashDodgeSystem && 
						ALYSLC::TKDodgeCompat::g_tkDodgeInstalled
					) ? 
					1.0f : 
					std::clamp(lsMag, 0.5f, 1.0f)
				);
				cost = a_p->pam->baseStamina * carryWeightRatioMult * dashDodgeCommitmentMult;

				break;
			}
			case InputAction::kBash:
			case InputAction::kPowerAttackDual:
			case InputAction::kPowerAttackLH:
			case InputAction::kPowerAttackRH:
			{
				// WORKAROUND NOTE:
				// This non-zero cost is not used to reduce stamina, 
				// since the game already handles this.
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
					float carryWeightRatio = 
					(
						a_p->coopActor->GetEquippedWeight() / 
						a_p->coopActor->GetActorValue(RE::ActorValue::kCarryWeight)
					);
					cost = a_p->pam->baseStamina * (min(0.9f, sqrtf(carryWeightRatio)) + 0.1f);
				}
				else if (!a_p->mm->isParagliding && !a_p->tm->isMARFing)
				{
					// No cost for paragliding or M.A.R.F as of now.
					
					// https://en.uesp.net/wiki/Skyrim:Stamina
					// Cost for sprinting and shield charging is the same.
					cost = (7.0f * (1.0f + 0.02f * a_p->coopActor->GetEquippedWeight()));
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
			// Eight different slots starting from an RS orientation of 'up'
			// and moving clockwise.

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

		void HandleBoundWeaponEquip
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		)
		{
			// Casting bound weapons with a co-op companion is completely bugged 
			// and needs all the special handling it can get.
			// 
			// Additional info:
			// Bound bow equip via spellcast: 
			// Has a few glitches and weird equip slot issues
			// once the spell spawning the bow is cast.
			// 1. The game tries to auto-equip gear, such as a shield in the LH,
			// which unequips the bound bow right as it spawns. 
			// Auto-equips are mostly blocked in the UnequipObject hook.
			// 2. The magic effect tied to the spell gets removed right after casting, 
			// so even without auto-equipping other gear, the bound bow FX lingers in the LH, 
			// while the game treats the player's hands as empty, 
			// resulting in the hilarious ability to punch with the bound bow "out". 
			// Bound arrows are also promptly unequipped at the same time.
			// 3. The bound weapon FX lingers until another weapon is equipped in the LH.
			//
			// Other bound weapons: 
			// Crash when unequipping/sheathing the bound weapon and equipping a spell 
			// in the same hand where the bound weapon was previously. 
			// Sometimes the bound weapon won't crash the game when unequipped from the RH
			// but it will always crash the game when unequipped from the LH.

			// Only equipped when the action starts.
			if (!HelperFuncs::ActionJustStarted(a_p, a_action))
			{
				return;
			}

			// If not triggered by the player through a casting action, return early.
			bool notACastAction = 
			{
				(
					a_action != InputAction::kCastLH && 
					a_action != InputAction::kCastRH && 
					a_action != InputAction::kQuickSlotCast
				) &&
				(
					(a_action != InputAction::kSpecialAction) || 
					(
						a_p->pam->reqSpecialAction != SpecialActionType::kCastBothHands && 
						a_p->pam->reqSpecialAction != SpecialActionType::kDualCast
					)
				)
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

			// Source from which the bound weapon equip request was made.
			// Left hand, right hand, or quick slot cast.
			enum Source { kLeft, kRight, kQuick };
			auto source = Source::kRight;
			// Set equip duration, get equip slot, and attempt to equip the bound weapon.
			auto setupBoundWeaponEquip = [a_p](RE::SpellItem* a_spell, Source&& a_source) 
			{
				if (!a_spell)
				{
					return;
				}

				auto assocWeap = Util::GetAssociatedBoundWeap(a_spell); 
				if (!assocWeap)
				{
					return;
				}
						
				// Player must have sufficient magicka.
				float cost = 
				(
					a_spell->CalculateMagickaCost(a_p->coopActor.get()) *
					Settings::vfMagickaCostMult[a_p->playerID]
				);
				if (cost > a_p->coopActor->GetActorValue(RE::ActorValue::kMagicka))
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kGeneralNotification,
						fmt::format("P{}: Not enough magicka!", a_p->playerID + 1),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kStealthState,
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);

					RE::PlaySound("MAGFailSD");
					// No need to attempt to equip.
					return;
				}

				// If the spell has base effects, 
				// set the bound weapon duration to the duration of first base effect.
				// Otherwise, default to 120 seconds.
				RE::BGSEquipSlot* slot = nullptr;
				bool hasEffect = !a_spell->effects.empty();
				if (assocWeap->equipSlot == glob.bothHandsEquipSlot)
				{
					if (hasEffect)
					{
						a_p->pam->secsBoundWeapon2HDuration = 
						(
							a_spell->effects[0]->GetDuration()
						);
					}
					else
					{
						a_p->pam->secsBoundWeapon2HDuration = 120.0f;
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
				auto caster = 
				(
					a_source == Source::kLeft ?
					a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand) :
					a_source == Source::kRight ? 
					a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand) :
					a_p->coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)
				);
				if (slot && caster)
				{
					HelperFuncs::EquipBoundWeapon(a_p, a_spell, assocWeap, slot, caster);
				}
			};

			// NOTE:
			// Must use the copied spell instead since something seems to be missing after copying
			// a bound spell to a placeholder spell and then casting the placeholder spell.
			// Game will crash once something else is equipped to the bound weapon's hand.
			if (a_action == InputAction::kQuickSlotCast) 
			{
				// Cast quick slot spells directly, not a placeholder spell. 
				setupBoundWeaponEquip(a_p->em->quickSlotSpell, Source::kQuick);
			}
			else if (a_action == InputAction::kCastLH || a_action == InputAction::kCastRH)
			{
				// Cast copied spell in LH/RH.
				auto isLeftHand = a_action == InputAction::kCastLH;
				auto copiedSpellForm = 
				(
					a_p->em->copiedMagic[isLeftHand ?
					!PlaceholderMagicIndex::kLH :
					!PlaceholderMagicIndex::kRH]
				);
				auto copiedSpell = copiedSpellForm->As<RE::SpellItem>();
				setupBoundWeaponEquip(copiedSpell, isLeftHand ? Source::kLeft : Source::kRight);
			}
			else
			{
				// Cast copied spells with both hands.
				auto copiedSpellLHForm = a_p->em->copiedMagic[!PlaceholderMagicIndex::kLH];
				auto copiedSpellRHForm = a_p->em->copiedMagic[!PlaceholderMagicIndex::kRH];
				auto copiedSpell = copiedSpellLHForm->As<RE::SpellItem>();
				setupBoundWeaponEquip(copiedSpell, Source::kLeft);
				copiedSpell = copiedSpellRHForm->As<RE::SpellItem>();
				setupBoundWeaponEquip(copiedSpell, Source::kRight);
			}
		}
		
		bool HandleDelayedHotkeyEquipRequest
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			const InputAction & a_occurringAction, 
			const PlayerActionManager::PlayerActionState& a_paState
		)
		{
			// Handle a delayed ('HotkeyEquip' bind released) hotkey equip request.
			// Return true if the current occurring action should skip executing its perf funcs,
			// since it is being used to equip the chosen hotkeyed item.

			// NOTE:
			// Four actions' composing inputs are used to signal which slot 
			// to equip the selected hotkeyed form into:
			// Left Trigger -> LH
			// Right Trigger -> RH
			// Left Bumper -> Item Quick Slot
			// Right Bumper -> Spell Quick Slot
			// 
			// If this occurring action is not a hotkey equip action,
			// and if the player is attempting to equip a hotkeyed form,
			// and the occurring PA includes one of the above composing inputs,
			// we can't perform this action.

			// Player has until the hotkey selection crosshair message is cleared
			// to choose which hand to equip the hotkeyed item into.
			bool choseHotkeyedItem = 
			{
				a_p->tm->lastCrosshairMessage->type == 
				CrosshairMessageType::kHotkeySelection &&
				Util::GetElapsedSeconds(a_p->tm->lastCrosshairMessage->setTP) < 
				a_p->tm->lastCrosshairMessage->secsMaxDisplayTime
			};
			// Do not execute PA funcs for this occurring action if the player 
			// has chosen a hotkeyed item and is now attempting
			// to equip that hotkeyed item to a hand or the quick slot.
			// Also do not run any funcs since we want the action to start and end normally 
			// to keep track of its pressed state,
			// but do not want the player to perform the action while the item is being equipped.
			bool isHotkeyEquipSlotSelectionBind = 
			(
				(
					a_occurringAction != InputAction::kHotkeyEquip
				) &&
				(
					(a_p->pam->IsPerforming(InputAction::kHotkeyEquip) || choseHotkeyedItem) &&
					(
						(a_paState.paParams.inputMask & (1 << !InputAction::kLT)) != 0 ||
						(a_paState.paParams.inputMask & (1 << !InputAction::kRT)) != 0 ||
						(
							a_paState.paParams.inputMask & (1 << !InputAction::kLShoulder)
						) != 0 ||
						(
							a_paState.paParams.inputMask & (1 << !InputAction::kRShoulder)
						) != 0
					)
				)
			);
			// Equip the chosen hotkeyed item or notify the player of success/failure
			// if this bind is used to select a slot to equip the hotkeyed form into,
			// and the player is not currently selecting a hotkeyed form.
			if (isHotkeyEquipSlotSelectionBind && 
				choseHotkeyedItem &&
				!a_p->pam->IsPerforming(InputAction::kHotkeyEquip) && 
				a_paState.perfStage == PerfStage::kInputsReleased)
			{
				// Default to right hand.
				EquipIndex equipIndex = EquipIndex::kRightHand;
				if ((a_paState.paParams.inputMask & (1 << !InputAction::kLT)) != 0)
				{
					equipIndex = EquipIndex::kLeftHand;
				}
				else if ((a_paState.paParams.inputMask & (1 << !InputAction::kLShoulder)) != 0)
				{
					equipIndex = EquipIndex::kQuickSlotItem;
				}
				else if ((a_paState.paParams.inputMask & (1 << !InputAction::kRShoulder)) != 0)
				{
					equipIndex = EquipIndex::kQuickSlotSpell;
				}

				// If the chosen hotkeyed form message is still displayed 
				// after the 'HotkeyEquip' bind is released, 
				// then on release of this bind, equip the chosen hotkeyed form.
				HelperFuncs::EquipHotkeyedForm
				(
					a_p, a_p->em->lastChosenHotkeyedForm, std::move(equipIndex)
				);
			}

			return isHotkeyEquipSlotSelectionBind;
		}

		bool InitiateSpecialInteractionPackage
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			RE::TESObjectREFR* a_interactionRefr, 
			RE::TESBoundObject* a_interactionBaseObj
		)
		{
			// If the targeted interaction refr is furniture, 
			// set up the special interaction package and entry position.
			// Return true if successful.
			
			if (!a_interactionRefr || !a_interactionBaseObj)
			{
				return false;
			}

			// Get furniture entry point to serve as interaction package entry position.
			bool isFurniture = 
			(
				a_interactionRefr->As<RE::TESFurniture>() || 
				a_interactionBaseObj->As<RE::TESFurniture>()
			);
			bool isIdleMarker = 
			(
				a_interactionRefr->As<RE::BGSIdleMarker>() || 
				a_interactionBaseObj->As<RE::BGSIdleMarker>()
			);
			if (!isFurniture && !isIdleMarker)
			{
				return false;
			}

			// Set initial interaction entry position to the object's position.
			// Will pick closest entry position to set below.
			a_p->mm->interactionPackageEntryPos = a_interactionRefr->data.location;
			auto asFurniture = 
			(
				a_interactionBaseObj->As<RE::TESFurniture>()
			); 
			if (asFurniture)
			{
				// If there is no marker and no workbench type,
				// there is no interaction animation to trigger,
				// so we don't need to run the interaction package
				// and can skip to just activating the object.
				bool noInteractionAnimation = 
				(
					asFurniture->furnFlags ==
					(
						RE::TESFurniture::ActiveMarker::kNone
					) &&
					asFurniture->workBenchData.benchType ==
					(
						RE::TESFurniture::WorkBenchData::BenchType::kNone
					)	
				);
				if (noInteractionAnimation)
				{
					return false;
				}

				auto refr3DPtr = Util::GetRefr3D(a_interactionRefr);
				if (refr3DPtr)
				{
					RE::BSFurnitureMarkerNode* markerNode = 
					(
						refr3DPtr->GetExtraData<RE::BSFurnitureMarkerNode>("FRN")
					);
					if (markerNode)
					{
						std::optional<RE::NiPoint3> closestEntryPoint = std::nullopt;
						for (uint32_t i = 0; i < markerNode->markers.size(); ++i)
						{
							// NOTE: 
							// I know the offset math is not quite correct,
							// so this is an approximation for now.
							const auto& marker = markerNode->markers[i];
							float localPitch = 0.0f;
							float localYaw = 0.0f;
							if (fabsf(marker.offset.z) > 1E-5f)
							{
								localPitch = Util::DirectionToGameAngPitch(marker.offset);
							}

							if (fabsf(marker.offset.x) > 1E-5f || fabsf(marker.offset.y) > 1E-5f)
							{
								localYaw = Util::DirectionToGameAngYaw(marker.offset);
							}

							float worldPitch = -a_interactionRefr->data.angle.x - localPitch;
							float worldYaw = Util::ConvertAngle
							(
								Util::NormalizeAng0To2Pi
								(
									localYaw + a_interactionRefr->data.angle.z
								)
							);
							auto entryPointOffset = 
							(
								Util::RotationToDirectionVect(worldPitch, worldYaw) *
								marker.offset.Length()
							);
							auto entryPoint = refr3DPtr->world.translate + entryPointOffset;
							// Set new closest entry point if there was no entry point 
							// previously set or this one is closer.
							if (!closestEntryPoint.has_value() ||
								entryPoint.GetDistance(a_p->coopActor->data.location) < 
								closestEntryPoint.value().GetDistance
								(
									a_p->coopActor->data.location
								))
							{
								closestEntryPoint = entryPoint;
							}
						}

						// Set to refr3D's world position if there's no closest entry point.
						if (!closestEntryPoint.has_value())
						{
							closestEntryPoint = refr3DPtr->world.translate;
						}

						// Set interaction entry position.
						a_p->mm->interactionPackageEntryPos = closestEntryPoint.value();
					}
				}
			}

			// Sheathe weapon and prep interaction package, 
			// and then evaluate it to initiate furniture use.
			a_p->pam->ReadyWeapon(false);
			a_p->coopActor->extraList.SetLinkedRef(a_interactionRefr, a_p->aimTargetKeyword);
			a_p->tm->aimTargetLinkedRefrHandle = a_p->tm->activationRefrHandle;
			a_p->pam->SetAndEveluatePackage
			(
				a_p->pam->GetCoopPackage(PackageIndex::kSpecialInteraction), false
			);
			// Turn to face when first activated.
			a_p->coopActor->SetHeading
			(
				Util::GetYawBetweenPositions
				(
					a_p->mm->interactionPackageEntryPos, a_p->coopActor->data.location
				)
			);

			// Successfully initiated the interaction.
			return true;
		}

		uint32_t LootAllItemsFromContainer
		(
			const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFRPtr a_containerPtr
		)
		{
			// Move all lootable items from the given container to the requesting player.
			// Return the number of looted objects.

			// Only loot if the container is available, not locked, 
			// and has a base type of TESContainer.
			if (!a_containerPtr || 
				a_containerPtr->IsLocked() || 
				!a_containerPtr->HasContainer()) 
			{
				return 0;
			}

			uint32_t lootedObjects = 0;
			auto p1 = RE::PlayerCharacter::GetSingleton();
			// Check inventory.
			auto inventory = 
			(
				a_containerPtr ? 
				a_containerPtr->GetInventory() : 
				RE::TESObjectREFR::InventoryItemMap()
			);
			if (inventory.empty())
			{
				return lootedObjects;
			}

			for (auto& [boundObj, countInvEntryDataPair] : inventory)
			{
				if (!boundObj || 
					countInvEntryDataPair.first <= 0 || 
					!Util::IsLootableObject(*boundObj))
				{
					continue;
				}
				
				// Give to P1 if the object is a party-wide or quest item.
				bool toP1 = 
				(
					(
						a_p->isPlayer1 || 
						Util::IsPartyWideItem(boundObj)
					) || 
					(
						countInvEntryDataPair.second &&
						countInvEntryDataPair.second->IsQuestObject()
					)
				);
				if (toP1)
				{
					a_containerPtr->RemoveItem
					(
						boundObj,
						countInvEntryDataPair.first,
						RE::ITEM_REMOVE_REASON::kStoreInContainer,
						nullptr,
						p1
					);
				}
				else
				{
					a_containerPtr->RemoveItem
					(
						boundObj,
						countInvEntryDataPair.first,
						RE::ITEM_REMOVE_REASON::kRemove,
						nullptr,
						nullptr
					);
					a_p->coopActor->AddObjectToContainer
					(
						boundObj,
						nullptr,
						countInvEntryDataPair.first,
						a_p->coopActor.get()
					);
				}

				lootedObjects += countInvEntryDataPair.first;
				// Show in TrueHUD recent loot widget by adding and removing the object from P1.
				if (p1 && ALYSLC::TrueHUDCompat::g_trueHUDInstalled && !toP1)
				{
					p1->AddObjectToContainer
					(
						boundObj, nullptr, countInvEntryDataPair.first, p1
					);
					p1->RemoveItem
					(
						boundObj,
						countInvEntryDataPair.first, 
						RE::ITEM_REMOVE_REASON::kRemove,
						nullptr, 
						nullptr
					);
				}
			}

			return lootedObjects;
		}

		uint32_t LootAllItemsFromCorpse
		(
			const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFRPtr a_corpseRefrPtr
		)
		{
			// Move all lootable items from the corpse to the requesting player.
			// Return the number of looted objects.

			// Only loot if the corpse is available.
			if (!a_corpseRefrPtr ||
				!a_corpseRefrPtr->As<RE::Actor>() || 
				!a_corpseRefrPtr->As<RE::Actor>()->IsDead())
			{
				return 0;
			}
			
			uint32_t lootedObjects = 0;
			auto asActor = a_corpseRefrPtr->As<RE::Actor>();
			auto p1 = RE::PlayerCharacter::GetSingleton();
			// Check inventory first.
			auto inventory = asActor->GetInventory();
			if (!inventory.empty())
			{
				for (auto& [boundObj, countInvEntryDataPair] : inventory)
				{
					if (!boundObj || 
						countInvEntryDataPair.first <= 0 || 
						!Util::IsLootableObject(*boundObj))
					{
						continue;
					}

					// Give to P1 if the object is a party-wide or quest item.	
					bool toP1 = 
					(
						(
							a_p->isPlayer1 || 
							Util::IsPartyWideItem(boundObj)
						) || 
						(
							countInvEntryDataPair.second &&
							countInvEntryDataPair.second->IsQuestObject()
						)
					);
					if (toP1)
					{
						asActor->RemoveItem
						(
							boundObj,
							countInvEntryDataPair.first,
							RE::ITEM_REMOVE_REASON::kStoreInContainer,
							nullptr,
							p1
						);
					}
					else
					{
						asActor->RemoveItem
						(
							boundObj,
							countInvEntryDataPair.first,
							RE::ITEM_REMOVE_REASON::kRemove,
							nullptr,
							nullptr
						);
						a_p->coopActor->AddObjectToContainer
						(
							boundObj,
							nullptr,
							countInvEntryDataPair.first,
							a_p->coopActor.get()
						);
					}

					lootedObjects += countInvEntryDataPair.first;
					// Show in TrueHUD recent loot widget 
					// by adding and removing the object from P1.
					if (p1 && ALYSLC::TrueHUDCompat::g_trueHUDInstalled && !toP1)
					{
						p1->AddObjectToContainer
						(
							boundObj, nullptr, countInvEntryDataPair.first, p1
						);
						p1->RemoveItem
						(
							boundObj,
							countInvEntryDataPair.first, 
							RE::ITEM_REMOVE_REASON::kRemove,
							nullptr, 
							nullptr
						);
					}
				}
			}

			// Check dropped inventory next.
			auto droppedInventory = asActor->GetDroppedInventory();
			if (!droppedInventory.empty())
			{
				for (const auto& [boundObj, countHandlePair] : droppedInventory)
				{
					if (!boundObj ||
						countHandlePair.first <= 0 || 
						!Util::IsLootableObject(*boundObj))
					{
						continue;
					}

					for (const auto& refrHandle : countHandlePair.second)
					{
						auto refrPtr = Util::GetRefrPtrFromHandle(refrHandle); 
						if (!refrPtr)
						{
							continue;
						}
							
						// Loot loose refr(s).
						lootedObjects += LootRefr(a_p, refrPtr);
					}
				}
			}

			return lootedObjects;
		}

		uint32_t LootRefr(const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFRPtr a_refrPtr)
		{
			// Loot a single refr or aggregation of the same refr.
			// Return the number of looted objects.

			// Must be valid.
			if (!a_refrPtr)
			{
				return 0;
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
			bool shouldPickup = 
			(
				(baseObj) && (baseObj->IsBook() || baseObj->IsNote())
			);

			if (a_p->isPlayer1)
			{
				// Activate or pickup right away with P1.
				if (shouldPickup) 
				{
					a_p->coopActor->PickUpObject(a_refrPtr.get(), count);
				}
				else
				{
					Util::ActivateRefr
					(
						a_refrPtr.get(),
						a_p->coopActor.get(), 
						0, 
						baseObj, 
						count, 
						false
					);
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
						a_refrPtr->extraList.HasType(RE::ExtraDataType::kFromAlias)
					};
					// Move shared items to P1 for ease of access,
					// especially if they are quest items.
					shouldLootWithP1 = isQuestItem || Util::IsPartyWideItem(baseObj);
					// Show in TrueHUD recent loot widget if P1 is not looting the item themselves.
					if (p1 && ALYSLC::TrueHUDCompat::g_trueHUDInstalled && !shouldLootWithP1)
					{
						p1->AddObjectToContainer(baseObj, nullptr, count, p1);
						p1->RemoveItem
						(
							baseObj, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
						);
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
						Util::ActivateRefr
						(
							a_refrPtr.get(), p1, 0, a_refrPtr.get()->GetBaseObject(), count, false
						);
					}
				}
				else
				{
					// Have the player themselves loot the item
					// by picking it up or through activation.
					if (shouldPickup)
					{
						a_p->coopActor->PickUpObject(a_refrPtr.get(), count);
					}
					else
					{
						Util::ActivateRefr
						(
							a_refrPtr.get(), 
							a_p->coopActor.get(),
							0,
							baseObj, 
							count, 
							false
						);
					}
				}
			}

			return count;
		}

		bool MovingBackward(const std::shared_ptr<CoopPlayer>& a_p, float a_facingToMovingAngDiff)
		{
			// Is moving backward relative to the player's facing direction 
			// (90 degree window behind the player's facing direction).

			return 
			(
				(
					a_facingToMovingAngDiff <= (-3.0f / 4.0f * PI) && 
					a_facingToMovingAngDiff >= (-5.0f / 4.0f * PI)
				) ||
				(
					a_facingToMovingAngDiff >= (3.0f / 4.0f * PI) && 
					a_facingToMovingAngDiff <= (5.0f / 4.0f * PI)
				)
			);
		}

		bool MovingForward(const std::shared_ptr<CoopPlayer>& a_p, float a_facingToMovingAngDiff)
		{
			// Is moving forward relative to the player's facing direction 
			// (90 degree window in front of the player's facing direction).

			return 
			(
				(
					a_facingToMovingAngDiff >= (-1.0f / 4.0f * PI) &&
					a_facingToMovingAngDiff <= (1.0f / 4.0f * PI)
				) ||
				(
					a_facingToMovingAngDiff >= (7.0f / 4.0f * PI) || 
					a_facingToMovingAngDiff <= (-7.0f / 4.0f * PI)
				)
			);
		}

		bool MovingLeft(const std::shared_ptr<CoopPlayer>& a_p, float a_facingToMovingAngDiff)
		{
			// Is moving left relative to the player's facing direction 
			// (90 degree window to the left of the player's facing direction).

			return 
			(
				(
					a_facingToMovingAngDiff >= (-3.0f / 4.0f * PI) &&
					a_facingToMovingAngDiff <= (-1.0f / 4.0f * PI)
				) ||
				(
					a_facingToMovingAngDiff >= (5.0f / 4.0f * PI) && 
					a_facingToMovingAngDiff <= (7.0f / 4.0f * PI)
				)
			);
		}
		bool MovingRight(const std::shared_ptr<CoopPlayer>& a_p, float a_facingToMovingAngDiff)
		{
			// Is moving right relative to the player's facing direction 
			// (90 degree window to the right of the player's facing direction).

			return 
			(
				(
					a_facingToMovingAngDiff >= (1.0f / 4.0f * PI) && 
					a_facingToMovingAngDiff <= (3.0f / 4.0f * PI)
				) ||
				(
					a_facingToMovingAngDiff <= (-5.0f / 4.0f * PI) &&
					a_facingToMovingAngDiff >= (-7.0f / 4.0f * PI)
				)
			);
		}

		void OpenMenuWithKeyboard
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		)
		{
			// Send an emulated keyboard input to open the menu associated with the given action.

			// Not an action that will open a menu, so return.
			if (glob.paInfoHolder->DEF_ACTION_INDICES_TO_GROUPS.at(!a_action) != 
				ActionGroup::kMenu)
			{
				return;
			}

			// Attempt to obtain menu control before opening menu.
			bool succ = false;
			if (a_action == InputAction::kMagicMenu)
			{
				succ = glob.moarm->InsertRequest
				(
					a_p->controllerID, a_action, SteadyClock::now(), RE::MagicMenu::MENU_NAME
				);
			}
			else if (a_action == InputAction::kMapMenu)
			{
				succ = glob.moarm->InsertRequest
				(
					a_p->controllerID, a_action, SteadyClock::now(), RE::MapMenu::MENU_NAME
				);
			}
			else if (a_action == InputAction::kPause)
			{
				succ = glob.moarm->InsertRequest
				(
					a_p->controllerID, a_action, SteadyClock::now(), RE::JournalMenu::MENU_NAME
				);
			}
			else if (a_action == InputAction::kStatsMenu)
			{
				succ = glob.moarm->InsertRequest
				(
					a_p->controllerID, a_action, SteadyClock::now(), RE::StatsMenu::MENU_NAME
				);
			}
			else if (a_action == InputAction::kTweenMenu)
			{
				succ = glob.moarm->InsertRequest
				(
					a_p->controllerID, a_action, SteadyClock::now(), RE::TweenMenu::MENU_NAME
				);
			}
			else if (a_action == InputAction::kWaitMenu)
			{
				succ = glob.moarm->InsertRequest
				(
					a_p->controllerID, a_action, SteadyClock::now(), RE::SleepWaitMenu::MENU_NAME
				);
			}

			// If the request was successfully inserted, open the requested menu.
			if (succ) 
			{
				a_p->pam->SendButtonEvent
				(
					a_action, 
					RE::INPUT_DEVICE::kKeyboard,
					ButtonEventPressType::kInstantTrigger, 
					0.0f,
					true
				);
			}
		}

		void PerformActivationCycling(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Check for downed players to revive before cycling through nearby refrs, 
			// highlighting one at a time if they are within activation distance.

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

			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (!pam->downedPlayerTarget) 
			{
				const float& secsSinceActivationStarted = 
				(
					pam->paStatesList
					[!InputAction::kActivate - !InputAction::kFirstAction].secsPerformed
				);
				auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
				// Ignore invalid refrs and normally hostile actors selected with the crosshair.
				// Enemies are too angry to interact with players, from my experience.
				bool crosshairRefrValidity = 	
				(
					(crosshairRefrPtr) && 
					(Util::IsValidRefrForTargeting(crosshairRefrPtr.get()))
				);
				auto asActor = crosshairRefrValidity ? crosshairRefrPtr->As<RE::Actor>() : nullptr;
				if (asActor)
				{
					crosshairRefrValidity &= 	
					(
						(asActor->IsDead()) ||
						(asActor->IsAMount()) ||
						(
							p1 && 
							!asActor->IsHostileToActor(p1) &&
							!asActor->IsHostileToActor(a_p->coopActor.get())
						) ||
						(Util::IsGuard(asActor)) || 
						(Util::HasNoBountyButInCrimeFaction(asActor)) ||
						(Util::IsPartyFriendlyActor(asActor)) ||
						(a_p->coopActor->IsSneaking())
					);
				}

				// Choose a valid crosshair refr for activation 
				// if the action press time is within the initial window before cycling.
				// Otherwise, look for a nearby refr to activate.
				a_p->tm->useProximityInteraction =
				(
					secsSinceActivationStarted >= Settings::fSecsBeforeActivationCycling ||
					!crosshairRefrValidity
				);

				if (justStarted)
				{
					a_p->lastActivationCheckTP = SteadyClock::now();
					// Remove activation effect shader on previous activation refr, if any.
					auto activationRefrPtr = Util::GetRefrPtrFromHandle
					(
						a_p->tm->activationRefrHandle
					); 
					if (activationRefrPtr &&
						Util::IsValidRefrForTargeting(activationRefrPtr.get()))
					{
						Util::StopEffectShader
						(
							activationRefrPtr.get(), glob.activateHighlightShader
						);
					}

					// Get new refr to activate.
					auto refrToActivatePtr = 
					(
						Util::GetRefrPtrFromHandle(a_p->tm->UpdateNextObjectToActivate())
					);
					if (refrToActivatePtr && 
						Util::IsValidRefrForTargeting(refrToActivatePtr.get()))
					{
						// Play highlight shader on crosshair refr.
						Util::StartEffectShader
						(
							refrToActivatePtr.get(),
							glob.activateHighlightShader,
							max(0.1f, Settings::fSecsBeforeActivationCycling)
						);
					}

					// Not activation cycling yet.
					a_p->pam->startedActivationCycling = false;
				}
				
				// Cycle through nearby objects at regular intervals 
				// while the activate bind is held.
				// NOTE:
				// No activation is performed here.
				// We are just caching the cycled refr and highlighting it.
				if (a_p->tm->useProximityInteraction)
				{
					// Seconds since cycling the next activation refr from nearby references.
					float secsSinceLastActivationCyclingCheck = Util::GetElapsedSeconds
					(
						a_p->lastActivationCheckTP
					);
					bool shouldStartCycling =
					(
						!a_p->pam->startedActivationCycling && 
						secsSinceActivationStarted >= Settings::fSecsBeforeActivationCycling
					);
					bool shouldCycleNewRefr = 
					(
						a_p->pam->startedActivationCycling &&
						secsSinceLastActivationCyclingCheck >=
						Settings::fSecsBetweenActivationChecks
					);
					if (shouldStartCycling || shouldCycleNewRefr)
					{
						a_p->lastActivationCheckTP = SteadyClock::now();
						// Remove activation effect shader on previous activation refr, if any.
						auto refrToActivatePtr = Util::GetRefrPtrFromHandle
						(
							a_p->tm->activationRefrHandle
						); 
						if (refrToActivatePtr &&
							Util::IsValidRefrForTargeting(refrToActivatePtr.get()))
						{
							Util::StopEffectShader
							(
								refrToActivatePtr.get(), glob.activateHighlightShader
							);
						}

						// Flag as started activation cycling.
						if (shouldStartCycling)
						{
							a_p->pam->startedActivationCycling = true;
						}

						// Get new refr to activate.
						refrToActivatePtr = 
						(
							Util::GetRefrPtrFromHandle(a_p->tm->UpdateNextObjectToActivate())
						);
						if (refrToActivatePtr && 
							Util::IsValidRefrForTargeting(refrToActivatePtr.get()))
						{
							// Play highlight shader on cycled activation refr.
							Util::StartEffectShader
							(
								refrToActivatePtr.get(),
								glob.activateHighlightShader,
								max(0.1f, Settings::fSecsBetweenActivationChecks)
							);
						}
					}
				}

				// Check if the crosshair-targeted/cycled refr is a downed player now.
				// Only a valid action if the revive system is enabled.
				// Also cannot initiate revive if ragdolling or staggered, so exit here.
				if (!Settings::bUseReviveSystem || 
					a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal)
				{
					return;
				}

				auto targetRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle);
				auto targetRefrValidity = 
				(
					targetRefrPtr && Util::IsValidRefrForTargeting(targetRefrPtr.get())
				);
				// No valid target or too far away to activate.
				// NOTE: 
				// Range check only done here on the initial check.
				if (!targetRefrValidity || 
					!a_p->tm->RefrIsInActivationRange(a_p->tm->activationRefrHandle))
				{
					return;
				}

				// Find downed, unrevived player.
				auto downedPlayerIter = std::find_if
				(
					glob.coopPlayers.begin(), glob.coopPlayers.end(),
					[&targetRefrPtr](const auto& p) 
					{
						return 
						(
							p->isActive && 
							p->coopActor == targetRefrPtr && 
							p->isDowned && 
							!p->isRevived
						);
					}
				);
				
				// No found player, nothing else to do.
				if (downedPlayerIter == glob.coopPlayers.end())
				{
					return;
				}

				// Is another player.
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
				// Start reviving now.
				pam->downedPlayerTarget = (*downedPlayerIter);
				pam->downedPlayerTarget->isBeingRevived = true;
				a_p->isRevivingPlayer = true;
				a_p->lastReviveCheckTP = SteadyClock::now();

				// Only will reach here if there is a revivable player target.
				// Play revive animation if grounded.
				if (!a_p->coopActor->IsOnMount() &&
					!a_p->coopActor->IsSwimming() && 
					!a_p->coopActor->IsFlying())
				{
					a_p->coopActor->NotifyAnimationGraph("IdleForceDefaultState");
					a_p->coopActor->NotifyAnimationGraph("IdleKneeling");
				}

				// Play shaders and hit effects.
				const auto& downedPlayerTarget = (*downedPlayerIter);
				Util::StartEffectShader(a_p->coopActor.get(), glob.dragonHolesShader, -1.0f);
				Util::StartEffectShader
				(
					downedPlayerTarget->coopActor.get(), glob.dragonSoulAbsorbShader, -1.0f
				);
				Util::StartHitArt
				(
					downedPlayerTarget->coopActor.get(), 
					glob.reviveDragonSoulEffect, 
					downedPlayerTarget->coopActor.get(),
					-1.0f, 
					false,
					true
				);
				Util::StartHitArt
				(
					downedPlayerTarget->coopActor.get(), 
					glob.reviveHealingEffect,
					downedPlayerTarget->coopActor.get(),
					-1.0f,
					false,
					false
				);
			}
			else if (!pam->downedPlayerTarget->isRevived) 
			{
				// Continue reviving the cached downed player if they are not fully revived.
				// Rotate to face downed player's torso.
				float yawToTarget = Util::GetYawBetweenPositions
				(
					a_p->coopActor->data.location,
					pam->downedPlayerTarget->mm->playerTorsoPosition
				);
				float angDiff = Util::NormalizeAngToPi
				(
					yawToTarget - a_p->coopActor->data.angle.z
				);
				a_p->coopActor->SetHeading
				(
					Util::NormalizeAng0To2Pi(a_p->coopActor->data.angle.z + angDiff)
				);

				// Will set the is revived flag to true 
				// once the downed player is fully revived.
				pam->RevivePlayer();
			}
		}

		void PlayEmoteIdle(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Play idle animation corresponding to current cycled emote if not already requested. 
			// Otherwise, stop the idle.

			if (!a_p->pam->isPlayingEmoteIdle && 
				!a_p->em->currentCycledIdleIndexPair.first.empty())
			{
				// Cycled idle available.
				a_p->coopActor->NotifyAnimationGraph(a_p->em->currentCycledIdleIndexPair.first);
				a_p->pam->isPlayingEmoteIdle = true;
			}
			else if (!a_p->coopActor->IsOnMount() && 
					 !a_p->coopActor->IsSwimming() && 
					 !a_p->coopActor->IsFlying())
			{
				// Emote on your own time. I'm not paying you to emote!
				a_p->coopActor->NotifyAnimationGraph("IdleStop");
				a_p->coopActor->NotifyAnimationGraph("IdleForceDefaultState");
				a_p->pam->isPlayingEmoteIdle = false;
			}
		}

		bool PlayKillmoveFromList
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			RE::Actor* a_targetActor,
			const bool& a_isOtherPlayer,
			const bool& a_hasCharacterSkeleton, 
			const std::vector<RE::TESIdleForm*>& a_killmoveIdlesList
		)
		{
			// Attempt to trigger each killmove in the given list.
			// Return true if a killmove was triggered successfully.

			bool performedKillmove = false;
			if (!a_killmoveIdlesList.empty())
			{
				float yawToTarget = Util::GetYawBetweenPositions
				(
					a_p->coopActor->data.location, a_targetActor->data.location
				);
				float absFacingAngDiff = fabsf
				(
					Util::NormalizeAngToPi(a_targetActor->GetHeading(false) - yawToTarget)
				);

				// Have the player face the target first.
				a_p->coopActor->SetHeading(yawToTarget);
				// Stop both actors from moving.
				Util::NativeFunctions::SetDontMove(a_targetActor, true);
				a_targetActor->StopMoving(*g_deltaTimeRealTime);
				a_targetActor->NotifyAnimationGraph("moveStop");
				Util::SetLinearVelocity(a_targetActor, RE::NiPoint3());
				Util::NativeFunctions::SetDontMove(a_targetActor, true);
				a_p->coopActor->StopMoving(*g_deltaTimeRealTime);
				a_p->coopActor->NotifyAnimationGraph("moveStop");
				Util::SetLinearVelocity(a_p->coopActor.get(), RE::NiPoint3());
				Util::NativeFunctions::SetDontMove(a_p->coopActor.get(), true);

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
						// Don't allow decapitation of other players 
						// if the revive system is in use.
						// Because, well, once revived, something will still be missing up top.
						isBackstab = glob.BACKSTAB_KILLMOVES_HASHES_SET.contains
						(
							Hash(killmoveIdle->formEditorID)
						);
						isDecap = glob.DECAP_KILLMOVES_HASHES_SET.contains
						(
							Hash(killmoveIdle->formEditorID)
						);
						facingCorrectDir = 
						(
							(isBackstab && absFacingAngDiff < PI / 2.0f) || 
							(!isBackstab && absFacingAngDiff >= PI / 2.0f)
						);
						canPerform = 
						(
							(facingCorrectDir) && 
							(!isDecap || !a_isOtherPlayer || !Settings::bUseReviveSystem)
						);
					}
					else
					{
						canPerform = false;
					}

					// Can attempt to trigger the killmove.
					if (canPerform)
					{
						// Choose front position killmoves 
						// if the target is not facing away from the player.
						// Choose back position killmoves otherwise.
						if (absFacingAngDiff >= PI / 2.0f)
						{
							// Face the player.
							a_targetActor->SetHeading(Util::NormalizeAngTo2Pi(yawToTarget + PI));
						}
						else
						{
							// Face 180 degrees away from the player.
							a_targetActor->SetHeading(yawToTarget);
						}

						performedKillmove = 
						(
							Util::PlayIdle(killmoveIdle, a_p->coopActor.get(), a_targetActor) &&
							a_p->coopActor->IsInKillMove()
						);
						// Update killmove index and num performed for the next iteration.
						killmoveIndex = 
						(
							killmoveIndex + 1 == a_killmoveIdlesList.size() ? 0 : killmoveIndex + 1
						);
						++killmovesAttempted;

						// Have to run delayed killmove checks, as some killmoves, 
						// such as 'pa_KillMoveDLC02RipHeartOut' do not start right away.
						// Update delayed killmove request info 
						// for this player's player action manager to handle later.
						a_p->pam->killmoveTargetActorHandle = a_targetActor->GetHandle();
						a_p->lastKillmoveCheckTP = SteadyClock::now();
					}
					else
					{
						// Move on.
						killmoveIndex = 
						(
							killmoveIndex + 1 == a_killmoveIdlesList.size() ? 0 : killmoveIndex + 1
						);
						++killmovesAttempted;
					}
				}

				// Generic killmoves fail often when targeting actors 
				// with a skeleton other than the character skeleton.
				// The killmove target may remain alive at 0 HP 
				// and gain temporary invulnerability for a bit 
				// until the kill request is carried out.
				// Force kill and ragdoll here.
				// The player actor will also, more often than not, 
				// warp around during the killmove animation.
				if (Settings::bUseGenericKillmovesOnUnsupportedNPCs && 
					!a_hasCharacterSkeleton && 
					performedKillmove && 
					a_targetActor && 
					!a_targetActor->IsInKillMove() && 
					a_targetActor->currentProcess &&
					a_targetActor->currentProcess->middleHigh)
				{
					a_targetActor->currentProcess->deathTime = 1.0f;
					a_targetActor->currentProcess->middleHigh->inDeferredKill = false;
					a_targetActor->KillImpl(a_p->coopActor.get(), FLT_MAX, true, true);
					a_targetActor->KillImmediate();
					a_targetActor->NotifyAnimationGraph("Ragdoll");
				}
			}
			
			// Allow both actors to move again post-killmove check.
			Util::NativeFunctions::SetDontMove(a_targetActor, false);
			Util::NativeFunctions::SetDontMove(a_p->coopActor.get(), false);
			return performedKillmove;
		}

		void PlayPowerAttackAnimation
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		)
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

			// NOTE: 
			// Have to start attacking with the LH/RH first
			// to trigger directional LH power attacks/some RH power attacks.
			// Also, LH/RH action command-triggered werewolf power attacks
			// do not work for non-P1 players.
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
					// Not selected running the dual power attack action command, 
					// so perform via idle.
					if (a_p->em->IsUnarmed())
					{
						Util::PlayIdle("RightH2HComboPowerAttack", a_p->coopActor.get());
					}
					else
					{
						Util::RunPlayerActionCommand
						(
							RE::DEFAULT_OBJECT::kActionDualPowerAttack, a_p->coopActor.get()
						);
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
					if (a_p->em->LHEmpty() && a_p->em->GetRHWeapon())
					{
						// Prevents blocking when there is a weapon in the RH.
						Util::PlayIdle("H2HLeftHandPowerAttack", a_p->coopActor.get());
					}
					else
					{
						// Have to trigger a normal LH attack first 
						// so that the power attack command executes successfully.
						Util::RunPlayerActionCommand
						(
							RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
						);
						Util::RunPlayerActionCommand
						(
							RE::DEFAULT_OBJECT::kActionLeftPowerAttack, a_p->coopActor.get()
						);
					}
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
					// The default unarmed right power attack fails to trigger 
					// via action command at times if using MCO.
					// Play the corresponding MCO idle instead.
					if (ALYSLC::MCOCompat::g_mcoInstalled) 
					{
						if (a_p->em->RHEmpty()) 
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
						// Have to trigger a normal RH attack before the H2H power attack idle;
						// otherwise, the power attack idle will fail to play.
						Util::RunPlayerActionCommand
						(
							RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get()
						);
						if (a_p->em->RHEmpty())
						{
							Util::PlayIdle("H2HRightHandPowerAttack", a_p->coopActor.get());
						}
						else
						{
							Util::RunPlayerActionCommand
							(
								RE::DEFAULT_OBJECT::kActionRightPowerAttack, a_p->coopActor.get()
							);
						}
					}
				}
			}
		}

		void PrepForTargetLocationSpellCast
		(
			const std::shared_ptr<CoopPlayer>& a_p, RE::SpellItem* a_spell)
		{
			// Look down at the ground or at the targeted world position
			// to choose the destination point before casting 
			// to maximize chances of a successful location-based cast, for P1 especially,
			// since the game will check the casting direction (determined by the player's X angle) 
			// and will not perform the cast if the player is looking at 
			// a non-traversable location, such as the sky.
			// Pitch angle will reset in the movement manager on the next iteration.

			if (!a_spell || a_spell->GetDelivery() != RE::MagicSystem::Delivery::kTargetLocation)
			{
				return;
			}
			
			a_p->coopActor->data.angle.x = 4.0f * PI / 9.0f;
		}

		bool RequestToUseParaglider(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Check if this player is requesting to paraglide.
			// Must have Skyrim's Paraglider installed:
			// https://www.nexusmods.com/skyrimspecialedition/mods/53256
			// Wish I could provide companion player compatibility 
			// for the slick paraglide animations. Sadge.
			// Return true if the request was successful.

			if (!ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled)
			{
				return false;
			}

			auto charController = a_p->coopActor->GetCharController(); 
			if (!charController)
			{
				return false;
			}

			bool justStarted = HelperFuncs::ActionJustStarted(a_p, InputAction::kActivate);
			if (justStarted)
			{
				// Reset request flag when the action starts.
				a_p->pam->requestedToParaglide = false;
			}

			// P1 must have the paraglider.
			if (!ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider)
			{
				return false;
			}

			bool isAirborne = 
			(
				charController->context.currentState == RE::hkpCharacterStateType::kInAir
			);
			// Indicate that this player would like to paraglide if P1 has one 
			// and the player is in the air.
			if (justStarted && isAirborne)
			{
				a_p->pam->requestedToParaglide = true;
				// Toggle magical paraglide state for companion players.
				if (!a_p->isPlayer1)
				{
					a_p->mm->shouldParaglide = !a_p->mm->isParagliding;
				}
			}

			return a_p->pam->requestedToParaglide;
		}

		void SetCameraAdjustmentMode
		(
			const int32_t& a_reqCID, const InputAction& a_action, bool&& a_set
		)
		{
			// Set or reset the co-op camera's adjustment mode (Rotate, Zoom, or None).

			const auto newAdjMode = 
			(
				a_action == InputAction::kRotateCam ? 
				CamAdjustmentMode::kRotate : 
				CamAdjustmentMode::kZoom
			);
			auto& controllingCID = glob.cam->controlCamCID;
			// Can only set if no other mode is set, 
			// meaning no other player is controlling the cam.
			if (controllingCID != a_reqCID && glob.cam->camAdjMode == CamAdjustmentMode::kNone)
			{
				controllingCID = a_reqCID;
			}

			// Controller with control over the camera can adjust the cam mode freely.
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

			const auto newCamState = 
			(
				a_action == InputAction::kCamLockOn ? 
				CamState::kLockOn : 
				CamState::kManualPositioning
			);
			auto& controllingCID = glob.cam->controlCamCID;
			// Set new cam control CID if no players are adjusting the camera.
			if (controllingCID != a_reqCID && glob.cam->camAdjMode == CamAdjustmentMode::kNone)
			{
				controllingCID = a_reqCID;
			}

			// Controller with control over the camera can adjust the cam state freely.
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
					p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kCamera,
						fmt::format
						(
							"P{}: Camera auto-trail mode", 
							glob.coopPlayers[a_reqCID]->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else if (glob.cam->camState == CamState::kLockOn)
				{
					p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kCamera,
						fmt::format
						(
							"P{}: Camera lock-on mode", 
							glob.coopPlayers[a_reqCID]->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kStealthState,
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kCamera,
						fmt::format
						(
							"P{}: Camera manual positioning mode",
							glob.coopPlayers[a_reqCID]->playerID + 1
						),
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

		void SetUpCastingPackage
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			bool&& a_lhCast,
			bool&& a_rhCast,
			bool a_actionJustSterted
		)
		{
			// Prepare companion player ranged package for casting with the given hand(s).

			const auto& em = a_p->em;
			const auto& pam = a_p->pam;
			if (a_p->coopActor->IsWeaponDrawn())
			{
				// Do not spellcast if canceled previously with the sheathe bind.
				// Reset if a hand casting bind has just started.
				if (a_actionJustSterted) 
				{
					a_p->pam->spellcastingCancelled = false;
				}
				else if (a_p->pam->spellcastingCancelled)
				{
					return;
				}

				auto lhSpell = em->GetLHSpell();
				auto rhSpell = em->GetRHSpell();
				auto& lhCasting = pam->castingGlobVars[!CastingGlobIndex::kLH];
				auto& rhCasting = pam->castingGlobVars[!CastingGlobIndex::kRH];
				auto& casting2H = pam->castingGlobVars[!CastingGlobIndex::k2H];
				auto& dualCasting = pam->castingGlobVars[!CastingGlobIndex::kDual];
				auto& shouting = pam->castingGlobVars[!CastingGlobIndex::kShout];
				auto& voiceCasting = pam->castingGlobVars[!CastingGlobIndex::kVoice];
				bool is2HSpellCast = 
				(
					a_lhCast && 
					a_rhCast && 
					lhSpell && 
					rhSpell && 
					rhSpell->equipSlot == glob.bothHandsEquipSlot
				);
				// Potentially notify the player that they have insufficient magicka 
				// to cast the spell when the bind is first pressed.
				// Skip the cast if there is not enough magicka.
				if (a_actionJustSterted && !a_p->isInGodMode)
				{
					float cost = 0.0f;
					if (is2HSpellCast)
					{
						cost = 
						(
							rhSpell ? rhSpell->CalculateMagickaCost(a_p->coopActor.get()) : 0.0f
						);
						if (rhSpell &&
							rhSpell->GetCastingType() == 
							RE::MagicSystem::CastingType::kConcentration)
						{
							cost *= *g_deltaTimeRealTime;
						}
					}
					else
					{
						if (a_lhCast)
						{
							lhCasting->value = 0.0f;
							float lhCost = 
							(
								lhSpell ?
								lhSpell->CalculateMagickaCost(a_p->coopActor.get()) :
								0.0f
							);
							if (lhSpell &&
								lhSpell->GetCastingType() == 
								RE::MagicSystem::CastingType::kConcentration)
							{
								lhCost *= *g_deltaTimeRealTime;
							}

							cost += lhCost;
						}

						if (a_rhCast)
						{
							rhCasting->value = 0.0f;
							float rhCost = 
							(
								rhSpell ? 
								rhSpell->CalculateMagickaCost(a_p->coopActor.get()) : 
								0.0f
							);
							if (rhSpell &&
								rhSpell->GetCastingType() == 
								RE::MagicSystem::CastingType::kConcentration)
							{
								rhCost *= *g_deltaTimeRealTime;
							}

							cost += rhCost;
						}
					}
					
					casting2H->value = 0.0f;
					cost *= Settings::vfMagickaCostMult[a_p->playerID];
					if (cost > a_p->pam->currentMagicka)
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kGeneralNotification,
							fmt::format("P{}: Not enough magicka!", a_p->playerID + 1),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kStealthState,
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs
						);

						RE::PlaySound("MAGFailSD");
						// No need to attempt to cast.
						return;
					}
				}

				auto rangedAttackPackage = pam->GetCoopPackage(PackageIndex::kRangedAttack);
				pam->SetCurrentPackage(rangedAttackPackage);
				bool isVoiceSpellCast = 
				(
					!a_lhCast && !a_rhCast && em->voiceForm && em->voiceForm->As<RE::SpellItem>()
				);
				bool isShoutCast = 
				(
					!a_lhCast && !a_rhCast && em->voiceForm && em->voiceForm->As<RE::TESShout>()
				);
				auto lhCaster = a_p->coopActor->GetMagicCaster
				(
					RE::MagicSystem::CastingSource::kLeftHand
				);
				auto rhCaster = a_p->coopActor->GetMagicCaster
				(
					RE::MagicSystem::CastingSource::kRightHand
				);
				
				// Typically, if a caster is stuck at state 1 for multiple frames,
				// the casting package is either not being executed or has stalled,
				// preventing the cast from occurring.
				// Request to cast again if this happens.
				// Unfortunately, from what I can tell, this unresponsiveness 
				// is tied to how often the package evaluates and is unavoidable.
				bool restartPackage = 
				(
					(a_lhCast && *lhCaster->state == RE::MagicCaster::State::kUnk01) || 
					(a_rhCast && *rhCaster->state == RE::MagicCaster::State::kUnk01)
				);
				if (restartPackage)
				{
					/*SPDLOG_DEBUG
					(
						"[PAFH] SetUpCastingPackage: {}: Restart package.",
						a_p->coopActor->GetName()
					);*/
					if (a_lhCast && *lhCaster->state == RE::MagicCaster::State::kUnk01)
					{
						lhCaster->RequestCastImpl();
					}
					if (a_rhCast && *rhCaster->state == RE::MagicCaster::State::kUnk01)
					{
						rhCaster->RequestCastImpl();
					}
				}
				
				// Extra flags for debug purposes.
				// 
				// Set casting global variables to match the requested hand/voice slots.
				bool globVarNotSetYet = 
				{ 
					(is2HSpellCast && casting2H->value != 1.0f) ||
					(isShoutCast && shouting->value != 1.0f) ||
					(isVoiceSpellCast && voiceCasting->value != 1.0f) ||
					(
						(!is2HSpellCast) && 
						(
							(a_lhCast && lhCasting->value != 1.0f) || 
							(a_rhCast && rhCasting->value != 1.0f)
						)
					) 
				};
				// At least one hand caster is inactive or interrupted.
				bool notHandCastingYet = 
				(
					(
						(a_lhCast) && 
						(
							(lhCaster->state == RE::MagicCaster::State::kNone) ||
							(lhCaster->state == RE::MagicCaster::State::kUnk07) ||
							(lhCaster->state == RE::MagicCaster::State::kUnk08) ||
							(lhCaster->state == RE::MagicCaster::State::kUnk09)
						)
					) || 
					(
						(a_rhCast) && 
						(
							(rhCaster->state == RE::MagicCaster::State::kNone) ||
							(rhCaster->state == RE::MagicCaster::State::kUnk07) ||
							(rhCaster->state == RE::MagicCaster::State::kUnk08) ||
							(rhCaster->state == RE::MagicCaster::State::kUnk09)
						)
					)
				);

				// Since the targeting manager's update call runs 
				// after the player action manager's main task,
				// any selected aim correction target would get set 
				// after we already start casting the spell, unless it is set here first.
				a_p->tm->UpdateAimCorrectionTarget();
				// Set target linked reference before setting and evaluating package.
				//
				// Special case (idk why this happens): 
				// If trying to cast a LH spell with a staff equipped in the RH,
				// setting the aim target linked refr to any actor but the player themselves
				// causes the casting package to fail to cast the spell.
				// So we have to set the aim target linked refr to the player themselves initially 
				// to begin the cast.
				bool aimTargetRefrChanged = false;
				if (a_lhCast && a_rhCast)
				{
					a_p->tm->UpdateAimTargetLinkedRefr
					(
						EquipIndex::kLeftHand,
						!a_actionJustSterted || !a_lhCast || !a_p->em->HasRHStaffEquipped()
					);
					a_p->tm->UpdateAimTargetLinkedRefr
					(
						EquipIndex::kRightHand,
						!a_actionJustSterted || !a_lhCast || !a_p->em->HasRHStaffEquipped()
					);
				}
				else 
				{
					a_p->tm->UpdateAimTargetLinkedRefr
					(
						a_lhCast ? EquipIndex::kLeftHand : EquipIndex::kRightHand,
						!a_actionJustSterted || !a_lhCast || !a_p->em->HasRHStaffEquipped()
					);
				}
				float dualValue = dualCasting->value;
				float dualPrevValue = dualCasting->value;
				float casting2HValue = casting2H->value;
				float casting2HPrevValue = casting2H->value;
				float lhValue = lhCasting->value;
				float lhPrevValue = lhCasting->value;
				float rhValue = rhCasting->value;
				float rhPrevValue = rhCasting->value;
				float shoutingValue = shouting->value;
				float shoutingPrevValue = shouting->value;
				float voiceValue = voiceCasting->value;
				float voicePrevValue = voiceCasting->value;
				if (is2HSpellCast) 
				{
					lhValue = 
					rhValue = 0.0f;
					casting2HValue = 1.0f;
				}
				else if (isShoutCast)
				{
					shoutingValue = 1.0f;
					voiceValue = 0.0f;
				}
				else if (isVoiceSpellCast)
				{
					shoutingValue = 0.0f;
					voiceValue = 1.0f;
				}
				else
				{
					if (a_lhCast)
					{
						lhValue = 1.0f;
					}

					if (a_rhCast)
					{
						rhValue = 1.0f;
					}

					voiceValue = 0.0f;
						
					// TODO: Add dual casting support
					/*if (a_lhCast && a_rhCast)
					{
						dualValue = 1.0f;
					}*/
				}

				// Avoid evaluating the package when the caster has just started the cast,
				// or is charging the cast. 
				// Re-evaluating here can restart the cast or stall the caster.
				bool casterNotPreppedYet = 
				(
					(
						(a_lhCast) && 
						(
							(lhCaster->state == RE::MagicCaster::State::kUnk01) ||
							(lhCaster->state == RE::MagicCaster::State::kUnk02)
						)
					) || 
					(
						(a_rhCast) && 
						(
							(rhCaster->state == RE::MagicCaster::State::kUnk01) ||
							(rhCaster->state == RE::MagicCaster::State::kUnk02)
						)
					)
				);
				// Evaluate when a casting global variable has changed.
				bool valueChanged = 
				(
					lhValue != lhPrevValue ||
					rhValue != rhPrevValue ||
					casting2HValue != casting2HPrevValue ||
					shoutingValue != shoutingPrevValue ||
					voiceValue != voicePrevValue ||
					dualValue != dualPrevValue
				);
				// Set casting global variables that are used in the ranged attack package.
				// Also evaluate package if the aim target linked reference changed mid-execution.
				if (!casterNotPreppedYet || valueChanged)
				{
					/*SPDLOG_DEBUG
					(
						"[PAFH] SetUpCastingPackage: {}: "
						"glob var not set yet: {}, "
						"not hand casting yet: {}, "
						"aim target refr changed: {}, "
						"action just started: {}, "
						"caster prepped: {}, value changed: {}. "
						"LH/RH caster states: {}, {}.",
						a_p->coopActor->GetName(),
						globVarNotSetYet,
						notHandCastingYet,
						aimTargetRefrChanged,
						a_actionJustSterted,
						!casterNotPreppedYet,
						valueChanged,
						lhCaster ? *lhCaster->state : RE::MagicCaster::State::kNone,
						rhCaster ? *rhCaster->state : RE::MagicCaster::State::kNone
					);*/

					if (is2HSpellCast) 
					{
						lhCasting->value = 
						rhCasting->value = 0.0f;
						casting2H->value = 1.0f;
					}
					else if (isShoutCast)
					{
						shouting->value = 1.0f;
						voiceCasting->value = 0.0f;
					}
					else if (isVoiceSpellCast)
					{
						shouting->value = 0.0f;
						voiceCasting->value = 1.0f;
					}
					else
					{
						if (a_lhCast)
						{
							lhCasting->value = 1.0f;
						}

						if (a_rhCast)
						{
							rhCasting->value = 1.0f;
						}

						casting2H->value = 0.0f;
						
						// TODO: Add dual casting support
						/*if (a_lhCast && a_rhCast)
						{
							dualCasting->value = 1.0f;
						}*/
					}
					
					// Evaluate ranged attack package with updated globals.
					pam->SetAndEveluatePackage(rangedAttackPackage);
				}
			}
			else
			{
				// Ready weapon/spell/fists if not out already.
				pam->ReadyWeapon(true);
			}
		}
		
		void UpdatePlayerMagicCasterTarget
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			RE::MagicSystem::CastingSource && a_casterSource
		)
		{
			// Update the target for the player's given magic caster.

			auto caster = a_p->coopActor->GetMagicCaster(a_casterSource);
			if (!caster || !caster->currentSpell)
			{
				return;
			}

			auto delivery = caster->currentSpell->GetDelivery();
			bool requiresTarget = 
			(
				delivery == RE::MagicSystem::Delivery::kAimed ||
				delivery == RE::MagicSystem::Delivery::kTargetActor ||
				delivery == RE::MagicSystem::Delivery::kTouch
			);
			// Only set the desired target for spells that require targets.
			// Can glitch out self-cast spells, for example, when setting the desired target
			// to something before casting.
			if (!requiresTarget)
			{
				return;
			}

			auto rangedTargetActorHandle = a_p->tm->GetRangedTargetActor();
			auto rangedTargetActorPtr = Util::GetActorPtrFromHandle
			(
				rangedTargetActorHandle
			);
			if (rangedTargetActorPtr)
			{
				caster->desiredTarget = rangedTargetActorHandle;
			}
			else if (Util::HandleIsValid(a_p->tm->crosshairRefrHandle))
			{
				caster->desiredTarget = a_p->tm->crosshairRefrHandle;
			}
		}

		void UseWeaponOnHold(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action)
		{
			// Use the equipped weapon in the hand corresponding to the requested action.

			const auto& pam = a_p->pam;
			const auto& em = a_p->em;
			// Draw weapon/mag/fists if not out already.
			if (ActionJustStarted(a_p, a_action) && !a_p->coopActor->IsWeaponDrawn())
			{
				pam->ReadyWeapon(true);
				return;
			}
			
			bool isLeftHand = a_action == InputAction::kAttackLH;
			if (a_p->isPlayer1) 
			{
				if (em->Has2HRangedWeapEquipped() && !isLeftHand)
				{
					// Need to toggle AI driven to draw bow.
					// Playing the right attack action on P1 does not update the draw timer, 
					// so all projectiles fire at the lowest release speed.
					if (ActionJustStarted(a_p, a_action))
					{
						auto currentAmmo = a_p->coopActor->GetCurrentAmmo();
						auto invCounts = a_p->coopActor->GetInventoryCounts();
						const auto iter = invCounts.find(currentAmmo);
						const int32_t ammoCount = iter != invCounts.end() ? iter->second : -1;
						if (!currentAmmo || ammoCount <= 0)
						{
							// Notify the player that they do not have ammo equipped.
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kGeneralNotification,
								fmt::format("P{}: No equipped ammo!", a_p->playerID + 1),
								{
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
							);
						}
						else
						{
							// Notify the player of their ammo count
							// before shooting the projectile.
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kGeneralNotification,
								fmt::format
								(
									"P{}: Ammo counter: {}", a_p->playerID + 1, max(0, ammoCount)
								),
								{ 
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
							);
						}
						
						// Trigger via action command.
						Util::RunPlayerActionCommand
						(
							RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get()
						);
					}
					else
					{
						// Don't need to toggle AI driven after drawing the bow/crossbow.
						pam->QueueP1ButtonEvent
						(
							a_action, 
							RE::INPUT_DEVICE::kGamepad, 
							ButtonEventPressType::kPressAndHold, 
							0.0f,
							false
						);
					}
				}
				else if ((isLeftHand && em->HasLHStaffEquipped()) ||
						 (!isLeftHand && em->HasRHStaffEquipped()))
				{
					// Cast with staff.
					auto staff = isLeftHand ? em->GetLHWeapon() : em->GetRHWeapon();
					a_p->pam->CastStaffSpell(staff, isLeftHand, true);
				}
			}
			else
			{
				// Start drawing bow/crossbow.
				if (ActionJustStarted(a_p, a_action) && 
					!isLeftHand && 
					em->Has2HRangedWeapEquipped())
				{
					auto currentAmmo = a_p->coopActor->GetCurrentAmmo();
					auto invCounts = a_p->coopActor->GetInventoryCounts();
					const auto iter = invCounts.find(currentAmmo);
					const int32_t ammoCount = iter != invCounts.end() ? iter->second : -1;
					if (!currentAmmo || ammoCount <= 0)
					{
						// Notify the player that they do not have ammo equipped.
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kGeneralNotification,
							fmt::format("P{}: No equipped ammo!", a_p->playerID + 1),
							{ 
								CrosshairMessageType::kNone,
								CrosshairMessageType::kStealthState,
								CrosshairMessageType::kTargetSelection 
							},
							0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
						);
					}
					else
					{
						// Notify the player of their ammo count before shooting the projectile.
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kGeneralNotification,
							fmt::format
							(
								"P{}: Ammo counter: {}", a_p->playerID + 1, max(0, ammoCount)
							),
							{ 
								CrosshairMessageType::kNone,  
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
						);
					}

					// Trigger via action command.
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get()
					);
				}
				else
				{
					// Cast with staff.
					bool lhStaffCastReq = 
					(
						isLeftHand && em->HasLHStaffEquipped() && pam->usingLHStaff->value != 1.0f
					);
					bool rhStaffCastReq = 
					(
						!isLeftHand && em->HasRHStaffEquipped() && pam->usingRHStaff->value != 1.0f
					);
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
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kReviveAlert,
						fmt::format
						(
							"P{}: <font color=\"#1E88E5\">Reviving {}</font>", 
							a_p->playerID + 1, 
							a_p->pam->downedPlayerTarget->coopActor->GetName()
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
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kReviveAlert,
						fmt::format
						(
							"P{}: <font color=\"#FF0000\">"
							"Not enough health to revive another player!</font>", 
							a_p->playerID + 1
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
				a_p->tm->canActivateRefr = false;
				const auto activationRefrPtr = Util::GetRefrPtrFromHandle
				(
					a_p->tm->activationRefrHandle
				);
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
					bool anotherPlayerControllingMenus = 
					(
						!GlobalCoopData::CanControlMenus(a_p->controllerID)
					);
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
					bool offLimits = activationRefrPtr->IsCrimeToActivate();
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
					bool isInRange = a_p->tm->RefrIsInActivationRange
					(
						a_p->tm->activationRefrHandle
					);
					// Is a lootable refr.
					bool isLootable = Util::IsLootableRefr(activationRefrPtr.get());
					// Player is sneaking.
					bool isSneaking = a_p->coopActor->IsSneaking();
					// Something to do with usability.
					bool isPlayable = activationRefrPtr->GetPlayable();
					// Player has LOS on the refr.
					bool passesLOSCheck =
					(
						activationRefrPtr &&
						Util::HasLOS
						(
							activationRefrPtr.get(), 
							a_p->coopActor.get(), 
							false, 
							a_p->tm->crosshairRefrHandle == a_p->tm->activationRefrHandle, 
							a_p->tm->crosshairWorldPos
						)
					);
					
					// Crosshair message to display.
					RE::BSFixedString activationMessage = ""sv;
					RE::BSFixedString activationString = ""sv;
					bool hasActivationText = false;
					if (!isSneaking && offLimits)
					{
						// Must sneak to interact with off-limits refrs.
						activationString = Util::GetActivationText
						(
							baseObj, activationRefrPtr.get(), hasActivationText
						);
						if (hasActivationText)
						{
							// Special Case:
							// Will pick up notes and books if they are activated 
							// while not selected by the crosshair.
							bool isBook = baseObj->IsBook();
							bool isNote = baseObj->IsNote();
							bool wouldPickupBookNote = 
							(
								(isBook || isNote) &&
								(
									a_p->tm->activationRefrHandle !=
									a_p->tm->crosshairRefrHandle
								)
							);
							if (wouldPickupBookNote)
							{
								if (offLimits)
								{
									activationMessage = fmt::format
									(
										"P{}: Sneak to <font color=\"#FF0000\">Steal</font> {}", 
										a_p->playerID + 1,
										activationRefrPtr->GetName()
									);
								}
								else
								{
									activationMessage = fmt::format
									(
										"P{}: Sneak to Take {}", 
										a_p->playerID + 1,
										activationRefrPtr->GetName()
									);
								}
							}
							else
							{
								activationMessage = fmt::format
								(
									"P{}: Sneak to {}", a_p->playerID + 1, activationString
								);
							}
						}
						else
						{
							activationMessage = fmt::format
							(
								"P{}: Sneak to "
								"<font color=\"#FF0000\">interact</font> with {}",
								a_p->playerID + 1, 
								activationRefrPtr->GetName()
							);
						}
					}
					else if (!isPlayable || activationBlocked)
					{
						// Blocked from activating.
						activationMessage = fmt::format
						(
							"P{}: {} cannot be activated", 
							a_p->playerID + 1,
							activationRefrPtr->GetName()
						);
					}
					else if (isLocked && !hasLockpicks && !canUnlockWithKey)
					{
						// No lockpicks or key.
						activationMessage = fmt::format
						(
							"P{}: Out of lockpicks", a_p->playerID + 1
						);
					}
					else if (otherPlayerDowned && tryingToUseTeleportRefr)
					{
						// Can't leave the current cell with a player downed.
						activationMessage = fmt::format
						(
							"P{}: Cannot leave downed teammates behind", a_p->playerID + 1
						);
					}
					else if (!menusOnlyAlwaysOpen && anotherPlayerControllingMenus && !isLootable)
					{
						// Another player is controlling menus and the target refr is not lootable.
						activationMessage = fmt::format
						(
							"P{}: Another player is controlling menus", a_p->playerID + 1
						);
					}
					else if (!passesLOSCheck)
					{
						// Player has no LOS.
						activationMessage = fmt::format
						(
							"P{}: {} is not accessible from this position",
							a_p->playerID + 1, activationRefrPtr->GetName()
						);
					}
					else
					{
						// Is another player.
						if (GlobalCoopData::IsCoopPlayer(activationRefrPtr.get()))
						{
							activationMessage = fmt::format
							(
								"P{}: Player {}",
								a_p->playerID + 1,
								activationRefrPtr->GetDisplayFullName()
							);
						}
						else
						{
							if (isInRange)
							{
								auto p1 = RE::PlayerCharacter::GetSingleton();
								auto asActor = activationRefrPtr->As<RE::Actor>();
								// Selected a hostile actor with the crosshair.
								bool crosshairTargetedHostileActor = 
								(
									(
										asActor && 
										asActor->GetHandle() == a_p->tm->crosshairRefrHandle
									) &&
									(
										asActor->IsHostileToActor(a_p->coopActor.get()) ||
										asActor->IsHostileToActor(p1)
									)
								);
								// Living guard with a bounty out on the player.
								bool showSurrenderMessage = 
								(
									crosshairTargetedHostileActor &&
									!asActor->IsDead() &&
									Util::IsGuard(asActor) &&
									Util::HasBountyOnPlayer(asActor) &&
									!a_p->coopActor->IsSneaking()
								);
								// Living, normally passive actor with no bounty on the player,
								// or fleeing the player.
								bool showStopCombatMessage = 
								(
									(
										!showSurrenderMessage &&
										crosshairTargetedHostileActor &&
										!asActor->IsDead() &&
										!a_p->coopActor->IsSneaking()
									) &&
									(
										Util::HasNoBountyButInCrimeFaction(asActor) || 
										Util::IsFleeing(asActor) ||
										asActor->IsAMount() ||
										Util::IsPartyFriendlyActor(asActor)
									)
								);
								if (showSurrenderMessage)
								{
									activationMessage = fmt::format
									(
										"P{}: Surrender to {}",
										a_p->playerID + 1,
										activationRefrPtr->GetName()
									);
								}
								else if (showStopCombatMessage)
								{
									activationMessage = fmt::format
									(
										"P{}: Stop combat with {}",
										a_p->playerID + 1,
										activationRefrPtr->GetName()
									);
								}
								else
								{
									// Player can activate this refr.
									// Set activation text to the refr's name 
									// if no text is available.
									activationString = Util::GetActivationText
									(
										baseObj, activationRefrPtr.get(), hasActivationText
									);
									if (hasActivationText)
									{
										// Special Case:
										// Pick up notes and books if they are activated 
										// while not selected by the crosshair.
										bool isBook = baseObj->IsBook();
										bool isNote = baseObj->IsNote();
										bool shouldPickupBookNote = 
										(
											(isBook || isNote) &&
											(
												a_p->tm->activationRefrHandle !=
												a_p->tm->crosshairRefrHandle
											)
										);
										if (shouldPickupBookNote)
										{
											if (offLimits)
											{
												activationMessage = fmt::format
												(
													"P{}: <font color=\"#FF0000\">Steal</font> {}", 
													a_p->playerID + 1,
													activationRefrPtr->GetName()
												);
											}
											else
											{
												activationMessage = fmt::format
												(
													"P{}: Take {}", 
													a_p->playerID + 1,
													activationRefrPtr->GetName()
												);
											}
										}
										else
										{
											activationMessage = fmt::format
											(
												"P{}: {}", a_p->playerID + 1, activationString
											);
										}
									}
									else
									{
										activationMessage = fmt::format
										(
											"P{}: Interact with {}",
											a_p->playerID + 1,
											activationRefrPtr->GetName()
										);
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
											a_p->coopActor->GetWeightInContainer()
										);
										const auto invChanges = 
										(
											a_p->coopActor->GetInventoryChanges()
										);
										if (invChanges)
										{
											inventoryWeight = invChanges->totalWeight;
										}

										const float carryweight = 
										(
											a_p->coopActor->GetTotalCarryWeight()
										);
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

								a_p->tm->canActivateRefr = true;
							}
							else
							{
								// Not in range.
								activationMessage = fmt::format
								(
									"P{}: {} is too far away",
									a_p->playerID + 1,
									activationRefrPtr->GetName()
								);
							}
						}
					}

					// Set crosshair message.
					a_p->tm->SetCrosshairMessageRequest
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
	};

	namespace ProgressFuncs
	{
		void Activate(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Perform activation cycling/validation if not using the paraglider.

			bool wantsToUseParaglider = HelperFuncs::RequestToUseParaglider(a_p); 
			if (wantsToUseParaglider)
			{
				return;
			}

			HelperFuncs::PerformActivationCycling(a_p);
			HelperFuncs::ValidateActivationRefr(a_p);
		}

		void AttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Use weapon if it is drawn.

			if (!a_p->coopActor->IsWeaponDrawn())
			{
				return;
			}

			HelperFuncs::UseWeaponOnHold(a_p, InputAction::kAttackLH);
		}

		void AttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Use weapon if it is drawn.

			if (!a_p->coopActor->IsWeaponDrawn())
			{
				return;
			}

			HelperFuncs::UseWeaponOnHold(a_p, InputAction::kAttackRH);
		}

		void Bash(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Power bash when the bash bind is held long enough,
			// the player has the power bash perk, 
			// and the player is not already bashing or performing a killmove.
			// Regular bash is performed on release if a power bash was not performed on hold.

			bool canPowerBash = 
			(
				a_p->pam->GetSecondsSinceLastInputStateChange(InputAction::kBash, true) > 
				Settings::fSecsDefMinHoldTime && 
				a_p->coopActor->HasPerk(glob.powerBashPerk) &&
				!a_p->pam->isAttacking &&
				a_p->em->HasShieldEquipped()
			);
			if (!canPowerBash) 
			{
				return;
			}

			bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kBash);
			if (performingKillmove)
			{
				return;
			}

			// Continue blocking if the player was blocking before the bash; stop otherwise.
			bool wasBlocking = a_p->pam->IsPerforming(InputAction::kBlock);
			// Action command for both player types.
			Util::RunPlayerActionCommand
			(
				RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
			);
			Util::PlayIdle("PowerBash", a_p->coopActor.get());
			// Redundancy since block start/stop requests fail at times.
			if (wasBlocking)
			{
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
				);
				a_p->coopActor->NotifyAnimationGraph("blockStart");
			}
			else
			{
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get()
				);
				a_p->coopActor->NotifyAnimationGraph("attackStop");
				a_p->coopActor->NotifyAnimationGraph("blockStop");
			}
		}

		void CastLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cast spell in the LH.
			
			if (a_p->isPlayer1) 
			{
				// Draw weapon if not out already.
				// Send button event.
				if (HelperFuncs::ActionJustStarted(a_p, InputAction::kCastLH) && 
					!a_p->coopActor->IsWeaponDrawn())
				{
					a_p->pam->ReadyWeapon(true);
				}
				
				// Set target actor/refr directly before sending each cast button event.
				// Allows for more consistent targeting with 'Aimed' spells
				// that appear to instantly apply their effect 
				// instead of firing a guidable projectile at the selected target.
				// Ex. the 'Heal Other' spell.
				HelperFuncs::UpdatePlayerMagicCasterTarget
				(
					a_p, RE::MagicSystem::CastingSource::kLeftHand
				);
				a_p->pam->QueueP1ButtonEvent
				(
					InputAction::kCastLH, 
					RE::INPUT_DEVICE::kGamepad,
					ButtonEventPressType::kPressAndHold, 
					0.0f, 
					false
				);
			}
			else
			{
				// Setup LH casting variables, 
				// add ranged package to the top of the player's package stack, 
				// and evaluate to start LH casting.
				// Has associated bound weapon.
				auto lhSpell = a_p->em->GetLHSpell();
				if (auto assocWeap = Util::GetAssociatedBoundWeap(lhSpell); assocWeap)
				{
					HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kCastLH);
				}
				else
				{
					bool justStarted = HelperFuncs::ActionJustStarted(a_p, InputAction::kCastLH);
					bool is2HSpell = lhSpell && lhSpell->equipSlot == glob.bothHandsEquipSlot;
					if (is2HSpell)
					{
						HelperFuncs::SetUpCastingPackage
						(
							a_p, 
							true, 
							true, 
							justStarted
						);
					}
					else
					{
						HelperFuncs::SetUpCastingPackage
						(
							a_p, 
							true,
							false,
							justStarted
						);
					}

					// Cast with P1 if necessary.
					if (bool shouldCastWithP1 = Util::ShouldCastWithP1(lhSpell); shouldCastWithP1) 
					{
						a_p->pam->CastSpellWithMagicCaster
						(
							EquipIndex::kLeftHand,
							justStarted,
							true,
							true,
							shouldCastWithP1
						);
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
				if (HelperFuncs::ActionJustStarted(a_p, InputAction::kCastRH) &&
					!a_p->coopActor->IsWeaponDrawn())
				{
					a_p->pam->ReadyWeapon(true);
				}
				
				// Set target actor/refr directly before sending each cast button event.
				// Allows for more consistent targeting with 'Aimed' spells
				// that appear to instantly apply their effect 
				// instead of firing a guidable projectile at the selected target.
				// Ex. the 'Heal Other' spell.
				HelperFuncs::UpdatePlayerMagicCasterTarget
				(
					a_p, RE::MagicSystem::CastingSource::kRightHand
				);
				a_p->pam->QueueP1ButtonEvent
				(
					InputAction::kCastRH,
					RE::INPUT_DEVICE::kGamepad, 
					ButtonEventPressType::kPressAndHold,
					0.0f, 
					false
				);
			}
			else
			{
				// Setup RH casting variables, 
				// add ranged package to the top of the player's package stack, 
				// and evaluate to start RH casting.
				// Has associated bound weapon.
				auto rhSpell = a_p->em->GetRHSpell();
				if (auto assocWeap = Util::GetAssociatedBoundWeap(rhSpell); assocWeap)
				{
					HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kCastRH);
				}
				else
				{
					bool justStarted = HelperFuncs::ActionJustStarted(a_p, InputAction::kCastRH);
					bool is2HSpell = rhSpell && rhSpell->equipSlot == glob.bothHandsEquipSlot;
					if (is2HSpell)
					{
						HelperFuncs::SetUpCastingPackage
						(
							a_p, 
							true,
							true,
							justStarted
						);
					}
					else
					{
						HelperFuncs::SetUpCastingPackage
						(
							a_p, 
							false,
							true,
							justStarted
						);
					}

					// Cast with P1 if necessary.
					if (bool shouldCastWithP1 = Util::ShouldCastWithP1(rhSpell); shouldCastWithP1)
					{
						a_p->pam->CastSpellWithMagicCaster
						(
							EquipIndex::kRightHand,
							justStarted,
							true,
							true, 
							shouldCastWithP1
						);
					}
				}
			}
		}

		void CycleAmmo(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player ammo on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleAmmo))
			{
				return;
			}

			// Set last cycled form and then cycle ammo.
			a_p->em->lastCycledForm = a_p->em->currentCycledAmmo;
			a_p->em->CycleAmmo();
			if (RE::TESForm* ammo = a_p->em->currentCycledAmmo; ammo)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, ammo->GetName()),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: No favorited ammo", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		void CycleSpellCategoryLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player LH spell category on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleSpellCategoryLH))
			{
				return;
			}

			// Set last cycled spell category and then cycle.
			a_p->em->lastCycledSpellCategory = a_p->em->lhSpellCategory;
			a_p->em->CycleHandSlotMagicCategory(false);
			const std::string_view newCategory = a_p->em->FavMagCyclingCategoryToString
			(
				a_p->em->lhSpellCategory
			);
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kEquippedItem,
				fmt::format("P{}: Left hand spell category: '{}'", a_p->playerID + 1, newCategory),
				{ 
					CrosshairMessageType::kNone,
					CrosshairMessageType::kStealthState,
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		void CycleSpellCategoryRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player RH spell category on first press and on hold at a certain interval.
			// If ammo cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleSpellCategoryRH))
			{
				return;
			}

			// Set last cycled spell category and then cycle.
			a_p->em->lastCycledSpellCategory = a_p->em->rhSpellCategory;
			a_p->em->CycleHandSlotMagicCategory(true);
			const std::string_view newCategory = a_p->em->FavMagCyclingCategoryToString
			(
				a_p->em->rhSpellCategory
			);
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kEquippedItem,
				fmt::format
				(
					"P{}: Right hand spell category: '{}'", a_p->playerID + 1, newCategory
				),
				{ 
					CrosshairMessageType::kNone,
					CrosshairMessageType::kStealthState, 
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		void CycleSpellLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player LH spell on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleSpellLH))
			{
				return;
			}

			// Set last cycled spell and then cycle.
			a_p->em->lastCycledForm = 
			(
				a_p->em->currentCycledLHSpellsList[!a_p->em->lhSpellCategory]
			);
			// Update cycled spell from the current spell category.
			a_p->em->CycleHandSlotMagic(false);
			auto spellForm = a_p->em->currentCycledLHSpellsList[!a_p->em->lhSpellCategory]; 
			if (spellForm)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, spellForm->GetName()),
					{
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: '{}' category is empty", 
						a_p->playerID + 1,
						a_p->em->FavMagCyclingCategoryToString(a_p->em->lhSpellCategory)
					),
					{
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		void CycleSpellRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player RH spell on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleSpellRH))
			{
				return;
			}

			// Set last cycled spell and then cycle.
			a_p->em->lastCycledForm = 
			(
				a_p->em->currentCycledRHSpellsList[!a_p->em->rhSpellCategory]
			);
			// Update cycled spell from the current spell category.
			a_p->em->CycleHandSlotMagic(true);
			auto spellForm = a_p->em->currentCycledRHSpellsList[!a_p->em->rhSpellCategory]; 
			if (spellForm)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, spellForm->GetName()),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: '{}' category is empty", 
						a_p->playerID + 1, 
						a_p->em->FavMagCyclingCategoryToString(a_p->em->rhSpellCategory)
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		void CycleVoiceSlotMagic(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player voice spell/shout on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleVoiceSlotMagic))
			{
				return;
			}

			// Set last cycled spell and then cycle.
			a_p->em->lastCycledForm = a_p->em->currentCycledVoiceMagic;
			// Update cycled spell from the current category.
			a_p->em->CycleVoiceSlotMagic();
			if (RE::TESForm* voiceForm = a_p->em->currentCycledVoiceMagic; voiceForm)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, voiceForm->GetName()),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: No favorited powers/shouts", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		void CycleWeaponCategoryLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player LH weapon category on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleWeaponCategoryLH))
			{
				return;
			}

			// Set last cycled weapon category and then cycle.
			a_p->em->lastCycledWeaponCategory = a_p->em->lhWeaponCategory;
			a_p->em->CycleWeaponCategory(false);
			const std::string_view newCategory = a_p->em->FavWeaponCyclingCategoryToString
			(
				a_p->em->lhWeaponCategory
			);
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kEquippedItem,
				fmt::format
				(
					"P{}: Left hand weapon category: '{}'", a_p->playerID + 1, newCategory
				),
				{ 
					CrosshairMessageType::kNone, 
					CrosshairMessageType::kStealthState,
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		void CycleWeaponCategoryRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player RH weapon category on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleWeaponCategoryRH)) 
			{
				return;
			}

			// Set last cycled weapon category and then cycle.
			a_p->em->lastCycledWeaponCategory = a_p->em->rhWeaponCategory;
			a_p->em->CycleWeaponCategory(true);
			const std::string_view newCategory = a_p->em->FavWeaponCyclingCategoryToString
			(
				a_p->em->rhWeaponCategory
			);
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kEquippedItem,
				fmt::format
				(
					"P{}: Right hand weapon category: '{}'", a_p->playerID + 1, newCategory
				),
				{ 
					CrosshairMessageType::kNone, 
					CrosshairMessageType::kStealthState, 
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		void CycleWeaponLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player LH weapon on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleWeaponLH)) 
			{
				return;
			}

			// Set last cycled form and then cycle.
			a_p->em->lastCycledForm = 
			(
				a_p->em->currentCycledLHWeaponsList[!a_p->em->lhWeaponCategory]
			);
			// Update cycled weapon from the current weapon category.
			a_p->em->CycleWeapons(false);
			auto form = a_p->em->currentCycledLHWeaponsList[!a_p->em->lhWeaponCategory]; 
			if (form)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, form->GetName()),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (a_p->em->lhWeaponCategory == FavWeaponCyclingCategory::kAllFavorites && 
					 a_p->em->HasCyclableWeaponInCategory
					 (
						 FavWeaponCyclingCategory::kAllFavorites, false
					 ))
			{
				// Will empty both hands if no weapon is returned 
				// while cycling through all favorites.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip 'Fists'", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: '{}' category is empty", 
						a_p->playerID + 1, 
						a_p->em->FavWeaponCyclingCategoryToString(a_p->em->lhWeaponCategory)
					),
					{
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		// Cycle weapon in RH on first press and on hold at a certain interval.
		void CycleWeaponRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle player RH weapon on first press and on hold at a certain interval.
			// If cycling on hold is not enabled, do nothing here.

			if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kCycleWeaponRH))
			{
				return;
			}

			// Set last cycled form and then cycle.
			a_p->em->lastCycledForm = 
			(
				a_p->em->currentCycledRHWeaponsList[!a_p->em->rhWeaponCategory]
			);
			// Update cycled weapon from the current weapon category.
			a_p->em->CycleWeapons(true);
			auto form = a_p->em->currentCycledRHWeaponsList[!a_p->em->rhWeaponCategory];
			if (form)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, form->GetName()),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (a_p->em->rhWeaponCategory == FavWeaponCyclingCategory::kAllFavorites &&
					 a_p->em->HasCyclableWeaponInCategory
					 (
						 FavWeaponCyclingCategory::kAllFavorites, true
					 ))
			{
				// Will empty both hands if no weapon is returned 
				// while cycling through all favorites.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip 'Fists'", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: '{}' category is empty",
						a_p->playerID + 1,
						a_p->em->FavWeaponCyclingCategoryToString(a_p->em->rhWeaponCategory)
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}

			// Update cycling time point after cycling.
			a_p->lastCyclingTP = SteadyClock::now();
		}

		void GrabObject(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Collect nearby clutter when this bind is held 
			// and the auto-grab clutter setting is enabled.

			// Object manipulation must be enabled.
			if (!Settings::bEnableObjectManipulation)
			{
				return;
			}

			// Check for any incoming projectiles to grab or lootable objects to auto-grab.
			bool shouldGrab = false;

			// Set auto grab TP to action start TP initially.
			// Do not check yet.
			bool justStarted = HelperFuncs::ActionJustStarted(a_p, InputAction::kGrabObject);
			if (justStarted)
			{
				a_p->tm->rmm->isAutoGrabbing = false;
				a_p->lastAutoGrabTP = SteadyClock::now();
			}

			auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
			bool crosshairRefrValidity = 
			(
				crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
			);
			bool canGrabAnotherRefr = a_p->tm->rmm->CanGrabAnotherRefr();
			// Cannot auto-grab on press/hold if facing a target (face a target to throw instead), 
			// or if a refr is targeted by the crosshair (target for the throw), 
			// or if another refr cannot be grabbed.
			if (a_p->mm->reqFaceTarget || crosshairRefrValidity || !canGrabAnotherRefr)
			{
				// Trying to grab, but no more slots available.
				if (!a_p->mm->reqFaceTarget && !crosshairRefrValidity && !canGrabAnotherRefr)
				{
					// Notify the player that they've reached max capacity for grabbed objects.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kActivationInfo,
						fmt::format
						(
							"P{}: <font color=\"#FF0000\">"
							"Cannot grab another object!</font>",
							a_p->playerID + 1
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

				// Base requirements for grabbing not met; return early.
				return;
			}
			
			// Max distance away from the player's torso to check for grabbable objects.
			const float maxGrabDist = a_p->tm->GetMaxActivationDist();
			// Remaining amount of magicka dictates 
			// how forgiving the projectile grab frame window is
			// by scaling down the additional distance added to the capture radius
			// from the projectile's distance traveled per frame.
			// The more remaining magicka, the larger the window.
			float remainingMagickaRatio = std::clamp
			(
				a_p->coopActor->GetActorValue(RE::ActorValue::kMagicka) /
				max(1.0f, Util::GetFullAVAmount(a_p->coopActor.get(), RE::ActorValue::kMagicka)),
				0.0f,
				1.0f
			);
			// List of handles for projectiles/released refrs to grab, if any.
			std::vector<RE::ObjectRefHandle> projectilesToGrabHandlesList{ };
			// Single handle for the refr to auto-grab, if any.
			RE::ObjectRefHandle autoGrabRefrHandle{ };
			const auto& playerTorsoPos = a_p->mm->playerTorsoPosition;
			// Check if any incoming projectiles are within the player's grab radius
			// and can be grabbed.
			bool grabIncomingProjectiles = false;
			if (justStarted || Settings::bGrabIncomingProjectilesOnHold)
			{
				// Maximum number of frames from the player that a projectile 
				// will be considered as grabbable.
				// Framerate independent; grab window lasts the same number of seconds.
				const float incomingProjGrabFrames = 
				(
					(Settings::fFrameWindowToGrabIncomingProjectiles * remainingMagickaRatio) * 
					((1.0f / *g_deltaTimeRealTime) / 60.0f)
				); 
				// Approximation of the player's width.
				const float playerWidth = 
				(
					0.5f * a_p->coopActor->GetScale() *
					Util::GetXYDistance
					(
						a_p->coopActor->GetBoundMax(),
						a_p->coopActor->GetBoundMin()
					)
				);
				auto isInGrabbingRange = 
				[
					&a_p, 
					&incomingProjGrabFrames, 
					&playerWidth
				]
				(RE::TESObjectREFR* a_projectile) -> bool
				{
					// Max bound range (guaranteed grab) 
					// if grabbing the player's own projectile when not firing at a target.
					float projDistWindow = FLT_MAX;
					auto asProjectile = a_projectile->As<RE::Projectile>();

					// Otherwise, the projectile is a released refr 
					// or is shot by someone else or by the player and directed at a target.
					if (!asProjectile || 
						asProjectile->shooter != a_p->coopActor->GetHandle() ||
						Util::HandleIsValid(asProjectile->desiredTarget))
					{
						RE::NiPoint3 gameVelocity{ }; 
						a_projectile->GetLinearVelocity(gameVelocity);
						if (asProjectile)
						{
							gameVelocity = asProjectile->linearVelocity;
						}
						else if (auto hkpRigidBodyPtr = Util::GethkpRigidBody(a_projectile);
								 hkpRigidBodyPtr)
						{
							gameVelocity = ToNiPoint3
							(
								hkpRigidBodyPtr->motion.linearVelocity * HAVOK_TO_GAME
							);
						}

						projDistWindow =
						(
							gameVelocity.Length() * 
							*g_deltaTimeRealTime * 
							incomingProjGrabFrames
						) + playerWidth;
					}

					if (projDistWindow != FLT_MAX)
					{
						// REMOVE when done debugging.
						/*if (auto trueHUD = TrueHUDCompat::g_trueHUDAPI3; trueHUD)
						{
							trueHUD->DrawCapsule
							(
								a_p->coopActor->data.location + 
								RE::NiPoint3(0.0f, 0.0f, 0.5f * a_p->coopActor->GetHeight()),
								0.5f * a_p->coopActor->GetHeight() + projDistWindow,
								projDistWindow,
								RE::NiQuaternion(0.0f, 0.0f, 1.0f, 0.0f),
								0.0f,
								Settings::vuOverlayRGBAValues[a_p->playerID],
								2.0f
							);
						}*/

						// Not close enough to grab.
						// Separate XY plane and Z axis distance checks -> 
						// Capsule:
						// height: 2 * distance window 
						// radius: distance window
						bool playerLevelAndTooFarAway = 
						(
							(
								a_projectile->data.location.z >= 
								a_p->coopActor->data.location.z 
							) &&
							(
								a_projectile->data.location.z <=
								a_p->coopActor->data.location.z + 
								a_p->coopActor->GetHeight()
							) &&
							(
								Util::GetXYDistance
								(
									a_projectile->data.location, a_p->coopActor->data.location
								) > projDistWindow	
							)
						);
						if (playerLevelAndTooFarAway)
						{
							return false;
						}
						
						bool aboveAndTooFarAway = 
						(
							a_projectile->data.location.z > 
							a_p->coopActor->data.location.z + a_p->coopActor->GetHeight() &&
							a_projectile->data.location.GetDistance
							(
								a_p->coopActor->data.location + 
								RE::NiPoint3(0.0f, 0.0f, a_p->coopActor->GetHeight())
							) > projDistWindow
						);
						if (aboveAndTooFarAway)
						{
							return false;
						}
						
						bool belowAndTooFarAway = 
						(
							a_projectile->data.location.z < a_p->coopActor->data.location.z &&
							a_projectile->data.location.GetDistance
							(
								a_p->coopActor->data.location
							) > projDistWindow	
						);
						if (belowAndTooFarAway)
						{
							return false;
						}
					}

					return true;
				};

				auto projectileMgr = RE::Projectile::Manager::GetSingleton();
				if (projectileMgr)
				{
					projectileMgr->projectileLock.Lock();
					for (const auto& handle : projectileMgr->unlimited)
					{
						auto projectilePtr = Util::GetRefrPtrFromHandle(handle);
						// Invalid projectile.
						if (!projectilePtr)
						{
							continue;
						}

						// Not already grabbed.
						if (a_p->tm->rmm->IsManaged(handle, true))
						{
							continue;
						}
					
						auto asProjectile = projectilePtr->As<RE::Projectile>();
						// Ignore limited projectiles, those that've hit something,
						// those who have lived longer than the max duration for released refrs,
						// and any projectile types which do not fly along a trajectory.
						if ((asProjectile->ShouldBeLimited() || 
							!asProjectile->impacts.empty() ||
							asProjectile->livingTime > 
							Settings::fMaxSecsBeforeClearingReleasedRefr) ||
							(!asProjectile->As<RE::ArrowProjectile>() &&
							!asProjectile->As<RE::MissileProjectile>() &&
							!asProjectile->As<RE::ConeProjectile>()))
						{
							continue;
						}

						// Ignore projectiles that are out of range.
						if (!isInGrabbingRange(asProjectile))
						{
							continue;
						}

						bool grabbedByAnotherPlayer = false;
						for (const auto& otherP : glob.coopPlayers)
						{
							if (!otherP->isActive || otherP == a_p)
							{
								continue;
							}

							if (!grabbedByAnotherPlayer && 
								otherP->tm->rmm->IsManaged(handle, true))
							{
								grabbedByAnotherPlayer = true;
							}

							// If released by another player, 
							// remove this refr from that player's released refr manager,
							// since this player is about to grab it.
							if (otherP->tm->rmm->IsManaged(handle, false))
							{
								otherP->tm->rmm->ClearRefr(handle);
							}
						}

						// Can only manipulate this object if it is not grabbed by another player 
						// and has a supported motion type.
						if (!grabbedByAnotherPlayer)
						{
							// Found an object to auto-grab.
							// Save handle and add it to the list of grabbable refr handles.
							projectilesToGrabHandlesList.emplace_back(handle);
							// Signal as trying to grab and grabbing an incoming projectile.
							if (!shouldGrab || !grabIncomingProjectiles)
							{
								shouldGrab = true;
								grabIncomingProjectiles = true;
							}
						}
					}
					projectileMgr->projectileLock.Unlock();
				}

				// Check for refrs released by other players next.
				// Temporary mapping of an attacking player's CID 
				// to a list of grabbable released refr candidates.
				// Done to avoid modifying the released refr info list until after looping through.
				std::unordered_map<int32_t, std::vector<RE::ObjectRefHandle>> 
				playerLinkedGrabCandidates{ };
				for (const auto& otherP : glob.coopPlayers)
				{
					if (!otherP->isActive || otherP == a_p)
					{
						continue;
					}

					for (const auto& releasedRefrInfo : otherP->tm->rmm->releasedRefrInfoList)
					{
						if (!releasedRefrInfo->IsValid() || 
							releasedRefrInfo->firstHitTP.has_value())
						{
							continue;
						}

						auto releasedRefrPtr = Util::GetRefrPtrFromHandle
						(
							releasedRefrInfo->refrHandle
						);

						// Just has to be in range to grab.
						if (isInGrabbingRange(releasedRefrPtr.get()))
						{
							const auto iter = playerLinkedGrabCandidates.find
							(
								otherP->controllerID
							);
							if (iter != playerLinkedGrabCandidates.end())
							{
								iter->second.emplace_back
								(
									releasedRefrInfo->refrHandle
								);
							}
							else
							{
								playerLinkedGrabCandidates.insert
								(
									{ otherP->controllerID, { releasedRefrInfo->refrHandle} }
								);
							}
						}
					}
				}

				// Remove all candidates from the attacking player's managed released refrs list.
				for (const auto& [cid, candidateHandles] : playerLinkedGrabCandidates)
				{
					// Should never happen. But who knows.
					if (cid == -1)
					{
						continue;
					}

					for (const auto& handle : candidateHandles)
					{
						glob.coopPlayers[cid]->tm->rmm->ClearRefr(handle);
						// Add to list of handles for refrs to grab.
						projectilesToGrabHandlesList.emplace_back(handle);
						// Signal as trying to grab and grabbing an incoming 'projectile'.
						if (!shouldGrab || !grabIncomingProjectiles)
						{
							shouldGrab = true;
							grabIncomingProjectiles = true;
						}
					}
				}
			}
			
			// Next up, auto-grab checks.
			// Check for nearby lootable refrs within activation range 
			// if no projectile was grabbed and if auto-grab is enabled.
			if (!grabIncomingProjectiles)
			{
				// Nothing to do if auto-grab is not enabled.
				if (!Settings::bAutoGrabNearbyLootableObjectsOnHold)
				{
					return;
				}

				// Can auto-grab if the check interval has passed,
				// if not facing a target (face a target to throw instead), 
				// if no refr is targeted by the crosshair, 
				// and if another refr can be grabbed.
				
				// Reset flag before the next check(s).
				shouldGrab = false;
				// Check for next object to grab at an interval.
				float secsSinceLastAutoGrab = Util::GetElapsedSeconds(a_p->lastAutoGrabTP);
				if (secsSinceLastAutoGrab > Settings::fSecsBeforeAutoGrabbingNextLootableObject) 
				{
					a_p->lastAutoGrabTP = SteadyClock::now();
				}
				else
				{
					// Auto-grab interval has not elapsed yet, so nothing to grab; return early.
					return;
				}

				Util::ForEachReferenceInRange
				(
					playerTorsoPos, 
					maxGrabDist, 
					true,
					[&a_p, &shouldGrab, &autoGrabRefrHandle](RE::TESObjectREFR* a_refr) 
					{
						// No refr, continue.
						if (!a_refr)
						{
							return RE::BSContainer::ForEachResult::kContinue;
						}

						auto handle = a_refr->GetHandle(); 
						// Continue if handle or 3D is invalid.
						if (!Util::HandleIsValid(handle) || !a_refr->Is3DLoaded())
						{
							return RE::BSContainer::ForEachResult::kContinue;
						}

						// Skip over any active projectiles that we've already handled above.
						auto asProj = a_refr->As<RE::Projectile>(); 
						if (asProj && !asProj->ShouldBeLimited())
						{
							return RE::BSContainer::ForEachResult::kContinue;
						}

						auto baseObj = a_refr->GetBaseObject();
						// Continue if the object is invalid, not lootable, or cannot be grabbed.
						if ((!baseObj) || 
							(!Util::IsLootableObject(*baseObj)) || 
							(!a_p->tm->rmm->CanGrabRefr(handle)))
						{
							return RE::BSContainer::ForEachResult::kContinue;
						}

						auto refr3DPtr = Util::GetRefr3D(a_refr);
						// Continue if refr's 3D is unavailable.
						if (!refr3DPtr)
						{
							return RE::BSContainer::ForEachResult::kContinue;
						}

						auto hkpRigidBodyPtr = Util::GethkpRigidBody(refr3DPtr.get());
						// Continue if refr's havok rigid body is unavailable.
						if (!hkpRigidBodyPtr)
						{
							return RE::BSContainer::ForEachResult::kContinue;
						}

						bool grabbedByAnotherPlayer = false;
						for (const auto& otherP : glob.coopPlayers)
						{
							if (!otherP->isActive || otherP == a_p)
							{
								continue;
							}

							if (!grabbedByAnotherPlayer && 
								otherP->tm->rmm->IsManaged(handle, true))
							{
								grabbedByAnotherPlayer = true;
							}

							// If released by another player, 
							// remove this refr from that player's released refr manager,
							// since this player is about to grab it.
							if (otherP->tm->rmm->IsManaged(handle, false))
							{
								otherP->tm->rmm->ClearReleasedRefr(handle);
							}
						}

						// Can only manipulate this object if it is not grabbed by another player 
						// and has a supported motion type.
						shouldGrab = 
						{ 
							!grabbedByAnotherPlayer &&
							hkpRigidBodyPtr->motion.type !=
							RE::hkpMotion::MotionType::kFixed &&
							hkpRigidBodyPtr->motion.type !=
							RE::hkpMotion::MotionType::kInvalid
						};
						if (shouldGrab)
						{
							// Found an object to auto-grab.
							// Save handle and stop iterating through nearby refrs.
							autoGrabRefrHandle = handle;
							return RE::BSContainer::ForEachResult::kStop;
						}
						else
						{
							// On to the next refr.
							return RE::BSContainer::ForEachResult::kContinue;
						}
					}
				);
			}
			
			// Grab the valid refr and notify the player.
			if (shouldGrab)
			{
				if (grabIncomingProjectiles)
				{
					// Cool catch hit art effect.
					auto wardEffect = RE::TESForm::LookupByID<RE::BGSArtObject>(0xEA518);
					size_t numGrabbedProjectiles = 0;
					for (const auto& projHandle : projectilesToGrabHandlesList)
					{
						if (!Util::HandleIsValid(projHandle) || 
							!a_p->tm->rmm->CanGrabRefr(projHandle))
						{
							continue;
						}

						auto targetRefrPtr = projHandle.get();
						auto asProjectile = targetRefrPtr->As<RE::Projectile>();
						// If the refr is a fired projectile, expend magicka before grabbing.
						if (asProjectile)
						{
							float magickaCost = 0.0f;
							if (asProjectile->spell)
							{
								// NOTE:
								// Magicka cost is auto-scaled by the player-specific multiplier
								// in CalculateMagickaCost() is because of our hook.
								magickaCost = asProjectile->spell->CalculateMagickaCost
								(
									a_p->coopActor.get()
								);
							}
							else 
							{
								magickaCost = 
								(
									(asProjectile->power) *
									(
										asProjectile->weaponDamage + 
										(
											asProjectile->ammoSource ? 
											asProjectile->ammoSource->data.damage :
											1.0f
										)
									)
								);
							}

							// Expend magicka.
							if (!a_p->isInGodMode && magickaCost > 0.0f)
							{
								// Tack on object manipulation cost mult.
								magickaCost *= 
								(
									Settings::vfObjectManipulationMagickaCostMult[a_p->playerID]
								);
								a_p->pam->ModifyAV(RE::ActorValue::kMagicka, -magickaCost);
							}
						}
	
						if (wardEffect)
						{
							Util::StartHitArt
							(
								targetRefrPtr.get(),
								wardEffect,
								targetRefrPtr.get(),
								5.0f,
								false,
								false
							);
						}

						// Is now grabbing the refr.
						a_p->tm->SetIsGrabbing(true);
						auto index = a_p->tm->rmm->AddGrabbedRefr
						(
							glob.coopPlayers[a_p->controllerID], targetRefrPtr->GetHandle()
						);
						a_p->tm->rmm->isAutoGrabbing = true;
						if (index > -1 && index < a_p->tm->rmm->grabbedRefrInfoList.size())
						{
							const auto& info = a_p->tm->rmm->grabbedRefrInfoList[index];
							// Zero out velocity.
							info->lastSetVelocity = RE::NiPoint3();

							// Stop the projectile momentarily do it does not continue
							// towards the target player and potentially hit them 
							// before the next UpdateImpl() call.
							if (asProjectile)
							{
								asProjectile->linearVelocity = RE::NiPoint3();
								asProjectile->UpdateImpl(0.0f);
							}
						}

						numGrabbedProjectiles++;
					}

					if (numGrabbedProjectiles > 0)
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kActivationInfo,
							fmt::format
							(
								"P{}: <font color=\"#00FF00\">"
								"Intercepted {} {}!</font>",
								a_p->playerID + 1,
								numGrabbedProjectiles,
								numGrabbedProjectiles > 1 ? "projectiles" : "projectile"
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
				else if (Util::HandleIsValid(autoGrabRefrHandle) && 
						 a_p->tm->rmm->CanGrabRefr(autoGrabRefrHandle))
				{
					auto targetRefrPtr = autoGrabRefrHandle.get();
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kActivationInfo,
						fmt::format
						(
							"P{}: Auto-grabbing {}",
							a_p->playerID + 1, 
							targetRefrPtr->GetName()
						),
						{
							CrosshairMessageType::kNone,
							CrosshairMessageType::kEquippedItem,
							CrosshairMessageType::kStealthState,
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
					a_p->tm->SetIsGrabbing(true);
					a_p->tm->rmm->AddGrabbedRefr
					(
						glob.coopPlayers[a_p->controllerID], targetRefrPtr->GetHandle()
					);
					a_p->tm->rmm->isAutoGrabbing = true;
				}
			}
		}

		void HotkeyEquip(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Choose hotkey slot based on RS orientation,
			// then check the slot for an valid form to cache before equipping.
			// If the player has also chosen which slot to equip the form into
			// while this action is occurring, attempt to equip the chosen form.

			if (HelperFuncs::ActionJustStarted(a_p, InputAction::kHotkeyEquip)) 
			{
				a_p->em->lastChosenHotkeyedForm = nullptr;
			}

			// Get the form from the selected hotkey slot.
			RE::TESForm* selectedHotkeyedForm = nullptr;
			auto hotkeySlot = HelperFuncs::GetSelectedHotkeySlot(a_p);
			// Invalid hotkey slot, should not happen, but notify the player anyways.
			if (hotkeySlot == -1)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kHotkeySelection,
					fmt::format("P{}: Invalid Hotkey ({})", a_p->playerID + 1, hotkeySlot + 1),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
				);
				return;
			}

			if (selectedHotkeyedForm = a_p->em->hotkeyedForms[hotkeySlot]; selectedHotkeyedForm)
			{
				// The selected hotkey has a form assigned.
				// Notify the player.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kHotkeySelection,
					fmt::format
					(
						"P{}: Hotkey ({}): {}", 
						a_p->playerID + 1, 
						hotkeySlot + 1, 
						selectedHotkeyedForm->GetName()
					),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
				);
			}
			else
			{
				// No form assigned to hotkey.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kHotkeySelection,
					fmt::format("P{}: Hotkey ({}): NONE", a_p->playerID + 1, hotkeySlot + 1),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					max(0.5f, Settings::fSecsBetweenDiffCrosshairMsgs * 0.5f)
				);
			}

			// Update last chosen hotkeyed form.
			a_p->em->lastChosenHotkeyedForm = selectedHotkeyedForm;

			// Check if any of the 'LT/RT/LB/RB' buttons were just released,
			// which indicates that the player wishes to 
			// equip the selected hotkeyed form into the LH/RH/QS Item/QS Spell slots.
			// Then, equip into the requested slot.
			if (glob.cdh->GetInputState(a_p->controllerID, InputAction::kLT).justReleased)
			{
				HelperFuncs::EquipHotkeyedForm
				(
					a_p, a_p->em->lastChosenHotkeyedForm, EquipIndex::kLeftHand
				);
			}
			else if (glob.cdh->GetInputState(a_p->controllerID, InputAction::kRT).justReleased)
			{
				HelperFuncs::EquipHotkeyedForm
				(
					a_p, a_p->em->lastChosenHotkeyedForm, EquipIndex::kRightHand
				);
			}
			else if (glob.cdh->GetInputState
					 (
						 a_p->controllerID, InputAction::kLShoulder
					 ).justReleased)
			{
				HelperFuncs::EquipHotkeyedForm
				(
					a_p, a_p->em->lastChosenHotkeyedForm, EquipIndex::kQuickSlotItem
				);
			}
			else if (glob.cdh->GetInputState
					 (
						 a_p->controllerID, InputAction::kRShoulder
					 ).justReleased)
			{
				HelperFuncs::EquipHotkeyedForm
				(
					a_p, a_p->em->lastChosenHotkeyedForm, EquipIndex::kQuickSlotSpell
				);
			}
		}

		void QuickSlotCast(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cast QS spell if one is equipped, the player's instant caster is available,
			// and if the player has enough magicka to cast the spell.

			auto quickSlotSpell = a_p->em->quickSlotSpell;
			if (!quickSlotSpell)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kGeneralNotification,
					fmt::format("P{}: No quick slot spell equipped!", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection
					},
					0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
				);
				return;
			}

			if (!HelperFuncs::EnoughOfAVToPerformPA(a_p, InputAction::kQuickSlotCast))
			{
				return;
			}

			// Quick slot bound weapon equip.
			// NOTE: 
			// For P1, not all bound weapons can be equipped with a quick slot cast.
			// If I recall correctly, the bound bow spell fails.
			auto assocWeap = Util::GetAssociatedBoundWeap(quickSlotSpell); 
			if (assocWeap && !a_p->isPlayer1)
			{
				HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kQuickSlotCast);
			}
			else
			{
				auto magicCaster = a_p->coopActor->GetMagicCaster
				(
					RE::MagicSystem::CastingSource::kInstant
				); 
				if (!magicCaster)
				{
					return;
				}

				bool justStarted = 
				(
					(HelperFuncs::ActionJustStarted(a_p, InputAction::kQuickSlotCast)) ||
					(
						a_p->pam->reqSpecialAction == SpecialActionType::kQuickCast &&
						HelperFuncs::ActionJustStarted(a_p, InputAction::kSpecialAction)
					)
				);
				// Since the targeting manager's update call runs 
				// after the player action manager's main task,
				// any selected aim correction target would get set 
				// after we already start casting the spell, unless it is set here first.
				a_p->tm->UpdateAimCorrectionTarget();
				// Update ranged attack package target.
				a_p->tm->UpdateAimTargetLinkedRefr(EquipIndex::kQuickSlotSpell);
				// Aim downward at the ground below if casting a target location spell.
				HelperFuncs::PrepForTargetLocationSpellCast(a_p, quickSlotSpell);
				// Perform the cast.
				a_p->pam->CastSpellWithMagicCaster
				(
					EquipIndex::kQuickSlotSpell,
					justStarted,
					true,
					false,
					Util::ShouldCastWithP1(quickSlotSpell)
				);
			}
		}

		void Shout(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Queue up emulated shout button press for P1 while the Shout bind is held.
			// TODO:
			// For companion players: play shout anims and charge shout on hold, just like for P1.

			// Will notify the player that they do not have voice magic equipped on release.
			if (!a_p->em->voiceForm)
			{
				return;
			}

			if (a_p->isPlayer1)
			{
				if (HelperFuncs::ActionJustStarted(a_p, InputAction::kShout)) 
				{
					// Only have to toggle AI driven on first press.
					// Send button event right away without queuing.
					a_p->pam->SendButtonEvent
					(
						InputAction::kShout, 
						RE::INPUT_DEVICE::kGamepad,
						ButtonEventPressType::kInstantTrigger
					);
				}
				else
				{
					// Queue event afterward.
					a_p->pam->QueueP1ButtonEvent
					(
						InputAction::kShout, 
						RE::INPUT_DEVICE::kGamepad, 
						ButtonEventPressType::kPressAndHold, 
						0.0f,
						false
					);
				}
			}
		}

		void SpecialAction(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Perform the requested special action on press/on hold.

			bool justStarted = HelperFuncs::ActionJustStarted(a_p, InputAction::kSpecialAction);
			const auto& pam = a_p->pam;
			const auto& em = a_p->em;

			// [On Press/On Press and Release Special Actions]:
			if (pam->reqSpecialAction == SpecialActionType::kBlock) 
			{
				// Only start blocking on press or if interrupted.
				if (justStarted || !pam->isBlocking)
				{				
					HelperFuncs::BlockInstant(a_p, true);
				}
			}

			// [On Hold Special Actions]:
			float holdTime = a_p->pam->GetPlayerActionInputHoldTime(InputAction::kSpecialAction);
			if (pam->reqSpecialAction == SpecialActionType::kCastBothHands ||
				pam->reqSpecialAction == SpecialActionType::kDualCast)
			{
				if (a_p->isPlayer1)
				{
					// Update desired targets for both hand casters first.
					HelperFuncs::UpdatePlayerMagicCasterTarget
					(
						a_p, RE::MagicSystem::CastingSource::kLeftHand
					);
					HelperFuncs::UpdatePlayerMagicCasterTarget
					(
						a_p, RE::MagicSystem::CastingSource::kRightHand
					);

					// Send button events to cast with both hands.
					// Togging AI driven not requird.
					if (justStarted)
					{
						a_p->pam->SendButtonEvent
						(
							InputAction::kCastLH,
							RE::INPUT_DEVICE::kGamepad,
							ButtonEventPressType::kInstantTrigger,
							0.0f,
							false
						);
						a_p->pam->SendButtonEvent
						(
							InputAction::kCastRH,
							RE::INPUT_DEVICE::kGamepad,
							ButtonEventPressType::kInstantTrigger, 
							0.0f, 
							false
						);
					}
					else
					{
						a_p->pam->SendButtonEvent
						(
							InputAction::kCastLH, 
							RE::INPUT_DEVICE::kGamepad, 
							ButtonEventPressType::kPressAndHold, 
							holdTime,
							false
						);
						a_p->pam->SendButtonEvent
						(
							InputAction::kCastRH,
							RE::INPUT_DEVICE::kGamepad, 
							ButtonEventPressType::kPressAndHold, 
							holdTime, 
							false
						);
					}
				}
				else if (em->HasLHSpellEquipped() && em->HasRHSpellEquipped())
				{
					auto lhSpell = em->GetLHSpell();
					auto rhSpell = em->GetRHSpell();
					auto assocWeapLH = Util::GetAssociatedBoundWeap(lhSpell);
					auto assocWeapRH = Util::GetAssociatedBoundWeap(rhSpell);
					bool shouldCastWithP1 = false;
					bool justStarted = HelperFuncs::ActionJustStarted
					(
						a_p, InputAction::kSpecialAction
					);
					if (!assocWeapLH && !assocWeapRH) 
					{
						// No bound weapons to equip, so cast with both hands.
						HelperFuncs::SetUpCastingPackage(a_p, true, true, justStarted);
						shouldCastWithP1 = Util::ShouldCastWithP1(lhSpell); 
						if (shouldCastWithP1)
						{
							a_p->pam->CastSpellWithMagicCaster
							(
								EquipIndex::kLeftHand, justStarted, true, true, shouldCastWithP1
							);
						}

						shouldCastWithP1 = Util::ShouldCastWithP1(rhSpell);
						if (shouldCastWithP1)
						{
							a_p->pam->CastSpellWithMagicCaster
							(
								EquipIndex::kRightHand, justStarted, true, true, shouldCastWithP1
							);
						}
					}
					else if (assocWeapLH && assocWeapRH)
					{
						// Bound weaps in both hands.
						HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kSpecialAction);
					}
					else if (assocWeapLH)
					{
						// Bound weap in LH, regular spell cast in RH.
						HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kSpecialAction);
						HelperFuncs::SetUpCastingPackage(a_p, false, true, justStarted);

						shouldCastWithP1 = Util::ShouldCastWithP1(rhSpell);
						if (shouldCastWithP1)
						{
							a_p->pam->CastSpellWithMagicCaster
							(
								EquipIndex::kRightHand, justStarted, true, true, shouldCastWithP1
							);
						}
					}
					else
					{
						// Bound weap in RH, regular spell cast in LH.
						HelperFuncs::HandleBoundWeaponEquip(a_p, InputAction::kSpecialAction);
						HelperFuncs::SetUpCastingPackage(a_p, true, false, justStarted);

						shouldCastWithP1 = Util::ShouldCastWithP1(lhSpell);
						if (shouldCastWithP1)
						{
							a_p->pam->CastSpellWithMagicCaster
							(
								EquipIndex::kLeftHand, justStarted, true, true, shouldCastWithP1
							);
						}
					}
				}
				else if ((em->HasLHStaffEquipped() && em->HasRHStaffEquipped()) &&
						 (pam->usingLHStaff->value != 1.0f || pam->usingRHStaff->value != 1.0f))
				{
					// Cast with both staves at once.
					pam->CastStaffSpell(em->GetLHWeapon(), true, true);
					pam->CastStaffSpell(em->GetRHWeapon(), false, true);
				}
			}
			else if (pam->reqSpecialAction == SpecialActionType::kQuickCast)
			{
				// Quick slot cast.
				ProgressFuncs::QuickSlotCast(a_p);
			}
			else if (pam->reqSpecialAction == SpecialActionType::kGetUp)
			{
				// Hold the special action bind to get up when knocked down.
				if (a_p->pam->GetPlayerActionInputHoldTime(InputAction::kSpecialAction) > 
					Settings::fSecsDefMinHoldTime && 
					a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kGetUp) 
				{
					// Stop other player from grabbing this player 
					// or get up if ragdolled on the ground.
					auto isGrabbedIter = std::find_if
					(
						glob.coopPlayers.begin(), glob.coopPlayers.end(),
						[&a_p](const auto& p) 
						{
							return 
							(
								p->isActive &&
								p->coopActor != a_p->coopActor && 
								p->tm->rmm->IsManaged(a_p->coopActor->GetHandle(), true)
							);
						}
					);

					// Release this player. Now!
					if (isGrabbedIter != glob.coopPlayers.end())
					{
						const auto& grabbingP = *isGrabbedIter;
						grabbingP->tm->rmm->ClearRefr(a_p->coopActor->GetHandle());
					}

					// Reset fall time and height before attempting to get up.
					if (auto charController = a_p->coopActor->GetCharController(); charController)
					{
						charController->lock.Lock();
						Util::AdjustFallState(charController, false);
						charController->lock.Unlock();
					}

					a_p->coopActor->NotifyAnimationGraph("GetUpBegin");
					a_p->coopActor->PotentiallyFixRagdollState();
				}
			}
			else if (pam->reqSpecialAction == SpecialActionType::kCycleOrPlayEmoteIdle && 
					 holdTime > max(Settings::fSecsDefMinHoldTime, Settings::fSecsCyclingInterval))
			{
				// Must be held long enough to start cycling.
				if (!HelperFuncs::CanCycleOnHold(a_p, InputAction::kSpecialAction))
				{
					return;
				}

				// Set last cycled emote and index before cycling.
				a_p->em->lastCycledIdleIndexPair = a_p->em->currentCycledIdleIndexPair;
				a_p->em->CycleEmoteIdles();
				// Get new idle and notify the player.
				const auto& emoteIdleToPlay = a_p->em->currentCycledIdleIndexPair.first;
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: Selected emote idle '{}'", a_p->playerID + 1, emoteIdleToPlay
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);

				// Update cycling time point after cycling.
				a_p->lastCyclingTP = SteadyClock::now();
			}
		}
	};

	namespace StartFuncs
	{
		void ActivateAllOfType(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Get all nearby objects of the same type and activate/loot each.
			
			// Temp menus are open, so ignore the request to loot all.
			// Can cause a crash with Skyrim Souls if one player loots from a container 
			// while another player is viewing the container via the Container Menu.
			if (!Util::MenusOnlyAlwaysOpen())
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kActivationInfo,
					fmt::format("P{}: Another player is controlling menus", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
				return;
			}

			// If the player has a crosshair refr, get all refrs of the same type.
			// Otherwise, choose what refrs to loot based on the closest refr 
			// in the player's facing direction.
			auto activationRefrPtr = Util::GetRefrPtrFromHandle
			(
				a_p->tm->activationRefrHandle
			);
			bool activationRefrValidity = 
			(
				activationRefrPtr && Util::IsValidRefrForTargeting(activationRefrPtr.get())
			);
			if (!activationRefrValidity)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kActivationInfo,
					fmt::format
					(
						"P{}: No object chosen to interact with", 
						a_p->playerID + 1
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
				return;
			}
			
			auto baseObj = activationRefrPtr->GetBaseObject(); 
			if (!a_p->tm->RefrIsInActivationRange(a_p->tm->activationRefrHandle))
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kActivationInfo,
					fmt::format
					(
						"P{}: {} is too far away", 
						a_p->playerID + 1,
						baseObj ? baseObj->GetName() : activationRefrPtr->GetName()
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
				return;
			}

			bool hasCrosshairRefr = a_p->tm->activationRefrHandle == a_p->tm->crosshairRefrHandle;
			uint32_t lootedObjects = 0;
			if (hasCrosshairRefr)
			{
				auto asActor = activationRefrPtr->As<RE::Actor>();
				// Next, the targeted refr must not be off limits, 
				// or the player must be sneaking to signal intent to steal.
				// Finally, the refr must be a lootable loose refr,
				// a corpse, or an unlocked container.
				if (activationRefrPtr->IsCrimeToActivate() && !a_p->coopActor->IsSneaking())
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kActivationInfo,
						fmt::format
						(
							"P{}: Sneak to <font color=\"#FF0000\">interact</font> "
							"with every '{}' in range", 
							a_p->playerID + 1,
							baseObj ? baseObj->GetName() : activationRefrPtr->GetName()
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
					return;
				}
			
				bool isLootableRefr = 
				(
					(Util::IsLootableRefr(activationRefrPtr.get())) || 
					(
						asActor &&
						asActor->IsDead()
					)	
				);
				if (!isLootableRefr)
				{
					if (activationRefrPtr->IsLocked() ||
						asActor || 
						!activationRefrPtr->HasContainer())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kActivationInfo,
							fmt::format
							(
								"P{}: Cannot interact with every '{}' in range", 
								a_p->playerID + 1,
								baseObj ? baseObj->GetName() : activationRefrPtr->GetName()
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs
						);
						return;
					}
				}

				// If the targeted activation refr is grabbed,
				// loot all lootable objects that the player is currently grabbing.
				bool isGrabbedRefr = a_p->tm->rmm->IsManaged(activationRefrPtr->GetHandle(), true);
				if (isGrabbedRefr) 
				{
					// Have to clear all grabbed objects before looting,
					// since looting and removing one at a time
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
					// Loot all saved grabbed refrs.
					for (const auto& grabbedRefrHandle : grabbedRefrHandles) 
					{
						auto grabbedRefrPtr = Util::GetRefrPtrFromHandle(grabbedRefrHandle);
						if (!grabbedRefrPtr)
						{
							continue;
						}

						if (Util::IsLootableRefr(grabbedRefrPtr.get()))
						{
							// Loot individual item/aggregation of the same item.
							lootedObjects += HelperFuncs::LootRefr(a_p, grabbedRefrPtr);
						}
						else if (grabbedRefrPtr->As<RE::Actor>() && grabbedRefrPtr->IsDead())
						{
							// Loot all items from grabbed corpse.
							lootedObjects += HelperFuncs::LootAllItemsFromCorpse
							(
								a_p, grabbedRefrPtr
							);
						}
					}

					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kActivationInfo,
						fmt::format
						(
							"P{}: Looted {} {} from all grabbed objects",
							a_p->playerID + 1, 
							lootedObjects,
							lootedObjects == 1 ? "item" : "items"
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					auto asActor = activationRefrPtr->As<RE::Actor>();
					bool isCorpse = asActor && asActor->IsDead();
					if (isCorpse)
					{
						// Is corpse.
						lootedObjects = HelperFuncs::LootAllItemsFromCorpse
						(
							a_p, activationRefrPtr
						);
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kActivationInfo,
							fmt::format
							(
								"P{}: Looted {} {} from '{}'",
								a_p->playerID + 1, 
								lootedObjects,
								lootedObjects == 1 ? "item" : "items",
								activationRefrPtr->GetName()
							),
							{
								CrosshairMessageType::kNone,
								CrosshairMessageType::kStealthState,
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs
						);
					}
					else if (!activationRefrPtr->IsLocked() && activationRefrPtr->HasContainer())
					{
						// Is unlocked container.
						lootedObjects = HelperFuncs::LootAllItemsFromContainer
						(
							a_p, activationRefrPtr
						);
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kActivationInfo,
							fmt::format
							(
								"P{}: Looted {} {} from container '{}'",
								a_p->playerID + 1,
								lootedObjects,
								lootedObjects == 1 ? "item" : "items",
								activationRefrPtr->GetName()
							),
							{ 
								CrosshairMessageType::kNone,
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs
						);
					}
					else
					{
						// Is lootable refr, but not a corpse or container.
						const auto& nearbyObjectsOfSameType = a_p->tm->GetNearbyRefrsOfSameType
						(
							a_p->tm->activationRefrHandle, Settings::uMaxGrabbedReferences
						);
						for (const auto& handle : nearbyObjectsOfSameType)
						{
							auto objectPtr = Util::GetRefrPtrFromHandle(handle);
							auto objectValidity = 
							(
								objectPtr && Util::IsValidRefrForTargeting(objectPtr.get())
							);
							if (!objectValidity)
							{
								continue;
							}

							// Clear released refr if activating it, 
							// as its 3D will be removed once picked up 
							// and will no longer need to be tracked in the targeting manager.
							if (a_p->tm->rmm->IsManaged(handle, true) || 
								a_p->tm->rmm->IsManaged(handle, false))
							{
								a_p->tm->rmm->ClearRefr(handle);
							}

							lootedObjects += HelperFuncs::LootRefr(a_p, objectPtr);
						}

						// Notify player.
						if (!nearbyObjectsOfSameType.empty())
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kActivationInfo,
								fmt::format
								(
									"P{}: Looted {} of '{}' in range", 
									a_p->playerID + 1, 
									lootedObjects,
									baseObj ? baseObj->GetName() : activationRefrPtr->GetName()
								),
								{
									CrosshairMessageType::kNone, 
									CrosshairMessageType::kStealthState, 
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs
							);
						}
						else
						{
							a_p->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kActivationInfo,
								fmt::format
								(
									"P{}: No '{}' in range", 
									a_p->playerID + 1, 
									baseObj ? baseObj->GetName() : activationRefrPtr->GetName()
								),
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
			}
			else
			{
				// If a loose refr is selected for activation, 
				// activate all lootable objects in range,
				// or if a container is selected for activation,
				// activate all objects from all lootable containers in range.
				// Is lootable refr, but not a corpse or container.
				bool hasContainer = activationRefrPtr->HasContainer();
				bool isLootable = !hasContainer && Util::IsLootableRefr(activationRefrPtr.get());
				bool isStealing = activationRefrPtr->IsCrimeToActivate();
				if (isStealing && !a_p->coopActor->IsSneaking())
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kActivationInfo,
						fmt::format
						(
							"P{}: Sneak to <font color=\"#FF0000\">steal</font> "
							"{}", 
							a_p->playerID + 1,
							hasContainer ? 
							"from every container in range" : 
							"every object in range"
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
					return;
				}

				// Get potential refrs to activate.
				const auto refrsToActivate = a_p->tm->GetLootableRefrsInRange
				(
					hasContainer, Settings::uMaxGrabbedReferences
				);
				for (const auto& handle : refrsToActivate)
				{
					auto objectPtr = Util::GetRefrPtrFromHandle(handle);
					auto objectValidity = 
					(
						objectPtr && Util::IsValidRefrForTargeting(objectPtr.get())
					);
					if (!objectValidity)
					{
						continue;
					}

					// Clear grabbed/released refr if activating it, 
					// as its 3D will be removed once picked up 
					// and will no longer need to be tracked in the targeting manager.
					if (a_p->tm->rmm->IsManaged(handle, true) || 
						a_p->tm->rmm->IsManaged(handle, false))
					{
						a_p->tm->rmm->ClearRefr(handle);
					}

					if (objectPtr->HasContainer())
					{
						if (!objectPtr->As<RE::Actor>())
						{
							lootedObjects += HelperFuncs::LootAllItemsFromContainer
							(
								a_p, objectPtr
							);
						}
						else if (objectPtr->IsDead())
						{
							lootedObjects += HelperFuncs::LootAllItemsFromCorpse(a_p, objectPtr);
						}
					}
					else
					{
						lootedObjects += HelperFuncs::LootRefr(a_p, objectPtr);
					}
				}

				if (hasContainer)
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kActivationInfo,
						fmt::format
						(
							"P{}: {} {} {} from all unlocked containers in range", 
							a_p->playerID + 1,
							isStealing ? "<font color=\"#FF0000\">Stealing</font>" : "Looting",
							lootedObjects,
							lootedObjects == 1 ? "item" : "items"
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kActivationInfo,
						fmt::format
						(
							"P{}: {} {} {} in range", 
							a_p->playerID + 1,
							isStealing ? "<font color=\"#FF0000\">Stealing</font>" : "Looting",
							lootedObjects,
							lootedObjects == 1 ? "item" : "items"
						),
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

		void ActivateCancel(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Prevent activation of any selected refr.
			// Useful when accidentally selecting an item that you do not want to activate.

			auto activationRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle);
			if (!activationRefrPtr) 
			{
				return;
			}

			auto baseObj = activationRefrPtr->GetBaseObject(); 
			if (!baseObj)
			{
				return;
			}
			
			// Notify the player.
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kActivationInfo,
				fmt::format("P{}: Cancelling activation", a_p->playerID + 1, baseObj->GetName()),
				{ 
					CrosshairMessageType::kNone,
					CrosshairMessageType::kStealthState, 
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void Block(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Start blocking with weapon/shield.

			if (!a_p->coopActor->IsWeaponDrawn())
			{
				// Unsheathe if not rotating arms.
				// Do nothing otherwise.
				if (!Settings::bEnableArmsRotation)
				{
					a_p->pam->ReadyWeapon(true);
				}

				return;
			}

			// Stop attacking first.
			a_p->coopActor->NotifyAnimationGraph("attackStop");
			// For Valhalla Combat projectile deflection compatibility.
			// Requires propagation of 'Block' input event.
			if (a_p->isPlayer1) 
			{
				a_p->pam->QueueP1ButtonEvent
				(
					InputAction::kBlock,
					RE::INPUT_DEVICE::kGamepad, 
					ButtonEventPressType::kInstantTrigger,
					0.0f,
					false
				);
				a_p->pam->QueueP1ButtonEvent
				(
					InputAction::kBlock,
					RE::INPUT_DEVICE::kGamepad,
					ButtonEventPressType::kPressAndHold,
					0.1f,
					false
				);
			}

			Util::RunPlayerActionCommand
			(
				RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
			);
			bool canShieldChargeAfter = 
			{
				a_p->mm->lsMoved &&
				a_p->em->HasShieldEquipped() &&
				a_p->coopActor->HasPerk(glob.shieldChargePerk) &&
				a_p->pam->IsPerforming(InputAction::kSprint)
			};
			if (canShieldChargeAfter)
			{
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionSprintStart, a_p->coopActor.get()
				);
			}
		}

		void CamLockOn(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Switch to lock-on state if a new valid actor is targeted
			// by this player's crosshair.
			// Reset to auto-trail otherwise.

			// Give this player control of the camera.
			auto& controllingCID = glob.cam->controlCamCID;
			if (controllingCID != a_p->controllerID && 
				glob.cam->camAdjMode == CamAdjustmentMode::kNone)
			{
				controllingCID = a_p->controllerID;
			}

			// Same controller as the one with control over camera can adjust the cam state freely.
			// Nothing to do otherwise.
			if (controllingCID != a_p->controllerID)
			{
				return;
			}

			// If this player has a selected target actor, it is within LOS on the screen,
			// since actors are not selectable if they are not hit by the crosshair's raycasts.
			auto targetActorPtr = Util::GetActorPtrFromHandle(a_p->tm->selectedTargetActorHandle);
			auto targetActorValidity = 
			(
				targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get())
			);
			// CID of the targeted player.
			// If no player is targeted, the CID is -1.
			int32_t targetPlayerCID = 
			(
				targetActorValidity ? 
				GlobalCoopData::GetCoopPlayerIndex(targetActorPtr.get()) :
				-1
			);
			auto currentFocalPlayerPtr = 
			(
				Settings::bFocalPlayerMode && glob.cam->focalPlayerCID != -1 ?
				glob.coopPlayers[glob.cam->focalPlayerCID]->coopActor :
				nullptr
			);
			auto currentLockOnTargetPtr = 
			(
				Util::HandleIsValid(glob.cam->camLockOnTargetHandle) ? 
				Util::GetActorPtrFromHandle(glob.cam->camLockOnTargetHandle) :
				nullptr
			);
			// Can target another player (not downed) to set as the focal player.
			bool newFocalPlayerTarget = 
			(
				targetActorValidity && 
				Settings::bFocalPlayerMode &&
				targetPlayerCID != -1 &&
				!glob.coopPlayers[targetPlayerCID]->isDowned &&
				targetActorPtr != currentFocalPlayerPtr
			);
			// Can set a new lock-on target if not the focal player or the current target,
			// and is either not a player or is a player that is not downed.
			bool newLockOnTarget = 
			(
				(targetActorValidity) && 
				(!newFocalPlayerTarget) &&
				(targetActorPtr != currentFocalPlayerPtr) &&
				(targetActorPtr != currentLockOnTargetPtr) &&
				(targetPlayerCID == -1 || !glob.coopPlayers[targetPlayerCID]->isDowned)
			);
			if (newFocalPlayerTarget || newLockOnTarget)
			{
				// Set the targeted player as the focal player.
				if (newLockOnTarget)
				{
					// If the crosshair refr target is not the same 
					// as the current cam lock-on target,
					// send a request to update the lock-on target,
					// and switch to the lock-on state.
					glob.cam->lockOnActorReq = a_p->tm->selectedTargetActorHandle;
					glob.cam->camState = CamState::kLockOn;
					// Inform the player.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera lock-on mode", a_p->playerID + 1),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					glob.cam->focalPlayerCID = targetPlayerCID;
					// Inform the player.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kCamera,
						fmt::format
						(
							"P{}: Gave P{} camera focus", 
							a_p->playerID + 1,
							glob.coopPlayers[targetPlayerCID]->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
			}
			else if (targetActorValidity)
			{
				// Clear the selected target, focal player or lock-on target.
				if (targetActorPtr == currentLockOnTargetPtr)
				{
					// Send a request to clear the cam lock-on target
					// and reset the cam state to auto-trail.
					glob.cam->lockOnActorReq = RE::ActorHandle();
					glob.cam->camState = CamState::kAutoTrail;
					// Inform the player of switch back to auto-trail mode.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera auto-trail mode", a_p->playerID + 1),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else if (targetActorPtr == currentFocalPlayerPtr)
				{
					glob.cam->focalPlayerCID = -1;
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kCamera,
						fmt::format
						(
							"P{}: Reset camera focus", 
							a_p->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
			}
			else
			{
				if (glob.cam->focalPlayerCID != -1)
				{
					// Inform the player of the removal of camera focus
					// and the switch back to auto-trail mode.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kCamera,
						fmt::format
						(
							"P{}: Reset camera focus and now auto-trailing", 
							a_p->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					// Inform the player of switch back to auto-trail mode.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kCamera,
						fmt::format("P{}: Camera auto-trail mode", a_p->playerID + 1),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}

				// Clear focal player and current lock-on target if not targeting anything.
				glob.cam->focalPlayerCID = -1;
				// Send a request to clear the cam lock-on target
				// and reset the cam state to auto-trail.
				glob.cam->lockOnActorReq = RE::ActorHandle();
				glob.cam->camState = CamState::kAutoTrail;
			}
		}

		void CamManualPos(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Toggle camera state between auto-trail and manual positioning if the player can
			// obtain control of the camera.

			auto& controllingCID = glob.cam->controlCamCID;
			if (controllingCID != a_p->controllerID &&
				glob.cam->camAdjMode == CamAdjustmentMode::kNone)
			{
				controllingCID = a_p->controllerID;
			}

			// Same controller as one with control over camera can adjust the cam state freely.
			// Nothing to do otherwise.
			if (controllingCID != a_p->controllerID)
			{
				return;
			}

			// Switch to manual positioning state if not already set.
			if (glob.cam->camState != CamState::kManualPositioning)
			{
				// Clear focal player if switching to manual positioning.
				if (glob.cam->camState == CamState::kLockOn && glob.cam->focalPlayerCID != -1)
				{
					glob.cam->focalPlayerCID = -1;
				}

				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kCamera,
					fmt::format("P{}: Camera manual positioning mode", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
				glob.cam->camState = CamState::kManualPositioning;
			}
			else
			{
				// Switch back to auto-trail otherwise.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kCamera,
					fmt::format("P{}: Camera auto-trail mode", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
				glob.cam->camState = CamState::kAutoTrail;
			}
		}

		void ChangeDialoguePlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// If this player is controlling dialogue, hand over dialogue control
			// to another player who has most recently made a request.
			// If this player is not controlling dialogue, send a request for dialogue control.

			if (bool controllingDialogue = glob.menuCID == a_p->controllerID; controllingDialogue)
			{
				// Check for dialogue control request and relinquish menu control 
				// to the requesting player.
				{
					std::unique_lock<std::mutex> lock
					(
						glob.moarm->reqTransferMenuControlMutex, std::try_to_lock
					);
					if (lock)
					{
						if (glob.moarm->reqTransferMenuControlPlayerCID != -1)
						{
							SPDLOG_DEBUG
							(
								"[PAFH] ChangeDialoguePlayer: {}: "
								"Dialogue req CID lock obtained. (0x{:X})",
								a_p->coopActor->GetName(),
								std::hash<std::jthread::id>()(std::this_thread::get_id())
							);
							const auto& reqP = 
							(
								glob.coopPlayers[glob.moarm->reqTransferMenuControlPlayerCID]
							);
							// Teleport the other player to the speaker/this player 
							// before handing over dialogue control if the requesting player 
							// is not close enough to engage in dialogue.
							auto menuTopicMgr = RE::MenuTopicManager::GetSingleton(); 
							if (menuTopicMgr) 
							{
								auto speakerPtr = Util::GetRefrPtrFromHandle
								(
									menuTopicMgr->speaker
								);
								if (!speakerPtr) 
								{
									speakerPtr = Util::GetRefrPtrFromHandle
									(
										menuTopicMgr->lastSpeaker
									);
								}

								if (speakerPtr) 
								{
									float distToSpeaker = 
									(
										reqP->coopActor->data.location.GetDistance
										(
											speakerPtr->data.location
										)
									);
									if (distToSpeaker > Settings::fAutoEndDialogueRadius) 
									{
										Util::TeleportToActor
										(
											reqP->coopActor.get(), a_p->coopActor.get()
										);
									}
								}
								else
								{
									float distToThisPlayer = 
									(
										reqP->coopActor->data.location.GetDistance
										(
											a_p->coopActor->data.location
										)
									);
									if (distToThisPlayer > Settings::fAutoEndDialogueRadius)
									{
										Util::TeleportToActor
										(
											reqP->coopActor.get(), a_p->coopActor.get()
										);
									}
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

							// Notify the requesting player 
							// that they are now in control of dialogue.
							reqP->tm->SetCrosshairMessageRequest
							(
								CrosshairMessageType::kGeneralNotification,
								fmt::format
								(
									"P{}: <font color=\"#E66100\">Now controlling dialogue</font>", 
									reqP->playerID + 1
								),
								{
									CrosshairMessageType::kNone,
									CrosshairMessageType::kStealthState,
									CrosshairMessageType::kTargetSelection 
								},
								Settings::fSecsBetweenDiffCrosshairMsgs
							);

							// Reset req dialogue menu CID to -1 
							// because request was handled (fulfilled or not).
							glob.moarm->reqTransferMenuControlPlayerCID = -1;
						}
					}
				}
			}
			else
			{
				// Submit request for menu control if there isn't one already.
				{
					std::unique_lock<std::mutex> lock
					(
						glob.moarm->reqTransferMenuControlMutex, std::try_to_lock
					);
					if (lock)
					{
						// Another player already requested dialogue control before this player,
						// so their request must be handled first.
						if (glob.moarm->reqTransferMenuControlPlayerCID != -1)
						{
							return;
						}

						SPDLOG_DEBUG
						(
							"[PAFH] ChangeDialoguePlayer: {}: "
							"Req CID NOT set and lock obtained (0x{:X}). Set to {}.",
							a_p->coopActor->GetName(), 
							std::hash<std::jthread::id>()(std::this_thread::get_id()), 
							a_p->controllerID
						);

						// Set requesting player CID.
						glob.moarm->reqTransferMenuControlPlayerCID = a_p->controllerID;
						// Notify player.
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kGeneralNotification,
							fmt::format
							(
								"P{}: <font color=\"#E66100\">Requesting dialogue control</font>", 
								a_p->playerID + 1
							),
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
		}

		void CoopDebugMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the co-op debug menu via script event, which will also set the menu CID.

			glob.onDebugMenuRequest.SendEvent(a_p->coopActor.get(), a_p->controllerID);
		}

		void CoopIdlesMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the co-op idles menu via script event, which will also set the menu CID.

			glob.onCoopHelperMenuRequest.SendEvent
			(
				a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kIdles
			);
		}

		void CoopMiniGamesMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the co-op mini games menu via script event, which will also set the menu CID.

			glob.onCoopHelperMenuRequest.SendEvent
			(
				a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kMiniGames
			);
		}

		void CoopSummoningMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the co-op summoning menu via script event, which will also set the menu CID.
			// Don't open if already open or a player is in combat.

			if (glob.summoningMenuOpenGlob->value != 0.0f)
			{
				return;
			}

			if (glob.isInCoopCombat)
			{
				RE::DebugMessageBox
				(
					"[ALYSLC] A player is in combat. "
					"Please ensure that all players are not in combat "
					"before attempting to open the Summoning Menu."
				);
				return;
			}

			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive)
				{
					continue;
				}

				if (p->isDowned)
				{
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kReviveAlert,
						fmt::format
						(
							"P{}: Cannot summon while a player is downed", a_p->playerID + 1
						),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState,
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
					return;
				}
			}

			glob.onSummoningMenuRequest.SendEvent();
		}

		void DebugReEquipHandForms(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Re-equip items in the player's hands.
			// Useful when those items are bugged out in some way.
			
			a_p->pam->ReadyWeapon(false);
			a_p->em->ReEquipHandForms();
			a_p->pam->ReadyWeapon(true);
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kGeneralNotification,
				fmt::format("P{}: Re-equipped items in hands", a_p->playerID + 1),
				{ 
					CrosshairMessageType::kNone,
					CrosshairMessageType::kStealthState,
					CrosshairMessageType::kTargetSelection 
				},
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
		}

		void DebugResetPlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset the player entirely.
			// A 'nuclear option' if the player is still bugged out 
			// after trying other debug options.
			// - Re-equip hand items/unequip all items, if requested.
			// - Revert form if transformed.
			// - Resurrect.
			// - Reset havok + ragdoll for companion players, if requested.
			// - Disable and re-enable companion players' characters.

			if (a_p->isPlayer1) 
			{
				a_p->ResetPlayer1();
			}	
			else
			{
				// Don't unequip all here.
				// The debug menu reset equip state option does unequip all, 
				// so if the player's equip state is still bugged,
				// choose that reset option instead.
				a_p->taskRunner->AddTask
				(
					[a_p]() { a_p->ResetCompanionPlayerStateTask(false, true); }
				);
			}

			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kGeneralNotification,
				fmt::format("P{}: Reset player", a_p->playerID + 1),
				{ 
					CrosshairMessageType::kNone, 
					CrosshairMessageType::kStealthState,
					CrosshairMessageType::kTargetSelection 
				},
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
			if (auto player3DPtr = Util::GetRefr3D(a_p->coopActor.get()); player3DPtr)
			{
				a_p->coopActor->DetachHavok(player3DPtr.get());
				a_p->coopActor->InitHavok();
				a_p->coopActor->MoveHavok(true);
			}

			// Knock 'em down.
			Util::PushActorAway(a_p->coopActor.get(), a_p->coopActor->data.location, -1.0f);
			a_p->coopActor->PotentiallyFixRagdollState();
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kGeneralNotification,
				fmt::format("P{}: Ragdolling", a_p->playerID + 1),
				{ 
					CrosshairMessageType::kNone,
					CrosshairMessageType::kStealthState,
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void DisableCoopCam(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Disable co-op camera and reset to the normal Third Person cam which follows P1.
			
			// Clear lock-on target and reset adjustment mode before waiting for toggle.
			glob.cam->camAdjMode = CamAdjustmentMode::kNone;
			glob.cam->ClearLockOnData();
			glob.cam->waitForToggle = true;
			glob.cam->RequestStateChange(ManagerState::kPaused);
		}

		void Dismount(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Dismount any mount instantly. 
			// Also stop running the special interaction package 
			// to free the companion player from occupied furniture.
			// NOTE: 
			// Jumping/dodging also unlinks the player from occupied furniture.

			if (a_p->isPlayer1)
			{
				// Activate to dismount for P1.
				a_p->pam->SendButtonEvent
				(
					InputAction::kActivate,
					RE::INPUT_DEVICE::kGamepad, 
					ButtonEventPressType::kInstantTrigger,
					0.0f
				);
				a_p->pam->SendButtonEvent
				(
					InputAction::kActivate,
					RE::INPUT_DEVICE::kGamepad,
					ButtonEventPressType::kPressAndHold,
					1.0f
				);
				a_p->pam->SendButtonEvent
				(
					InputAction::kActivate, 
					RE::INPUT_DEVICE::kGamepad, 
					ButtonEventPressType::kRelease, 
					1.0f
				);
			}
			else
			{
				// Reset to default package first.
				a_p->pam->SetAndEveluatePackage();
			}

			a_p->mm->wantsToMount = false;
			// Get off mount/stop interacting with furniture instantly.
			a_p->coopActor->StopInteractingQuick(true);
		}

		void Dodge(const std::shared_ptr<CoopPlayer>& a_p) 
		{
			// Currently, a speedmult-based dash dodge with I-frames has been implemented.
			// This general-use dodge is almost always triggerable 
			// when the player is grounded, swimming, or paragliding.
			// TODO: 
			// Dodge support for other popular dodge mods and an MCM option to choose 
			// either the mod's speedmult dodge or any installed custom dodge mod.
			// TKDodge is partially supported (I-frames do not trigger as of now).
			
			if (Settings::bUseDashDodgeSystem) 
			{
				// Reset to default package first.
				bool occupyingFurniture = 
				(
					Util::HandleIsValid(a_p->coopActor->GetOccupiedFurniture())
				);
				if (occupyingFurniture)
				{
					if (a_p->isPlayer1)
					{
						// Activate to exit furniture for P1.
						a_p->pam->SendButtonEvent
						(
							InputAction::kActivate,
							RE::INPUT_DEVICE::kGamepad, 
							ButtonEventPressType::kInstantTrigger,
							0.0f
						);
						a_p->pam->SendButtonEvent
						(
							InputAction::kActivate,
							RE::INPUT_DEVICE::kGamepad,
							ButtonEventPressType::kPressAndHold,
							1.0f
						);
						a_p->pam->SendButtonEvent
						(
							InputAction::kActivate, 
							RE::INPUT_DEVICE::kGamepad, 
							ButtonEventPressType::kRelease, 
							1.0f
						);
					}
					else
					{
						// Reset to default package to exit furniture for companion players.
						a_p->pam->SetAndEveluatePackage();
						// Get off mount/stop interacting with furniture instantly.
						a_p->coopActor->StopInteractingQuick(true);
					}
				}

				// Signal movement manager to dash dodge.
				a_p->mm->isRequestingDashDodge = true;
			}
			else if (ALYSLC::TKDodgeCompat::g_tkDodgeInstalled) 
			{
				// Stop sprinting before dodging.
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionSprintStop, a_p->coopActor.get()
				);
				auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
				// Targeting a world position or refr.
				bool hasTarget = 
				(
					(a_p->mm->reqFaceTarget) || 
					(
						crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
					)
				);
				const float& lsGameAngle = a_p->mm->movementOffsetParams[!MoveParams::kLSGameAng];
				// Facing to moving angle difference.
				float facingToMovingAngDiff = 0.0f;
				float angToTarget = Util::GetYawBetweenPositions
				(
					a_p->coopActor->data.location, a_p->tm->crosshairWorldPos
				);
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
			// If enabled, the player will continuously face the crosshair position.
			// Otherwise, the player rotates to face their movement direction as usual.
			a_p->mm->reqFaceTarget = !a_p->mm->reqFaceTarget;
			if (!Settings::vbAnimatedCrosshair[a_p->playerID]) 
			{
				return;
			}

			// Signal targeting manager to smoothly rotate the crosshair into the 'X' configuration 
			// to notify the player that they are facing the crosshair position now.
			a_p->tm->crosshairRotationData->SetTimeSinceUpdate(0.0f);
			a_p->tm->crosshairRotationData->ShiftEndpoints
			(
				a_p->mm->reqFaceTarget ? PI / 4.0f : 0.0f
			);
		}

		void Favorites(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the FavoritesMenu.

			bool succ = glob.moarm->InsertRequest
			(
				a_p->controllerID, 
				InputAction::kFavorites, 
				SteadyClock::now(), 
				RE::FavoritesMenu::MENU_NAME
			);
			if (!succ) 
			{
				return;
			}

			a_p->pam->SendButtonEvent
			(
				InputAction::kFavorites,
				RE::INPUT_DEVICE::kKeyboard, 
				ButtonEventPressType::kInstantTrigger
			);

			// FavoritesMenu is blocked from opening when P1 is a Werewolf -- 
			// (no input event sent or processed by MenuControls or InputEventDispatcher) --
			// which blocks other non-transformed players from accessing their favorites,
			// so attempt to open it directly via the message queue instead.

			// This player is a werewolf or P1 is not, so no need to use the message queue.
			if (Util::IsWerewolf(a_p->coopActor.get()) || 
				!Util::IsWerewolf(RE::PlayerCharacter::GetSingleton())) 
			{
				return;
			}

			auto msgQueue = RE::UIMessageQueue::GetSingleton(); 
			if (!msgQueue) 
			{
				return;
			}

			msgQueue->AddMessage
			(
				RE::FavoritesMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr
			);
		}

		void Inventory(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open P1's inventory if the requesting player is P1.
			// Otherwise, open the companion player's 'inventory', which is a ContainerMenu.
			
			bool succ = glob.moarm->InsertRequest
			(
				a_p->controllerID, 
				InputAction::kInventory, 
				SteadyClock::now(), 
				RE::ContainerMenu::MENU_NAME, 
				a_p->coopActor->GetHandle()
			);
			if (!succ) 
			{
				return;
			}

			if (a_p->isPlayer1)
			{
				a_p->pam->SendButtonEvent
				(
					InputAction::kInventory,
					RE::INPUT_DEVICE::kKeyboard,
					ButtonEventPressType::kInstantTrigger
				);
			}
			else
			{
				// Reset P1 damage multiplier so that the co-op player's inventory 
				// reports the correct damage for weapons, if perks are also imported.
				glob.player1Actor->SetActorValue(RE::ActorValue::kAttackDamageMult, 1.0f);
				a_p->coopActor->OpenContainer(!RE::ContainerMenu::ContainerMode::kNPCMode);
			}
		}

		void Jump(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Signal movement manager to start jump.
		
			// Stop sprinting before jump.
			Util::RunPlayerActionCommand
			(
				RE::DEFAULT_OBJECT::kActionSprintStop, a_p->coopActor.get()
			);
			// Reset to default package and stop any interaction idles.
			if (!a_p->isPlayer1 && !a_p->coopActor->IsOnMount())
			{
				a_p->pam->SetAndEveluatePackage();
				// Quickly exit any interaction.
				a_p->coopActor->StopInteractingQuick(true);
			}

			a_p->mm->reqStartJump = true;
		}

		void MagicMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the MagicMenu.
			
			HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kMagicMenu);
			// Magic menu is blocked from opening via player input
			// when P1 is transformed into a Werewolf or Vampire Lord,
			// but if this player is not transformed, 
			// open the Magic Menu via the message queue.

			// This player is transformed, or P1 is not, so no need to use the message queue.
			if (a_p->isTransformed || !glob.coopPlayers[glob.player1CID]->isTransformed)
			{
				return;
			}

			auto msgQueue = RE::UIMessageQueue::GetSingleton(); 
			if (!msgQueue)
			{
				return;
			}

			msgQueue->AddMessage
			(
				RE::MagicMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr
			);
		}

		void MapMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the MapMenu.
			HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kMapMenu);
			// Map menu is blocked from opening via player input 
			// when P1 is transformed into a Werewolf or Vampire Lord,
			// but if this player is not transformed, open the Map Menu via the message queue.

			// This player is transformed, or P1 is not, so no need to use the message queue.
			if (a_p->isTransformed || !glob.coopPlayers[glob.player1CID]->isTransformed)
			{
				return;
			}

			auto msgQueue = RE::UIMessageQueue::GetSingleton();
			if (!msgQueue)
			{
				return;
			}

			msgQueue->AddMessage(RE::MapMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		}

		void Pause(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the JournalMenu to pause the game.

			HelperFuncs::OpenMenuWithKeyboard(a_p, InputAction::kPause);
		}

		void PowerAttackDual(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Play dual-wield power attack animation.

			// Attempt to perform a power attack if the player's weapon is drawn 
			// and they are not in a killmove.
			// Otherwise, draw weapons/magic if arms rotation is disabled.
			if (a_p->coopActor->IsWeaponDrawn())
			{
				// Check if a killmove should be played first.
				bool performingKillmove = HelperFuncs::CheckForKillmove
				(
					a_p, InputAction::kAttackLH
				);
				if (!performingKillmove)
				{
					HelperFuncs::PlayPowerAttackAnimation(a_p, InputAction::kPowerAttackDual);
				}
			}
			else if (!Settings::bEnableArmsRotation)
			{
				// Unsheathe if not rotating arms.
				// Do nothing otherwise, since the player is rotating their arms.
				a_p->pam->ReadyWeapon(true);
			}
		}

		void PowerAttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Play LH power attack animation.
			
			// Attempt to perform power attack if the player's weapon is drawn 
			// and they are not in a killmove.
			// Otherwise, draw weapons/magic if arms rotation is disabled.
			if (a_p->coopActor->IsWeaponDrawn()) 
			{
				// Check if a killmove should be played first.
				bool performingKillmove = HelperFuncs::CheckForKillmove
				(
					a_p, InputAction::kAttackLH
				);
				if (!performingKillmove)
				{
					HelperFuncs::PlayPowerAttackAnimation(a_p, InputAction::kPowerAttackLH);
				}
			}
			else if (!Settings::bEnableArmsRotation)
			{
				// Unsheathe if not rotating arms.
				// Do nothing otherwise, since the player is rotating their arms.
				a_p->pam->ReadyWeapon(true);
			}
		}

		void PowerAttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Play RH power attack animation.
			
			// Attempt to perform power attack if the player's weapon is drawn 
			// and they are not in a killmove.
			// Otherwise, draw weapons/magic if arms rotation is disabled.
			if (a_p->coopActor->IsWeaponDrawn())
			{
				// Check if a killmove should be played first.
				bool performingKillmove = HelperFuncs::CheckForKillmove
				(
					a_p, InputAction::kAttackLH
				);
				if (!performingKillmove)
				{
					HelperFuncs::PlayPowerAttackAnimation(a_p, InputAction::kPowerAttackRH);
				}
			}
			else if (!Settings::bEnableArmsRotation)
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
			const auto qsBoundObj = 
			(
				qsItem && qsItem->IsBoundObject() ?
				qsItem->As<RE::TESBoundObject>() :
				nullptr
			);
			if (!qsBoundObj) 
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kGeneralNotification,
					fmt::format("P{}: No quick slot item equipped!", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection
					},
					0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
				);
				return;
			}
			
			// Allow usage of a QS item only if the player HAD at least 1 in their inventory,
			// as indicated by presence of an entry in the player's inventory counts.
			// Otherwise, clear out the slot and notify the player.
			const auto invCounts = a_p->coopActor->GetInventoryCounts();				
			const auto iter = invCounts.find(qsBoundObj);
			const int32_t count = iter != invCounts.end() ? iter->second : -1;
			if (count > 0)
			{
				// Has at least 1, so use the item via equip.
				auto aem = RE::ActorEquipManager::GetSingleton(); 
				if (!aem) 
				{
					return;
				}

				aem->EquipObject
				(
					a_p->coopActor.get(),
					qsItem->As<RE::TESBoundObject>(), 
					nullptr, 
					1,
					nullptr,
					false,
					true, 
					true, 
					true
				);
				// Notify the player of quick item use and how many remain.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: Using item '{}' ({} remaining)",
						a_p->playerID + 1, qsItem->GetName(), count - 1
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: Out of item '{}'",
						a_p->playerID + 1, qsItem->GetName()
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
				// Remove QS item since the player does not have the item anymore.
				a_p->em->quickSlotItem = nullptr;
				a_p->em->equippedQSItemIndex = -1;
				a_p->em->RefreshEquipState(RefreshSlots::kWeapMag);
			}
		}

		void ResetAim(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Signal the movement manager to reset this player's aim pitch and node rotations.
			// Also reset the grabbed refr XY distance offset.

			a_p->mm->reqResetAimAndBody = true;
			a_p->tm->grabbedRefrDistanceOffset = 0.0f;
		}

		void ResetCamOrientation(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset camera orientation by resetting radial distance and height offsets.

			// Give this player control of the camera.
			auto& controllingCID = glob.cam->controlCamCID;
			if (controllingCID != a_p->controllerID && 
				glob.cam->camAdjMode == CamAdjustmentMode::kNone)
			{
				controllingCID = a_p->controllerID;
			}

			// Same controller as the one with control over camera can adjust the cam state freely.
			// Nothing to do otherwise.
			if (controllingCID != a_p->controllerID)
			{
				return;
			}

			glob.cam->camRadialDistanceOffset = 
			glob.cam->camSavedRadialDistanceOffset =
			glob.cam->camBaseHeightOffset = 0.0f;
			// Inform the player of switch back to auto-trail mode.
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kCamera,
				fmt::format("P{}: Reset camera orientation", a_p->playerID + 1),
				{ 
					CrosshairMessageType::kNone,
					CrosshairMessageType::kStealthState, 
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void RotateCam(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Set cam adjustment mode to rotate if this player can obtain control of the camera.

			HelperFuncs::SetCameraAdjustmentMode(a_p->controllerID, InputAction::kRotateCam, true);
		}

		void Sheathe(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Sheathe if weapons/magic is out, draw otherwise.

			a_p->pam->ReadyWeapon(!a_p->coopActor->IsWeaponDrawn(), false);
		}

		void Sneak(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Toggle sneak state for the player.
			
			// Sneaking toggles levitation for Vampire Lords.
			// Enter sneak mode to levitate, 
			// which will also equip the default LH and RH vampiric spells.
			if (!a_p->isPlayer1 && Util::IsVampireLord(a_p->coopActor.get()))
			{
				// Set to true while toggling levitation state.
				// Don't queue another task if in the process of changing the levitation state.
				if (a_p->isTogglingLevitationStateTaskRunning) 
				{
					return;
				}

				a_p->taskRunner->AddTask([a_p]() { a_p->ToggleVampireLordLevitationTask(); });
			}
			else
			{
				// Toggle with action command.
				// Ensure package flags are in sync for co-op companions,
				// and also update the actor state to keep everything in sync.
				// Additional state sync corrections are made in the movement manager
				// to ensure all three states are equivalent.
				// Sneak state changes are still sometimes delayed
				// for companion players unfortunately.
				a_p->pam->wantsToSneak = !a_p->pam->wantsToSneak;
				if (a_p->pam->wantsToSneak)
				{
					// Ensure package flags are in sync for co-op companions.
					if (!a_p->isPlayer1)
					{
						a_p->pam->SetPackageFlag
						(
							RE::PACKAGE_DATA::GeneralFlag::kAlwaysSneak, true
						);
					}

					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionSneak, a_p->coopActor.get()
					);
					a_p->coopActor->actorState1.sneaking = 1;
					a_p->coopActor->actorState2.forceSneak = 1;
				}
				else
				{
					if (!a_p->isPlayer1)
					{
						a_p->pam->SetPackageFlag
						(
							RE::PACKAGE_DATA::GeneralFlag::kAlwaysSneak, false
						);
					}

					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionSneak, a_p->coopActor.get()
					);
					a_p->coopActor->actorState1.sneaking = 0;
					a_p->coopActor->actorState2.forceSneak = 0;
				}
				
				// TODO:
				// Properly toggle the widget's fade state to sync with P1's sneak state in co-op.
			}
		}

		void Sprint(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Start sprinting if not overencumbered, paragliding, or using M.A.R.F.
			// If sneaking with the silent roll perk, start rolling.
			// If blocking, start shield charging.
			// Otherwise, start sprint animation.
			// And finally, if paragliding or using M.A.R.F, trigger gale spell.

			// First check is for overencumbrance.
			if (HelperFuncs::ActionJustStarted(a_p, InputAction::kSprint))
			{
				a_p->mm->UpdateEncumbranceFactor();
			}

			bool overencumbered = 
			(
				(
					!Settings::bInfiniteCarryweight &&
					!a_p->isInGodMode
				) &&
				(
					a_p->mm->encumbranceFactor >= 1.0f ||
					a_p->coopActor->IsOverEncumbered()
				)
			);

			// Inform the player that they are overencumbered before returning.
			if (overencumbered)
			{
				float inventoryWeight = a_p->coopActor->GetWeightInContainer();
				const auto invChanges = a_p->coopActor->GetInventoryChanges();
				if (invChanges)
				{
					inventoryWeight = invChanges->totalWeight;
				}

				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kGeneralNotification,
					fmt::format
					(
						"P{}: <font color=\"#FF0000\">Over-encumbered! ({:.0f} / {:.0f})</font>", 
						a_p->playerID + 1,
						inventoryWeight,
						max(1.0f, a_p->coopActor->GetActorValue(RE::ActorValue::kCarryWeight))
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);

				return;
			}

			if (!a_p->mm->isParagliding && !a_p->tm->isMARFing) 
			{
				if (a_p->coopActor->IsSneaking() || a_p->pam->IsPerforming(InputAction::kBlock)) 
				{
					// Trigger silent roll or shield charge animation via action command.
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionSprintStart, a_p->coopActor.get()
					);
				}
				else
				{
					// Stop attacking before sprinting/dodging.
					a_p->coopActor->NotifyAnimationGraph("attackStop");
					// NOTE:
					// Sprint start action fails to modify P1's mount speedmult.
					// Since the sprint start/stop actions do not function
					// for P1's mount while motion driven, 
					// we trigger the mounted player actor sprint animation (hunched over)
					// and modify the mount's speed separately in the Update() hook.
					// Running the action command fails sometimes, 
					// so we directly notify the animation graph instead.
					a_p->coopActor->NotifyAnimationGraph("SprintStart");
				}

				// Sprint stamina expenditure starts now.
				a_p->expendSprintStaminaTP = SteadyClock::now();
			}
			else if (glob.tarhielsGaleSpell && glob.player1Actor->HasSpell(glob.tarhielsGaleSpell))
			{
				// Conveniently cast gale spell (if known by P1) when paragliding/M.A.R.F-ing.
				auto instantCaster = a_p->coopActor->GetMagicCaster
				(
					RE::MagicSystem::CastingSource::kInstant
				); 
				if (instantCaster) 
				{
					instantCaster->CastSpellImmediate
					(
						glob.tarhielsGaleSpell, 
						false,
						a_p->coopActor.get(), 
						1.0f, 
						false,
						0.0f, 
						a_p->coopActor.get()
					);
				}
			}
		}

		void SprintPowerAttackLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Power attack with LH object/fists while sprinting.

			// Doesn't trigger consistently unless already performing a LH attack.
			Util::RunPlayerActionCommand
			(
				RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
			);
			Util::RunPlayerActionCommand
			(
				RE::DEFAULT_OBJECT::kActionLeftPowerAttack, a_p->coopActor.get()
			);
		}

		void SprintPowerAttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Power attack with RH object/fists while sprinting.

			// Same reasoning as with LH sprint power attack above.
			Util::RunPlayerActionCommand
			(
				RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get()
			);
			Util::RunPlayerActionCommand
			(
				RE::DEFAULT_OBJECT::kActionRightPowerAttack, a_p->coopActor.get()
			);
		}

		void StatsMenu(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Open the StatsMenu if playing Skyrim.
			// Otherwise, open the Hero Menu or a UIExtensions stats menu if playing Enderal.

			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
			{
				// Check if Maxsu2017's 'Hero Menu Enhanced' mod is installed, 
				// and trigger the StatsMenu as usual to open the enhanced Hero Menu.
				bool enhancedHeroMenuAvailable = static_cast<bool>
				(
					GetModuleHandleA("EnderalHeroMenu.dll")
				); 
				if (enhancedHeroMenuAvailable) 
				{
					auto ue = RE::UserEvents::GetSingleton();
					auto controlMap = RE::ControlMap::GetSingleton();
					if (!ue || !controlMap)
					{
						return;
					}

					auto keyCode = controlMap->GetMappedKey
					(
						glob.paInfoHolder->ACTIONS_TO_P1_UE_STRINGS.at(!InputAction::kStatsMenu), 
						RE::INPUT_DEVICE::kKeyboard
					);
					if (keyCode != 0xFF)
					{
						// Stats Menu-opening bind found, insert request.
						bool succ = glob.moarm->InsertRequest
						(
							a_p->controllerID,
							InputAction::kStatsMenu, 
							SteadyClock::now(), 
							GlobalCoopData::ENHANCED_HERO_MENU
						);
						// If request was successfully inserted, open the requested menu.
						if (succ)
						{
							a_p->pam->SendButtonEvent
							(
								InputAction::kStatsMenu, 
								RE::INPUT_DEVICE::kKeyboard,
								ButtonEventPressType::kInstantTrigger
							);
						}

						return;
					}
					else
					{
						// Quick Stats not bound -> open UIExtensions stats menu instead.
						glob.onCoopHelperMenuRequest.SendEvent
						(
							a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kStats
						);
					}
				}
				else
				{
					// NOTE:
					// Commented out for now. 
					// Potential support for vanilla Hero Menu in the future.
					// Doesn't consistently open the vanilla Enderal Hero Menu at the moment.
					/*
					if (a_p->isPlayer1)
					{
						// Attempt to find the hotkey for opening the Hero menu
						// by reading an Enderal MCM script property,
						// and then open it using the hotkey.
						auto skyrimVM = RE::SkyrimVM::GetSingleton(); 
						if (skyrimVM->impl)
						{
							const auto& vm = skyrimVM->impl;
							if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
							{
								auto levelSystemQuest = RE::TESForm::LookupByID<RE::TESQuest>
								(
									0x10AA2
								);
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
										auto handle = 
										(
											vm->GetObjectHandlePolicy()->GetHandleForObject
											(
												playerAlias->GetVMTypeID(), playerAlias
											)
										);
										RE::BSTSmartPointer<RE::BSScript::Object> objectPtr;
										vm->FindBoundObject
										(
											handle, "_00E_Game_SkillmenuSC", objectPtr
										);
										if (objectPtr)
										{
											auto heroMenuDXSCVar = objectPtr->GetVariable
											(
												"iHeroMenuKeycode"
											);
											if (heroMenuDXSCVar)
											{
												uint32_t heroMenuDXSC = heroMenuDXSCVar->GetUInt();
												Util::SendButtonEvent
												(
													RE::INPUT_DEVICE::kKeyboard, 
													"HeroMenu"sv, 
													heroMenuDXSC, 
													1.0f,
													0.0f, 
													true,
													false
												);
												Util::SendButtonEvent
												(
													RE::INPUT_DEVICE::kKeyboard,
													"HeroMenu"sv, 
													heroMenuDXSC,
													0.0f, 
													1.0f,
													true,
													false
												);
											}
										}
									}
									
								}
							}
						}
					}
					else
					{
						glob.onCoopHelperMenuRequest.SendEvent
						(
							a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kStats
						);
					}
					*/

					// Signal script to open UIExtensions stats menu.
					glob.onCoopHelperMenuRequest.SendEvent
					(
						a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kStats
					);
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
			
			// Only need to send menu-open request if there are more than two players,
			// since the player would have to select which one of the other players to teleport to.
			bool noPlayerInMenus = Util::MenusOnlyAlwaysOpen();
			if (glob.activePlayers > 2) 
			{
				// Open helper menu if no player is controlling menus.
				if (noPlayerInMenus)
				{
					glob.onCoopHelperMenuRequest.SendEvent
					(
						a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kTeleport
					);
				}
				else
				{
					// Teleport to the player that this player is facing 
					// if another player is controlling menus.
					// (can't open player selection menu).
					RE::Actor* facingPlayer = nullptr;
					float minFacingDiff = FLT_MAX;
					for (const auto& otherP : glob.coopPlayers) 
					{
						if (!otherP->isActive || otherP->coopActor == a_p->coopActor) 
						{
							continue;
						}

						float facingDiff = fabsf
						(
							Util::NormalizeAngToPi
							(
								Util::GetYawBetweenRefs
								(
									a_p->coopActor.get(), otherP->coopActor.get()
								) - 
								a_p->coopActor->GetHeading(false)
							)
						);
						if (facingDiff < minFacingDiff) 
						{
							facingPlayer = otherP->coopActor.get();
							minFacingDiff = facingDiff;
						}
					}

					// No player found. Huh.
					if (!facingPlayer) 
					{
						return;
					}

					a_p->taskRunner->AddTask
					(
						[facingPlayer, a_p]() { a_p->TeleportTask(facingPlayer->GetHandle()); }
					);
				}
			}
			else
			{
				// Only two active players.
				// Get the other active player to teleport to.
				RE::Actor* otherPlayer = nullptr;
				for (const auto& otherP : glob.coopPlayers)
				{
					if (otherP->isActive && otherP->coopActor != a_p->coopActor)
					{
						otherPlayer = otherP->coopActor.get();
						break;
					}
				}

				// No other player? Huh.
				if (!otherPlayer)
				{
					return;
				}

				a_p->taskRunner->AddTask
				(
					[otherPlayer, a_p]() { a_p->TeleportTask(otherPlayer->GetHandle()); }
				);
			}
		}

		void TradeWithPlayer(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// If there are more than 2 active players, 
			// the script will open the UIExtensions character selection menu,
			// allowing this player to choose a player to trade with.
			// Otherwise, it will directly open the GiftMenu 
			// with the only other player to commence trading.

			glob.onCoopHelperMenuRequest.SendEvent
			(
				a_p->coopActor.get(), a_p->controllerID, !HelperMenu::kTrade
			);
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
			
			// NOTE:
			// Once the player requests to paraglide, activation of objects is disabled 
			// until the player presses activate again on the ground.
			// Prevents activation of objects while paragliding or when landing after paragliding.
			if (a_p->pam->requestedToParaglide)
			{
				return;
			}

			if (a_p->pam->downedPlayerTarget)
			{
				// NOTE: 
				// Have to stop revive idle and draw weapons/magic to prevent bugged
				// animation state where weapons are in hand but unusable.
				if (!a_p->coopActor->IsOnMount() &&
					!a_p->coopActor->IsSwimming() && 
					!a_p->coopActor->IsFlying())
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
				Util::StopEffectShader
				(
					a_p->pam->downedPlayerTarget->coopActor.get(), 
					glob.dragonSoulAbsorbShader, 
					2.0f
				);
				Util::StopHitArt
				(
					a_p->pam->downedPlayerTarget->coopActor.get(),
					glob.reviveDragonSoulEffect, 
					2.0f
				);
				Util::StopHitArt
				(
					a_p->pam->downedPlayerTarget->coopActor.get(), glob.reviveHealingEffect, 2.0f
				);

				// Set being revived to false, and then clear the downed player target. 
				// Done to allow for a new downed target when the action is restarted.
				a_p->pam->downedPlayerTarget->isBeingRevived = false;
				a_p->pam->downedPlayerTarget = nullptr;
				// No longer reviving.
				a_p->isRevivingPlayer = false;
			}
			else 
			{
				// Only attempt activation on input release, not if 'Activate' was interrupted.
				const auto& perfStage = 
				(
					a_p->pam->paStatesList
					[!InputAction::kActivate - !InputAction::kFirstAction].perfStage
				);
				if (perfStage != PerfStage::kInputsReleased && 
					perfStage != PerfStage::kSomeInputsReleased) 
				{
					return;
				}

				auto p1 = RE::PlayerCharacter::GetSingleton();
				auto activationRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->activationRefrHandle);
				auto activationRefrValidity = 
				(
					activationRefrPtr && Util::IsValidRefrForTargeting(activationRefrPtr.get())
				);
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
					RE::PlaySound("UIActivateFail");
					return;
				}

				// Clear grabbed/released refr if activating it, 
				// as its 3D will be removed once picked up
				// and will no longer need to be tracked in the targeting manager.
				if (a_p->tm->rmm->IsManaged(a_p->tm->activationRefrHandle, true) ||
					a_p->tm->rmm->IsManaged(a_p->tm->activationRefrHandle, false))
				{
					a_p->tm->rmm->ClearRefr(a_p->tm->activationRefrHandle);
				}

				// Get count.
				uint32_t count = 1;
				auto extraCount = activationRefrPtr->extraList.GetByType<RE::ExtraCount>();
				if (extraCount)
				{
					count = extraCount->count;
				}

				// Get base object to get refr type and for use with the activate refr call.
				// Cannot activate if the refr has no base object.
				auto baseObj = activationRefrPtr->GetBaseObject();
				if (!baseObj)
				{
					return;
				}

				// Special handling first:
				// 1. Stop combat with a crosshair-selected, normally neutral or friendly actor 
				// that has no bounty on the player or is fleeing. -OR-
				// 2. Start arrest dialogue with the crosshair-selected guard
				// if the guard is hostile and the player has accrued a bounty
				// with the guard's crime faction.
				auto asActor = activationRefrPtr->As<RE::Actor>();
				bool crosshairTargetedHostileActor = 
				(
					(
						asActor && 
						asActor->GetHandle() == a_p->tm->crosshairRefrHandle
					) &&
					(
						asActor->IsHostileToActor(a_p->coopActor.get()) ||
						asActor->IsHostileToActor(p1)
					)
				);
				bool startArrestDialogue = 
				(
					crosshairTargetedHostileActor &&
					!asActor->IsDead() &&
					Util::IsGuard(asActor) &&
					Util::HasBountyOnPlayer(asActor) &&
					!a_p->coopActor->IsSneaking()
				);
				bool stopCombatWithNonHostiles = 
				(
					(
						!startArrestDialogue &&
						crosshairTargetedHostileActor &&
						!asActor->IsDead() &&
						!a_p->coopActor->IsSneaking()
					) &&
					(
						Util::HasNoBountyButInCrimeFaction(asActor) || 
						Util::IsFleeing(asActor) ||
						asActor->IsAMount() ||
						Util::IsPartyFriendlyActor(asActor)
					)
				);
				if (startArrestDialogue)
				{
					if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
					{
						// Tried the following topic, but it's probably not the right one.
						// For reference:
						/*RE::TESForm::LookupByEditorID<RE::TESTopic>
						(
							"GuardDialogueCrimeForcegreetTopic"
						)*/
						
						// Do not specify topic to trigger generic 'Hey, I know you!'
						// arrest dialogue for Enderal.
						// Only works for some guards, though.
						bool succ = glob.moarm->InsertRequest
						(
							a_p->controllerID, 
							InputAction::kActivate, 
							SteadyClock::now(), 
							RE::DialogueMenu::MENU_NAME,
							asActor->GetHandle()
						);
						if (succ)
						{
							asActor->SetDialogueWithPlayer
							(
								true, true, nullptr
							);
							auto procLists = RE::ProcessLists::GetSingleton();
							if (procLists)
							{
								// Prevent other aggroed actors from ganging up on players
								// while the Dialogue Menu is open.
								procLists->StopCombatAndAlarmOnActor(p1, false);
								if (!a_p->isPlayer1)
								{
									procLists->StopCombatAndAlarmOnActor
									(
										a_p->coopActor.get(), false
									);
								}
							}

							return;
						}
					}
					else
					{
						auto topic = 
						(
							RE::TESForm::LookupByEditorID<RE::TESTopic>
							(
								"DGCrimeForcegreetTopic"
							)
						);
						if (topic)
						{
							bool succ = glob.moarm->InsertRequest
							(
								a_p->controllerID, 
								InputAction::kActivate, 
								SteadyClock::now(), 
								RE::DialogueMenu::MENU_NAME,
								asActor->GetHandle()
							);
							// 'You have committed crimes against Skyrim and her people' line
							// at index 4 for default Skyrim guard forcegreet topic.
							if (succ && topic->numTopicInfos >= 5)
							{
								asActor->SetDialogueWithPlayer
								(
									true, true, topic->topicInfos[4]
								);
								auto procLists = RE::ProcessLists::GetSingleton();
								if (procLists)
								{
									// Prevent other aggroed actors from ganging up on players
									// while the Dialogue Menu is open.
									procLists->StopCombatAndAlarmOnActor(p1, false);
									if (!a_p->isPlayer1)
									{
										procLists->StopCombatAndAlarmOnActor
										(
											a_p->coopActor.get(), false
										);
									}
								}

								return;
							}
						}
					}
				}
				else if (stopCombatWithNonHostiles)
				{
					a_p->pam->StopCombatWithFriendlyActors();
					return;
				}
				
				// Check if another player is controlling menus.
				// This influences what objects this player can activate.
				// Cannot activate anything that could potentially open a menu 
				// if another player is controlling menus.
				bool menusOnlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
				bool anotherPlayerControllingMenus = !GlobalCoopData::CanControlMenus
				(
					a_p->controllerID
				);

				bool isBook = baseObj->IsBook();
				bool isNote = baseObj->IsNote();
				bool lootable = Util::IsLootableRefr(activationRefrPtr.get());
				// Has extra data with an associated quest.
				// Might not necessarily be a quest item, but better safe than sorry.
				bool isQuestItem = 
				{
					(lootable) && 
					(
						activationRefrPtr->extraList.HasType
						(
							RE::ExtraDataType::kAliasInstanceArray
						) ||
						activationRefrPtr->extraList.HasType(RE::ExtraDataType::kFromAlias)
					)
				};
				// Pick up notes and books if they are activated 
				// while not selected by the crosshair.
				// Also, can still loot refrs when another player is controlling menus.
				bool shouldPickup = 
				(
					(
						(menusOnlyAlwaysOpen) &&
						(a_p->tm->activationRefrHandle != a_p->tm->crosshairRefrHandle) &&
						(!isQuestItem || a_p->isPlayer1) && 
						(isBook || isNote)
					) ||
					(
						(!menusOnlyAlwaysOpen) &&
						(!isQuestItem || a_p->isPlayer1) && 
						(anotherPlayerControllingMenus && lootable)
					)
				);
				if (a_p->isPlayer1)
				{
					if (shouldPickup)
					{
						a_p->coopActor->PickUpObject(activationRefrPtr.get(), count);
					}
					else if (menusOnlyAlwaysOpen)
					{
						// No player is controlling menus.
						if (asActor && asActor->IsAMount() && !asActor->IsDead())
						{
							// Ignore if already attempting to mount or mounted.
							if (!a_p->mm->isMounting && !a_p->coopActor->IsOnMount())
							{
								// Attempt to mount.
								a_p->SetTargetedMount(asActor->GetHandle());
								glob.moarm->InsertRequest
								(
									a_p->controllerID, 
									InputAction::kActivate,
									SteadyClock::now(), 
									"", 
									a_p->tm->activationRefrHandle
								);
								a_p->taskRunner->AddTask([a_p](){ a_p->MountTask(); });
							}
						}
						else
						{
							// Activate the refr after sending an activation request 
							// to give P1 control if any menus triggered by activation open later.
							// Toggle AI driven off while activating.
							Util::SetPlayerAIDriven(false);
							glob.moarm->InsertRequest
							(
								a_p->controllerID, 
								InputAction::kActivate, 
								SteadyClock::now(),
								"", 
								a_p->tm->activationRefrHandle
							);
							Util::ActivateRefr
							(
								activationRefrPtr.get(), p1, 0, baseObj, count, false
							);
							Util::SetPlayerAIDriven(true);
						}
					}
				}
				else
				{
					// Have P1 activate shared items or refrs that can trigger menus.
					bool p1Activate = false;
					// A crime to activate.
					bool offLimits = activationRefrPtr->IsCrimeToActivate();
					RE::Actor* asActor = activationRefrPtr->As<RE::Actor>();
					// Trying to pickpocket an actor.
					bool attemptingToPickpocket = 
					(
						asActor && asActor->CanPickpocket() && a_p->coopActor->IsSneaking()
					);
					bool isActivator = baseObj->Is
					(
						RE::FormType::Activator, RE::FormType::TalkingActivator
					);
					bool isBed = 
					(
						baseObj->Is(RE::FormType::Furniture) && 
						baseObj->As<RE::TESFurniture>()->furnFlags.all
						(
							RE::TESFurniture::ActiveMarker::kCanSleep
						)
					);
					bool isPartyWideItem = Util::IsPartyWideItem(baseObj);
					// Unread skill/spell books are read right away by P1 here, 
					// since, once read, all players' skills increase 
					// or the learned spell is usable by all players.
					// Other P1-activated items:
					// Gold, lockpicks, all regular books, notes, keys, activators,
					// workbenches, non-lootable items that aren't furniture, doors, 
					// locked objects, dead objects, containers, and non-mount actors.
					bool isSkillBook = 
					(
						isBook && baseObj->As<RE::TESObjectBOOK>()->TeachesSkill()
					);
					bool isSpellBook = 
					(
						isBook && baseObj->As<RE::TESObjectBOOK>()->TeachesSpell()
					);
					bool isUnreadSkillbook = 
					(
						isSkillBook && !baseObj->As<RE::TESObjectBOOK>()->IsRead()
					);
					bool isUnreadSpellbook = 
					(
						isSpellBook && !baseObj->As<RE::TESObjectBOOK>()->IsRead()
					);
					bool isWorkBenchFurniture = 
					(
						baseObj->Is(RE::FormType::Furniture) && 
						baseObj->As<RE::TESFurniture>()->workBenchData.benchType.get() != 
						RE::TESFurniture::WorkBenchData::BenchType::kNone
					);
					bool requiresP1ToOpenMenu = isBed || isWorkBenchFurniture;
					p1Activate = 
					(
						(
							isActivator || 
							isBed ||
							isNote ||
							isPartyWideItem || 
							isQuestItem || 
							isWorkBenchFurniture || 
							isUnreadSkillbook || 
							isUnreadSpellbook
						) || 
						(isBook && !isSkillBook && !isSpellBook) ||
						(
							(
								!lootable && 
								baseObj->IsNot(RE::FormType::Furniture)
							) &&
							(
								(
									baseObj->Is(RE::FormType::Door) ||
									activationRefrPtr->IsLocked() ||
									activationRefrPtr->IsDead()
								) ||
								(!asActor && activationRefrPtr->HasContainer()) ||
								(asActor && !asActor->IsAMount())
							)
						) 
					);
					
					// Is this player running the special interaction package
					// to interact with targeted furniture?
					// Start furniture interaction, if needed.
					bool inInteractionPackage = HelperFuncs::InitiateSpecialInteractionPackage
					(
						a_p, activationRefrPtr.get(), baseObj
					);

					if (asActor && asActor->IsAMount() && !asActor->IsDead())
					{
						// Ignore if already attempting to mount or mounted.
						if (!a_p->mm->isMounting && !a_p->coopActor->IsOnMount())
						{
							// Attempt to mount.
							a_p->SetTargetedMount(asActor->GetHandle());
							glob.moarm->InsertRequest
							(
								a_p->controllerID, 
								InputAction::kActivate,
								SteadyClock::now(),
								"",
								asActor->GetHandle()
							);
							a_p->taskRunner->AddTask([a_p](){ a_p->MountTask(); });
						}
					}
					else
					{
						// WOOO
						SPDLOG_DEBUG
						(
							"[PAFH] {} is activating {} ({}, form type 0x{:X}, "
							"base form type: 0x{:X}, count: {}). "
							"Is activator: {}, is bed: {}, is party wide item: {}, "
							"is quest item: {}, "
							"is workbench furniture: {}, is unread skill or spellbook: {}, "
							"is regular book: {}, not lootable and not furniture: {}, "
							"is door: {}, is locked: {}, is dead: {}, no container: {}, "
							"non mount actor: {}. Furn flags: 0b{:B}.",
							a_p->coopActor->GetName(), 
							activationRefrPtr->GetName(),
							p1Activate ? "via P1" : "via self",
							activationRefrPtr->GetFormType(),
							*baseObj->formType,
							extraCount ? extraCount->count : -1,
							isActivator,
							isBed,
							isPartyWideItem,
							isQuestItem,
							isWorkBenchFurniture,
							isUnreadSkillbook || isUnreadSpellbook,
							isBook && !isSkillBook && !isSpellBook,
							!lootable && baseObj->IsNot(RE::FormType::Furniture),
							baseObj->Is(RE::FormType::Door),
							activationRefrPtr->IsLocked(),
							activationRefrPtr->IsDead(),
							!asActor && activationRefrPtr->HasContainer(),
							asActor && !asActor->IsAMount(),
							baseObj->Is(RE::FormType::Furniture) ? 
							*baseObj->As<RE::TESFurniture>()->furnFlags :
							RE::TESFurniture::ActiveMarker::kNone
						);

						// Will get caught stealing when activating while fully detected. 
						// Alert nearby actors of P1 and trigger steal alarm.
						if ((a_p->tm->detectionPct == 100.0f) && 
							(offLimits || attemptingToPickpocket))
						{
							p1->currentProcess->SetActorsDetectionEvent
							(
								p1, p1->data.location, 1, p1
							);
							p1->currentProcess->high->detectAlert = true;
							p1->StealAlarm
							(
								activationRefrPtr.get(),
								baseObj, 
								count,
								count, 
								activationRefrPtr->GetOwner(),
								true
							);
						}

						if (shouldPickup)
						{
							// Pick up the object to loot it.
							a_p->coopActor->PickUpObject(activationRefrPtr.get(), count);
						}
						else if (p1Activate)
						{
							// Can only activate with P1 if no player is controlling menus.
							if (!menusOnlyAlwaysOpen)
							{
								return;
							}

							// Trick the game into thinking that P1 is sneaking during activation 
							// to trigger the pickpocketing menu when targeting NPCs.
							// Match P1 and this players' sneak states briefly 
							// before and during activation.
							bool toggleP1SneakOn = 
							(
								a_p->coopActor->IsSneaking() && !p1->IsSneaking()
							);
							bool toggleP1SneakOff = 
							(
								!a_p->coopActor->IsSneaking() && p1->IsSneaking()
							);

							if (toggleP1SneakOn)
							{
								p1->actorState1.sneaking = 1;
							}
							else if (toggleP1SneakOff)
							{
								p1->actorState1.sneaking = 0;
							}

							// Prevent Crafting/Smithing or other only-P1-triggerable menus 
							// from opening if P1 is too far away.
							bool p1TooFarAway = 
							(
								p1->data.location.GetDistance(a_p->coopActor->data.location) >
								a_p->tm->GetMaxActivationDist()
							);
							if (inInteractionPackage && requiresP1ToOpenMenu && p1TooFarAway)
							{
								a_p->tm->SetCrosshairMessageRequest
								(
									CrosshairMessageType::kGeneralNotification,
									fmt::format
									(
										"P{}: P1 is too far away to interact with {}.",
										a_p->playerID + 1,
										activationRefrPtr->GetName()
									),
									{ 
										CrosshairMessageType::kNone, 
										CrosshairMessageType::kStealthState, 
										CrosshairMessageType::kTargetSelection 
									},
									Settings::fSecsBetweenDiffCrosshairMsgs
								);
							}
							else
							{
								// Set P1 as motion driven before activating.
								Util::SetPlayerAIDriven(false);
								// Activate the refr after sending an activation request
								// to give this player control if any menus triggered by activation
								// open afterward.
								glob.moarm->InsertRequest
								(
									a_p->controllerID, 
									InputAction::kActivate, 
									SteadyClock::now(),
									"", 
									activationRefrPtr->GetHandle()
								);
								Util::ActivateRefr
								(
									activationRefrPtr.get(), p1, 0, baseObj, count, false
								);
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
						else if (!inInteractionPackage)
						{
							// If there is an accompanying interaction animation,
							// do not activate here, as the activation will not occur in sync 
							// with the interaction animation.
							// 
							// Activate the refr after sending an activation request 
							// to give this player control if any menus triggered by activation
							// open afterward.
							glob.moarm->InsertRequest
							(
								a_p->controllerID, 
								InputAction::kActivate, 
								SteadyClock::now(), 
								"", 
								a_p->tm->activationRefrHandle
							);
							// Show in TrueHUD recent loot widget.
							if (lootable && ALYSLC::TrueHUDCompat::g_trueHUDInstalled)
							{
								p1->AddObjectToContainer(baseObj, nullptr, count, p1);
								p1->RemoveItem
								(
									baseObj, 
									count,
									RE::ITEM_REMOVE_REASON::kRemove,
									nullptr,
									nullptr
								);
							}

							Util::ActivateRefr
							(
								activationRefrPtr.get(), 
								a_p->coopActor.get(), 
								0,
								baseObj,
								count,
								false
							);
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
				if (!Settings::bEnableArmsRotation) 
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
			if (performingKillmove) 
			{
				return;
			}

			if (a_p->pam->isSprinting) 
			{
				// Perform LH sprint attack.
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
				);
			}
			else if (a_p->em->Has2HRangedWeapEquipped())
			{
				auto attackState = a_p->coopActor->actorState1.meleeAttackState;
				// Bash with ranged weapon.
				if (attackState == RE::ATTACK_STATE_ENUM::kNone)
				{
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
					);
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get()
					);
				}

				// Performing LH release or attack stop when a crossbow-type weapon 
				// is in the process of firing chambers the next bolt
				// and forgoes the reload animation, 
				// which allows for a much faster firerate than intended.
				// Only cancel firing of bolt if not reloading.
				// Un-nock arrow if the bow is drawn.
				if (attackState == RE::ATTACK_STATE_ENUM::kBowDraw || 
					attackState == RE::ATTACK_STATE_ENUM::kBowDrawn)
				{
					a_p->coopActor->NotifyAnimationGraph("attackStop");
				}
			}
			else if (a_p->em->HasLHStaffEquipped())
			{
				a_p->pam->CastStaffSpell(a_p->em->GetLHWeapon(), true, false);
				if (a_p->pam->usingLHStaff)
				{
					a_p->pam->usingLHStaff->value = 0.0f;
				}
				// TODO: 
				// Manually update charge for P1.
				// Currently gets overidden by the game once the inventory menu is opened,
				// and not all staves have their charges updated by the game when used.
			}
			else
			{
				// Generic attack and release.
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
				);
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get()
				);
			}
		}

		void AttackRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Attack cleanup for RH weapon/fists.

			if (!a_p->coopActor->IsWeaponDrawn())
			{
				// Unsheathe if not rotating arms.
				// Do nothing otherwise.
				if (!Settings::bEnableArmsRotation)
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
			if (performingKillmove)
			{
				return;
			}

			if (a_p->pam->isSprinting)
			{
				// Perform RH sprint attack.
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get()
				);
			}
			else if (a_p->em->Has2HRangedWeapEquipped())
			{
				// Bows/crossbows require toggling AI driven 
				// and only sending button events on press and release.
				if (a_p->isPlayer1) 
				{
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionRightRelease, a_p->coopActor.get()
					);
				}
				else
				{
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionRightRelease, a_p->coopActor.get()
					);
				}
			}
			else if (a_p->em->HasRHStaffEquipped())
			{
				a_p->pam->CastStaffSpell(a_p->em->GetRHWeapon(), false, false);
				if (a_p->pam->usingRHStaff)
				{
					a_p->pam->usingRHStaff->value = 0.0f;
				}
				// TODO: Manually update charge for P1.
				// Currently gets overidden by game once the inventory menu is opened,
				// and not all staves have their charges updated by the game when used.
			}
			else
			{
				// Generic attack and release.
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get()
				);
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionRightRelease, a_p->coopActor.get()
				);
			}
		}

		void Bash(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Bash with the equipped LH weapon/shield.

			// Only bash if a power bash was not performed already while the bind was held.
			bool canBashOnRelease = 
			(
				(!a_p->pam->isAttacking) && 
				(
					a_p->pam->GetSecondsSinceLastInputStateChange(InputAction::kBash, true) <= 
					Settings::fSecsDefMinHoldTime ||
					!a_p->coopActor->HasPerk(glob.powerBashPerk)
				)
			);
			if (!canBashOnRelease)
			{
				return;
			}

			// Check if a killmove should be played first.
			bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kBash);
			if (performingKillmove)
			{
				return;
			}

			if (a_p->em->Has2HRangedWeapEquipped())
			{
				// Bash only if not on some stage of an attack.
				auto attackState = a_p->coopActor->actorState1.meleeAttackState;
				if (attackState == RE::ATTACK_STATE_ENUM::kNone)
				{
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
					);
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get()
					);
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
					// Emulate when happens when the player bashes 
					// with the default Skyrim controls:
					// Hold 'Block' + press 'Right Attack' + release 'Block'.

					// Start blocking.
					a_p->pam->QueueP1ButtonEvent
					(
						InputAction::kBlock, 
						RE::INPUT_DEVICE::kGamepad, 
						ButtonEventPressType::kInstantTrigger
					);
					a_p->pam->QueueP1ButtonEvent
					(
						InputAction::kBlock,
						RE::INPUT_DEVICE::kGamepad,
						ButtonEventPressType::kPressAndHold,
						0.1f, 
						true
					);
					// Start right attack.
					a_p->pam->QueueP1ButtonEvent
					(
						InputAction::kAttackRH,
						RE::INPUT_DEVICE::kGamepad,
						ButtonEventPressType::kInstantTrigger
					);
					a_p->pam->QueueP1ButtonEvent
					(
						InputAction::kAttackRH,
						RE::INPUT_DEVICE::kGamepad, 
						ButtonEventPressType::kPressAndHold,
						0.1f,
						true
					);
					// Release right attack.
					a_p->pam->QueueP1ButtonEvent
					(
						InputAction::kAttackRH,
						RE::INPUT_DEVICE::kGamepad, 
						ButtonEventPressType::kRelease,
						0.1f, 
						true
					);
					// Release block.
					a_p->pam->QueueP1ButtonEvent
					(
						InputAction::kBlock, 
						RE::INPUT_DEVICE::kGamepad, 
						ButtonEventPressType::kRelease, 
						0.1f, 
						true
					);

					// Send straight away in one chain.
					a_p->pam->ChainAndSendP1ButtonEvents();
					// Ensure P1 is AI driven again after.
					a_p->pam->sendingP1MotionDrivenEvents = false;
					Util::SetPlayerAIDriven(true);
					// Redundancy since block start/stop requests fail at times.
					if (wasBlocking)
					{
						Util::RunPlayerActionCommand
						(
							RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
						);
						a_p->coopActor->NotifyAnimationGraph("blockStart");
					}
					else
					{
						Util::RunPlayerActionCommand
						(
							RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get()
						);
						a_p->coopActor->NotifyAnimationGraph("attackStop");
						a_p->coopActor->NotifyAnimationGraph("blockStop");
					}
				}
				else
				{
					// Triggered via action command for companion players.
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
					);
					Util::RunPlayerActionCommand
					(
						RE::DEFAULT_OBJECT::kActionRightAttack, a_p->coopActor.get()
					);
					// Redundancy since block start/stop requests fail at times.
					if (wasBlocking)
					{
						Util::RunPlayerActionCommand
						(
							RE::DEFAULT_OBJECT::kActionLeftAttack, a_p->coopActor.get()
						);
						a_p->coopActor->NotifyAnimationGraph("blockStart");
					}
					else
					{
						Util::RunPlayerActionCommand
						(
							RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get()
						);
						a_p->coopActor->NotifyAnimationGraph("attackStop");
						a_p->coopActor->NotifyAnimationGraph("blockStop");
					}
				}
			}
		}

		void Block(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Stop blocking.

			if (a_p->isPlayer1)
			{
				// For Valhalla Combat projectile deflection compatibility,
				// which requires the block input event.
				a_p->pam->QueueP1ButtonEvent
				(
					InputAction::kBlock,
					RE::INPUT_DEVICE::kGamepad, 
					ButtonEventPressType::kRelease,
					0.1f, 
					false
				);
			}

			Util::RunPlayerActionCommand
			(
				RE::DEFAULT_OBJECT::kActionLeftRelease, a_p->coopActor.get()
			);
			// Extra animation event sent to ensure P1 stops blocking, 
			// since getting hit while blocking and then releasing block does not always work.
			a_p->coopActor->NotifyAnimationGraph("blockStop");
			// Stop sprinting to ensure any active shield charge also ends.
			Util::RunPlayerActionCommand
			(
				RE::DEFAULT_OBJECT::kActionSprintStop, a_p->coopActor.get()
			);
		}

		void CastLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset LH casting variables, remove ranged package, 
			// and evaluate the default package to stop LH casting.
			
			// Check if a killmove should be played first.
			bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kCastLH);
			if (performingKillmove)
			{
				return;
			}
			
			if (a_p->isPlayer1)
			{
				// Release the LH cast bind.
				a_p->pam->QueueP1ButtonEvent
				(
					InputAction::kCastLH,
					RE::INPUT_DEVICE::kGamepad,
					ButtonEventPressType::kRelease,
					0.0f,
					false
				);
			}
			else
			{
				auto lhSpell = a_p->em->GetLHSpell();
				// Remove casting package after resetting casting state.
				bool is2HSpell = lhSpell && lhSpell->equipSlot == glob.bothHandsEquipSlot;
				if (is2HSpell)
				{
					HelperFuncs::FinishCasting(a_p, true, true);
				}
				else
				{
					HelperFuncs::FinishCasting(a_p, true, false);
				}

				// Quit casting with P1 if the spell should've been cast with P1.
				if (bool shouldCastWithP1 = Util::ShouldCastWithP1(lhSpell); shouldCastWithP1)
				{
					a_p->pam->CastSpellWithMagicCaster
					(
						EquipIndex::kLeftHand, false, false, false, shouldCastWithP1
					);
				}
			}	
		}

		void CastRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset RH casting variables, remove ranged package, 
			// and evaluate default package to stop RH casting.
			
			// Check if a killmove should be played first.
			bool performingKillmove = HelperFuncs::CheckForKillmove(a_p, InputAction::kCastRH);
			if (performingKillmove)
			{
				return;
			}
			
			if (a_p->isPlayer1)
			{
				// Release the RH cast bind.
				a_p->pam->QueueP1ButtonEvent
				(
					InputAction::kCastRH, 
					RE::INPUT_DEVICE::kGamepad,
					ButtonEventPressType::kRelease,
					0.0f, 
					false
				);
			}
			else
			{
				auto rhSpell = a_p->em->GetRHSpell();
				// Remove casting package after resetting casting state.
				bool is2HSpell = rhSpell && rhSpell->equipSlot == glob.bothHandsEquipSlot;
				if (is2HSpell)
				{
					HelperFuncs::FinishCasting(a_p, true, true);
				}
				else
				{
					HelperFuncs::FinishCasting(a_p, false, true);
				}

				// Quit casting with P1 if the spell should've been cast with P1.
				if (bool shouldCastWithP1 = Util::ShouldCastWithP1(rhSpell); shouldCastWithP1)
				{
					a_p->pam->CastSpellWithMagicCaster
					(
						EquipIndex::kRightHand, false, false, false, shouldCastWithP1
					);
				}
			}
		}

		void CycleAmmo(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Equip cycled ammo on release.

			if (Settings::bHoldToCycle)
			{
				// Set to previous cycled item if this bind is released during the "grace period",
				// as the input release and crosshair text update aren't instant, 
				// and if the player releases the bind very shortly after the last cycling, 
				// they'd likely expect the previous cycled item to be applied, 
				// since the text would not have updated within a human-reactable interval.
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledAmmo = a_p->em->lastCycledForm;
				}
			}
			else
			{
				// Cycle on release.
				a_p->em->CycleAmmo();
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}

			if (auto ammoForm = a_p->em->currentCycledAmmo; ammoForm)
			{
				// Equip on release.
				a_p->em->EquipAmmo(ammoForm);
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, ammoForm->GetName()),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				// Nothing to equip.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: No favorited ammo", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleSpellCategoryLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle LH spell category on release.

			if (Settings::bHoldToCycle)
			{
				// Set to previous cycled item if this bind is released 
				// during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->lhSpellCategory = a_p->em->lastCycledSpellCategory;
				}
			}
			else
			{
				a_p->em->CycleHandSlotMagicCategory(false);
			}

			// Notify the player.
			const std::string_view newCategory = a_p->em->FavMagCyclingCategoryToString
			(
				a_p->em->lhSpellCategory
			);
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kEquippedItem,
				fmt::format("P{}: Left hand spell category: '{}'", a_p->playerID + 1, newCategory),
				{ 
					CrosshairMessageType::kNone, 
					CrosshairMessageType::kStealthState, 
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);;
		}

		void CycleSpellCategoryRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle RH spell category on release.

			if (Settings::bHoldToCycle)
			{
				// Set to previous cycled item if this bind is released
				// during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->rhSpellCategory = a_p->em->lastCycledSpellCategory;
				}
			}
			else
			{
				a_p->em->CycleHandSlotMagicCategory(true);
			}

			// Notify the player.
			const std::string_view newCategory = a_p->em->FavMagCyclingCategoryToString
			(
				a_p->em->rhSpellCategory
			);
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kEquippedItem,
				fmt::format
				(
					"P{}: Right hand spell category: '{}'", a_p->playerID + 1, newCategory
				),
				{ 
					CrosshairMessageType::kNone, 
					CrosshairMessageType::kStealthState, 
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void CycleSpellLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle spell in LH on release.

			if (Settings::bHoldToCycle) 
			{
				// Set to previous cycled item if this bind is released 
				// during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledRHSpellsList[!a_p->em->lhSpellCategory] = 
					(
						a_p->em->lastCycledForm
					);
				}
			}
			else
			{
				// Cycle on release.
				a_p->em->CycleHandSlotMagic(false);
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}

			// Get cycled spell from current category.
			auto spellForm = a_p->em->currentCycledLHSpellsList[!a_p->em->lhSpellCategory];
			if (spellForm)
			{
				RE::SpellItem* spell = spellForm->As<RE::SpellItem>();
				if (!spell)
				{
					return;
				}

				bool is2HSpell = spell->equipSlot == glob.bothHandsEquipSlot;
				// Equip on release.
				a_p->em->EquipSpell
				(
					spell,
					EquipIndex::kLeftHand, 
					is2HSpell ? glob.bothHandsEquipSlot : glob.leftHandEquipSlot
				);
				// Notify the player.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, spell->GetName()),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (!a_p->em->HasCyclableSpellInCategory(a_p->em->lhSpellCategory))
			{
				// No spell to equip.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: '{}' category is empty",
						a_p->playerID + 1,
						a_p->em->FavMagCyclingCategoryToString(a_p->em->lhSpellCategory)
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleSpellRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle spell in RH on release.

			if (Settings::bHoldToCycle)
			{
				// Set to previous cycled item if this bind is released
				// during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledRHSpellsList[!a_p->em->rhSpellCategory] = 
					(
						a_p->em->lastCycledForm
					);
				}
			}
			else
			{
				// Cycle on release.
				a_p->em->CycleHandSlotMagic(true);
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}

			// Get cycled spell from current category.
			auto spellForm = a_p->em->currentCycledRHSpellsList[!a_p->em->rhSpellCategory]; 
			if (spellForm)
			{
				RE::SpellItem* spell = spellForm->As<RE::SpellItem>();
				if (!spell)
				{
					return;
				}

				bool is2HSpell = spell->equipSlot == glob.bothHandsEquipSlot;
				// Equip on release.
				a_p->em->EquipSpell
				(
					spell, 
					EquipIndex::kRightHand, 
					is2HSpell ? glob.bothHandsEquipSlot : glob.rightHandEquipSlot
				);
				// Notify the player.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, spell->GetName()),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (!a_p->em->HasCyclableSpellInCategory(a_p->em->rhSpellCategory))
			{
				// No spell to equip.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: '{}' category is empty",
						a_p->playerID + 1, 
						a_p->em->FavMagCyclingCategoryToString(a_p->em->rhSpellCategory)
					),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleVoiceSlotMagic(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle voice magic (shouts + powers) on release.

			if (Settings::bHoldToCycle)
			{
				// Set to previous cycled item if this bind is released 
				// during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledVoiceMagic = a_p->em->lastCycledForm;
				}
			}
			else
			{
				// Cycle on release.
				a_p->em->CycleVoiceSlotMagic();
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}

			// Get cycled spell from current category.
			if (auto voiceForm = a_p->em->currentCycledVoiceMagic; voiceForm)
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
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, voiceForm->GetName()),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				// No shout/power to equip.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: No favorited powers/shouts", a_p->playerID + 1),
					{
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleWeaponCategoryLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle LH weapon category on release.

			if (Settings::bHoldToCycle)
			{
				// Set to previous cycled item if this bind is released
				// during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->lhWeaponCategory = a_p->em->lastCycledWeaponCategory;
				}
			}
			else
			{
				a_p->em->CycleWeaponCategory(false);
			}

			// Notify the player.
			const std::string_view newCategory = a_p->em->FavWeaponCyclingCategoryToString
			(
				a_p->em->lhWeaponCategory
			);
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kEquippedItem,
				fmt::format
				(
					"P{}: Left hand weapon category: '{}'", a_p->playerID + 1, newCategory
				),
				{ 
					CrosshairMessageType::kNone, 
					CrosshairMessageType::kStealthState, 
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void CycleWeaponCategoryRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle RH weapon category on release.

			if (Settings::bHoldToCycle)
			{
				// Set to previous cycled item if this bind is released 
				// during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->rhWeaponCategory = a_p->em->lastCycledWeaponCategory;
				}
			}
			else
			{
				a_p->em->CycleWeaponCategory(true);
			}

			// Notify the player.
			const std::string_view newCategory = a_p->em->FavWeaponCyclingCategoryToString
			(
				a_p->em->rhWeaponCategory
			);
			a_p->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kEquippedItem,
				fmt::format
				(
					"P{}: Right hand weapon category: '{}'", a_p->playerID + 1, newCategory
				),
				{ 
					CrosshairMessageType::kNone,
					CrosshairMessageType::kStealthState, 
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}

		void CycleWeaponLH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle weapon in LH on release.

			if (Settings::bHoldToCycle)
			{
				// Set to previous cycled item if this bind is released 
				// during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval / 2.0f, 0.5f))
				{
					a_p->em->currentCycledLHWeaponsList[!a_p->em->lhWeaponCategory] = 
					(
						a_p->em->lastCycledForm
					);
				}
			}
			else
			{
				// Cycle on release.
				a_p->em->CycleWeapons(false);
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}

			// Get cycled spell from current category.
			if (auto form = a_p->em->currentCycledLHWeaponsList[!a_p->em->lhWeaponCategory]; form)
			{
				// Equip on release.
				// Shield and torch go in the left hand.
				if (auto armor = form->As<RE::TESObjectARMO>(); armor && armor->IsShield()) 
				{
					a_p->em->EquipArmor(form);
				}
				else if (auto light = form->As<RE::TESObjectLIGH>(); 
						 light && light->data.flags.all(RE::TES_LIGHT_FLAGS::kCanCarry))
				{
					a_p->em->EquipForm
					(
						form, EquipIndex::kLeftHand,
						(RE::ExtraDataList*)nullptr, 
						1, 
						glob.leftHandEquipSlot
					);
				}
				else
				{
					RE::TESObjectWEAP* weap = form->As<RE::TESObjectWEAP>();
					if (!weap)
					{
						return;
					}

					bool is2HWeap = weap->equipSlot == glob.bothHandsEquipSlot;
					a_p->em->EquipForm
					(
						form, 
						EquipIndex::kLeftHand, 
						(RE::ExtraDataList*)nullptr, 
						1, 
						is2HWeap ? glob.bothHandsEquipSlot : glob.leftHandEquipSlot
					);
				}

				// Notify the player.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip '{}'", a_p->playerID + 1, form->GetName()),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (a_p->em->lhWeaponCategory == FavWeaponCyclingCategory::kAllFavorites &&
					 a_p->em->HasCyclableWeaponInCategory
					 (
						 FavWeaponCyclingCategory::kAllFavorites, false
					 ))
			{
				// Empty both hands if no weapon is returned while cycling through all favorites.
				a_p->em->UnequipHandForms(glob.bothHandsEquipSlot);
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip 'Fists'", a_p->playerID + 1),
					{
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (!a_p->em->HasCyclableWeaponInCategory(a_p->em->lhWeaponCategory, false))
			{
				// No weapon to equip.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: '{}' category is empty",
						a_p->playerID + 1,
						a_p->em->FavWeaponCyclingCategoryToString(a_p->em->lhWeaponCategory)
					),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void CycleWeaponRH(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Cycle weapon in RH on release.

			if (Settings::bHoldToCycle)
			{
				// Set to previous cycled item if this bind is released
				// during the cycling "grace period" described in CycleAmmo().
				float secsSinceLastCycling = Util::GetElapsedSeconds(a_p->lastCyclingTP);
				if (secsSinceLastCycling < min(Settings::fSecsCyclingInterval * 0.4f, 0.2f))
				{
					a_p->em->currentCycledRHWeaponsList[!a_p->em->rhWeaponCategory] = 
					(
						a_p->em->lastCycledForm
					);
				}
			}
			else
			{
				// Cycle on release.
				a_p->em->CycleWeapons(true);
				// Set cycling TP.
				a_p->lastCyclingTP = SteadyClock::now();
			}

			// Get cycled spell from current category.
			if (auto form = a_p->em->currentCycledRHWeaponsList[!a_p->em->rhWeaponCategory]; form)
			{
				if (auto weap = form->As<RE::TESObjectWEAP>(); weap) 
				{
					// Equip on release.
					// Shield and torch cannot be cycled in right hand,
					// so no need to handle those cases here.
					bool is2HWeap = weap->equipSlot == glob.bothHandsEquipSlot;
					a_p->em->EquipForm
					(
						form, 
						EquipIndex::kRightHand,
						(RE::ExtraDataList*)nullptr, 
						1, 
						is2HWeap ? glob.bothHandsEquipSlot : glob.rightHandEquipSlot
					);
					// Notify the player.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format("P{}: Equip '{}'", a_p->playerID + 1, form->GetName()),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					// Shield and torch cannot be cycled in right hand.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kEquippedItem,
						fmt::format
						(
							"P{}: Cannot equip '{}' in the right hand", 
							a_p->playerID + 1, 
							form->GetName()
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
			}
			else if (a_p->em->rhWeaponCategory == FavWeaponCyclingCategory::kAllFavorites &&
					 a_p->em->HasCyclableWeaponInCategory
					 (
						 FavWeaponCyclingCategory::kAllFavorites, true
					 ))
			{
				// Empty both hands if no weapon is returned while cycling through all favorites.
				a_p->em->UnequipHandForms(glob.bothHandsEquipSlot);
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format("P{}: Equip 'Fists'", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else if (!a_p->em->HasCyclableWeaponInCategory(a_p->em->rhWeaponCategory, true))
			{
				// No weapon to equip.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kEquippedItem,
					fmt::format
					(
						"P{}: '{}' category is empty",
						a_p->playerID + 1, 
						a_p->em->FavWeaponCyclingCategoryToString(a_p->em->rhWeaponCategory)
					),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
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

			// Object manipulation must be enabled.
			if (!Settings::bEnableObjectManipulation)
			{
				return;
			}

			//===============
			// [Grab Checks]:
			//===============

			auto targetRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
			bool targetRefrValidity = 
			(
				targetRefrPtr && Util::IsValidRefrForTargeting(targetRefrPtr.get())
			);
			bool shouldGrab = 
			{
				targetRefrValidity &&
				!a_p->mm->reqFaceTarget &&
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
				auto targetActor = targetRefrPtr->As<RE::Actor>();
				for (const auto& otherP : glob.coopPlayers)
				{
					if (!otherP->isActive || otherP == a_p)
					{
						continue;
					}

					// Grabbing another player.
					if (targetRefrPtr == otherP->coopActor)
					{
						targetPlayerActor = otherP->coopActor.get();
						targetPlayerIsDowned = otherP->isDowned;
					}

					// The refr this player is targeting is grabbed by another player.
					if (!grabbedByAnotherPlayer && 
						otherP->tm->rmm->IsManaged(a_p->tm->crosshairRefrHandle, true))
					{
						grabbedByAnotherPlayer = true;
					}

					// If released by another player, 
					// remove this refr from that player's released refr list,
					// since this player is about to grab it.
					if (otherP->tm->rmm->IsManaged(a_p->tm->crosshairRefrHandle, false))
					{
						otherP->tm->rmm->ClearRefr(a_p->tm->crosshairRefrHandle);
					}
				}

				// Actor grab check.
				// Can grab if:
				// 1. Not grabbed by another player.
				// 2. Not an actor.
				// 3. Can grab actors and not targeting a player.
				// 4. Can grab other players and targeting a player -AND-
				//	a. Can grab other players 
				//     and grabbing a player that is not trying to get up -OR-
				//	b. The target player is downed.
				shouldGrab = 
				{
					(!grabbedByAnotherPlayer) &&
					(
						(!targetActor) || 
						(targetActor->IsDead()) ||
						(
							Settings::bCanGrabActors && 
							!targetPlayerActor && 
							targetActor->actorState1.knockState !=
							RE::KNOCK_STATE_ENUM::kGetUp &&
							targetActor->actorState1.knockState !=
							RE::KNOCK_STATE_ENUM::kQueued
						) || 
						(
							(targetPlayerActor) &&
							(
								(
									Settings::bCanGrabOtherPlayers && 
									targetPlayerActor->actorState1.knockState !=
									RE::KNOCK_STATE_ENUM::kGetUp &&
									targetPlayerActor->actorState1.knockState !=
									RE::KNOCK_STATE_ENUM::kQueued
								) || 
								(
									targetPlayerIsDowned
								)
							)
						)
					)
				};

				// Motion type/race knockdown flag check.
				if (shouldGrab)
				{
					shouldGrab = false;
					auto refr3DPtr = Util::GetRefr3D(targetRefrPtr.get());
					if (refr3DPtr) 
					{
						auto hkpRigidBodyPtr = Util::GethkpRigidBody(refr3DPtr.get());
						if (hkpRigidBodyPtr) 
						{
							// Cannot pick up actors that can't be knocked down,
							// such as horses, dragons, skeletons.
							// Once dead, however, these actors can be picked up.
							// Can only manipulate this object if it has a supported motion type.
							shouldGrab = 
							(
								(
									targetActor &&
									Util::CanManipulateActor(targetActor, hkpRigidBodyPtr.get())
								) ||
								(
									!targetActor && 
									hkpRigidBodyPtr->motion.type !=
									RE::hkpMotion::MotionType::kFixed &&
									hkpRigidBodyPtr->motion.type !=
									RE::hkpMotion::MotionType::kInvalid
								)
							);
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
					text = fmt::format
					(
						"P{}: Grabbing {}", a_p->playerID + 1, targetRefrPtr->GetName()
					);
					a_p->tm->SetIsGrabbing(true);
					a_p->tm->rmm->AddGrabbedRefr
					(
						glob.coopPlayers[a_p->controllerID], targetRefrPtr->GetHandle()
					);
				}
				else
				{
					// Notify the player that the targeted object is too far away.
					text = fmt::format
					(
						"P{}: {} is too far away", a_p->playerID + 1, targetRefrPtr->GetName()
					);
				}

				// Set grabbed object message.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kActivationInfo,
					text,
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);

				// Return here, since the release check is next.
				return;
			}

			//=================
			// [Release Check]:
			//=================
			
			// Check if all grabbed objects should be released 
			// now that it has been established that they cannot grab the targeted object.
			
			// If the player is already grabbing objects and is not auto-grabbing,
			// release all grabbed objects.
			bool shouldRelease = a_p->tm->rmm->isGrabbing && !a_p->tm->rmm->isAutoGrabbing; 
			if (shouldRelease)
			{
				a_p->tm->SetIsGrabbing(false);
				// Notify the player that all grabbed objects are about to be released.
				const auto objectsToRelease = a_p->tm->rmm->grabbedRefrInfoList.size();
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kActivationInfo,
					fmt::format
					(
						"P{}: {} {} {}",
						a_p->playerID + 1, 
						a_p->mm->reqFaceTarget ? "Throwing" : "Dropping",
						objectsToRelease,
						objectsToRelease == 1 ? "object" : "objects"
					),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			} 
			else if (!shouldGrab && targetRefrValidity)
			{
				// Notify the player that this refr is not grabbable/throwable.
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kActivationInfo,
					fmt::format
					(
						"P{}: Cannot {} {}",
						a_p->playerID + 1, 
						a_p->mm->reqFaceTarget ? "throw" : "grab",
						targetRefrPtr->GetName()
					),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}

		void HotkeyEquip(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Nothing to do on release.
			// Hotkeyed form to equip was already set while the bind was held.

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

			// Aim downward at the ground below if casting a target location spell.
			HelperFuncs::PrepForTargetLocationSpellCast(a_p, quickSlotSpell);
			// If not a bound weapon spell, stop casting.
			if (auto assocWeap = Util::GetAssociatedBoundWeap(quickSlotSpell); !assocWeap)
			{
				a_p->pam->CastSpellWithMagicCaster
				(
					EquipIndex::kQuickSlotSpell,
					false,
					false, 
					false,
					Util::ShouldCastWithP1(quickSlotSpell)
				);
			}

			// Clear out linked refr target used in ranged attack package.
			a_p->tm->ClearTarget(TargetActorType::kLinkedRefr);
		}

		void RotateCam(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset cam adjustment mode to None, and relinquish control of the camera.

			HelperFuncs::SetCameraAdjustmentMode
			(
				a_p->controllerID, InputAction::kRotateCam, false
			);
		}

		void Shout(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// For P1, release the 'Shout' bind.
			// For other players, either release the shout if not shouting already 
			// and if it isn't on cooldown, or leave a cooldown notification.
			// TODO: 
			// Have co-op companion players release their shouts on hold, 
			// instead of on release, just like P1.

			if (!a_p->em->voiceForm)
			{
				a_p->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kGeneralNotification,
					fmt::format("P{}: No shout or power equipped!", a_p->playerID + 1),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection
					},
					0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
				);
				return;
			}

			if (a_p->isPlayer1)
			{
				// Send input event to emulate pressing the shout key
				// for the same time as the shout bind was held.
				a_p->pam->QueueP1ButtonEvent
				(
					InputAction::kShout, 
					RE::INPUT_DEVICE::kGamepad, 
					ButtonEventPressType::kRelease
				);
			}
			else if (!a_p->pam->isVoiceCasting)
			{
				// Not already shouting.
				auto voiceForm = a_p->em->voiceForm;
				if (!voiceForm)
				{
					// No equipped voice magic. Time to leave.
					return;
				}

				// Shout/use power if the equipped voice slot form is valid, 
				// and if the player is in god mode, the equipped form is a power, 
				// or if the player can shout (cooldown expired).
				if (a_p->isInGodMode || 
					voiceForm->IsNot(RE::FormType::Shout) || 
					a_p->pam->canShout)
				{
					a_p->taskRunner->AddTask([a_p]() { a_p->ShoutTask(); });
				}
				else if (voiceForm->Is(RE::FormType::Shout) && !a_p->pam->canShout)
				{
					// TODO: 
					// Replace with bar fill progression and flash in a way 
					// that doesn't conflict with P1's shout cooldown 
					// if multiple players shout at once.
					// Can use TrueHUD's special bar for this later on, 
					// but updating the crosshair text will serve for now.
					a_p->pam->FlashShoutMeter();
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kShoutCooldown,
						fmt::format
						(
							"P{}: Shout cooldown: {}% ",
							a_p->playerID + 1, 
							static_cast<uint32_t>
							(
								100.0f * 
								(a_p->pam->secsSinceLastShout / a_p->pam->secsCurrentShoutCooldown)
							)
						),
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

		void SpecialAction(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Perform special actions that trigger on release.
			// Or perform cleanup for single tap or hold actions.

			// Cached requested action from earlier.
			const SpecialActionType& reqAction = a_p->pam->reqSpecialAction;

			// Single tap actions. Mostly cleanup, except for Bash and Transformation.
			if (reqAction == SpecialActionType::kBlock) 
			{
				// Stop blocking instantly by sending anim event request.
				HelperFuncs::BlockInstant(a_p, false);
			}
			else if (reqAction == SpecialActionType::kCastBothHands || 
					 reqAction == SpecialActionType::kDualCast)
			{
				if (a_p->isPlayer1)
				{
					// Stop casting with both hands.
					a_p->pam->SendButtonEvent
					(
						InputAction::kCastLH, 
						RE::INPUT_DEVICE::kGamepad,
						ButtonEventPressType::kRelease,
						0.0f,
						false
					);
					a_p->pam->SendButtonEvent
					(
						InputAction::kCastRH, 
						RE::INPUT_DEVICE::kGamepad, 
						ButtonEventPressType::kRelease,
						0.0f,
						false
					);
				}
				else
				{
					if (a_p->em->HasLHSpellEquipped() || a_p->em->HasRHSpellEquipped()) 
					{
						// At least one spell still equipped.
						// (other hand may have a bound weapon now after casting)
						HelperFuncs::FinishCasting(a_p, true, true);
					}
					else if (a_p->em->HasLHStaffEquipped() && a_p->em->HasRHStaffEquipped())
					{
						// Stop casting with both staves.
						a_p->pam->CastStaffSpell(a_p->em->GetLHWeapon(), true, false);
						a_p->pam->CastStaffSpell(a_p->em->GetRHWeapon(), false, false);

						if (a_p->pam->usingLHStaff)
						{
							a_p->pam->usingLHStaff->value = 0.0f;
						}

						if (a_p->pam->usingRHStaff)
						{
							a_p->pam->usingRHStaff->value = 0.0f;
						}
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
			else if (reqAction == SpecialActionType::kToggleGrabbedRefrCollisions)
			{
				// Toggle collision for all grabbed refrs.
				a_p->tm->rmm->ToggleGrabbedRefrCollisions();
			}
			else if (reqAction == SpecialActionType::kTransformation)
			{
				// Reference for meself.
				// Beast form: WerewolfChange (0x92C48)
				// Vampire lord: DLC1VampireChange (0x200283B)
				// Revert form: DLC1RevertForm (0x200CD5C)

				if (Util::IsWerewolf(a_p->coopActor.get()))
				{
					auto targetActorPtr = Util::GetActorPtrFromHandle
					(
						a_p->tm->selectedTargetActorHandle
					);
					// If no/invalid feeding target or target is alive, 
					// show remaining transformation time.
					if (!targetActorPtr || !targetActorPtr->IsDead())
					{
						a_p->tm->SetCrosshairMessageRequest
						(
							CrosshairMessageType::kGeneralNotification,
							fmt::format
							(
								"P{}: {:.1f} seconds left as a werewolf!",
								a_p->playerID + 1, 
								(
									a_p->secsMaxTransformationTime - 
									Util::GetElapsedSeconds(a_p->transformationTP)
								)
							),
							{ 
								CrosshairMessageType::kNone, 
								CrosshairMessageType::kActivationInfo, 
								CrosshairMessageType::kStealthState, 
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs
						);

						// Nothing else to do.
						return;
					}
					
					// Werewolf feeding checks.
					if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) 
					{
						// Ensure target is a corpse within the player's reach 
						// and is of a targetable race.
						bool isNPC = targetActorPtr->HasKeyword(glob.npcKeyword);
						// Can target undead and other actors without the NPC keyword.
						auto savageFeedingPerk = dataHandler->LookupForm<RE::BGSPerk>
						(
							0x59A6, "Dawnguard.esm"
						);
						bool hasSavageFeedingPerk = 
						(
							savageFeedingPerk && a_p->coopActor->HasPerk(savageFeedingPerk)
						);
						// Is a dead NPC or the player has the savage feeding perk 
						// and the target is within range.
						bool validWerewolfFeedingTarget = 
						{
							(isNPC || hasSavageFeedingPerk) &&
							(
								a_p->coopActor->data.location.GetDistance
								(
									targetActorPtr->data.location
								) <= a_p->em->GetMaxWeapReach()
							)
						};
						if (validWerewolfFeedingTarget)
						{
							// Begin feeding animation.
							Util::PlayIdle
							(
								"SpecialFeeding", a_p->coopActor.get(), targetActorPtr.get()
							);
							auto instantCaster = a_p->coopActor->GetMagicCaster
							(
								RE::MagicSystem::CastingSource::kInstant
							); 
							if (instantCaster)
							{
								// Cast victim feed spell to advance P1's Vampire Lord progression.
								bool firstTimeFeeding = false;
								auto victimFeedSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
								(
									"PlayerWerewolfFeedVictimSpell"
								); 
								if (victimFeedSpell)
								{
									instantCaster->CastSpellImmediate
									(
										victimFeedSpell, 
										false, 
										targetActorPtr.get(),
										1.0f, 
										false, 
										0.0f, 
										a_p->coopActor.get()
									);

									// Check if the victim spell's magic effect was applied, 
									// and if so, the corpse has not been fed upon yet 
									// and the player can gain health 
									// and extend their transformation after feeding.
									auto activeEffects = targetActorPtr->GetActiveEffectList(); 
									if (activeEffects)
									{
										for (auto effect : *activeEffects)
										{
											if (!effect)
											{
												continue;
											}

											firstTimeFeeding = 
											(
												(
													effect->spell && 
													effect->spell == victimFeedSpell
												) || 
												(
													effect->GetBaseObject() == 
													victimFeedSpell->avEffectSetting
												)	
											); 
											if (firstTimeFeeding)
											{
												break;
											}
										}
									}
								}

								if (firstTimeFeeding)
								{
									// Restore health and increment transform time.
									auto attackerFeedSpell = 
									(
										RE::TESForm::LookupByEditorID<RE::SpellItem>
										(
											"WerewolfFeed"
										)
									); 
									if (attackerFeedSpell)
									{
										instantCaster->CastSpellImmediate
										(
											attackerFeedSpell, 
											false, 
											a_p->coopActor.get(), 
											1.0f,
											false, 
											0.0f, 
											a_p->coopActor.get()
										);

										float deltaTransformationTime = 0.0f;
										if (isNPC)
										{
											// Add 30 seconds to the transformation duration.
											deltaTransformationTime = 30.0f;
										}
										else if (hasSavageFeedingPerk)
										{
											// Allows for feeding with half as much 
											// transformation time restored on (un)dead,
											// non-humanoid types.
											deltaTransformationTime = 15.0f;
										}

										// Doubles health restored on feeding.
										auto gorgingPerk = dataHandler->LookupForm<RE::BGSPerk>
										(
											0x59A7, "Dawnguard.esm"
										); 
										if (gorgingPerk && a_p->coopActor->HasPerk(gorgingPerk))
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
											a_p->secsMaxTransformationTime += 
											(
												deltaTransformationTime
											);
											a_p->tm->SetCrosshairMessageRequest
											(
												CrosshairMessageType::kGeneralNotification,
												fmt::format
												(
													"P{}: {:.1f} seconds left as a werewolf "
													"after feeding "
													"<font color=\"#00FF00\">(+{:.0f})</font>!",
													a_p->playerID + 1, 
													(
														a_p->secsMaxTransformationTime - 
														Util::GetElapsedSeconds
														(
															a_p->transformationTP
														)
													), 
													deltaTransformationTime
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
								}

								// Lastly, play blood FX.
								auto bleedingSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
								(
									"BleedingSpell"
								); 
								if (bleedingSpell)
								{
									instantCaster->CastSpellImmediate
									(
										bleedingSpell, 
										false, 
										targetActorPtr.get(),
										1.0f,
										false,
										0.0f, 
										a_p->coopActor.get()
									);
								}
							}

							// Done attempting to feed, so return.
							return;
						}
					}
				}
				else if (a_p->coopActor->race && 
						 !a_p->coopActor->race->GetPlayable() && 
						 !Util::IsVampireLord(a_p->coopActor.get()))
				{
					// Show remaining transformation lifetime 
					// when transformed into a non-playable race
					// that is not the Werewolf or Vampire Lord races.
					a_p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kGeneralNotification,
						fmt::format
						(
							"P{}: Transformed: {:.1f} seconds left!", 
							a_p->playerID + 1, 
							(
								a_p->secsMaxTransformationTime - 
								Util::GetElapsedSeconds(a_p->transformationTP)
							)
						),
						{ 
							CrosshairMessageType::kNone, 
							CrosshairMessageType::kActivationInfo, 
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						Settings::fSecsBetweenDiffCrosshairMsgs
					);

					// Nothing else to do.
					return;
				}
			}

			// Do not perform OnRelease actions if interrupted or blocked.
			const auto& perfStage = 
			(
				a_p->pam->paStatesList
				[!InputAction::kSpecialAction - !InputAction::kFirstAction].perfStage
			);
			if (perfStage != PerfStage::kInputsReleased && 
				perfStage != PerfStage::kSomeInputsReleased)
			{
				return;
			}

			// Double-tap actions.
			if (reqAction == SpecialActionType::kFlop)
			{
				auto isGrabbedIter = std::find_if
				(
					glob.coopPlayers.begin(), glob.coopPlayers.end(),
					[&a_p](const auto& otherP) 
					{
						return 
						(
							otherP->isActive &&
							otherP->coopActor != a_p->coopActor && 
							otherP->tm->rmm->IsManaged(a_p->coopActor->GetHandle(), true)
						);
					}
				);
				if (isGrabbedIter != glob.coopPlayers.end())
				{
					// If grabbed, have the other player release this player before ragdolling.
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
				/*if (auto charController = a_p->coopActor->GetCharController(); charController)
				{
					charController->lock.Lock();
					Util::AdjustFallState(charController, false);
					charController->lock.Unlock();
				}*/

				// Drop on the deck and flop like a fish!
				// Set self as grabbed refr to ragdoll the player, clear the grabbed refr, 
				// and add as a released refr to listen for collisions afterward.
				const auto handle = a_p->coopActor->GetHandle();
				a_p->tm->rmm->AddGrabbedRefr(a_p, handle);
				a_p->tm->rmm->ClearGrabbedRefr(handle);

				if (a_p->tm->rmm->GetNumGrabbedRefrs() == 0)
				{
					a_p->tm->SetIsGrabbing(false);
				}

				a_p->tm->rmm->AddReleasedRefr(a_p, handle, 0.0f);
			}
			else if (reqAction == SpecialActionType::kQuickItem)
			{
				// Use the player's quick slot item.
				StartFuncs::QuickSlotItem(a_p);
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
				bool canPlayEmoteIdle = 
				(
					reqAction == SpecialActionType::kCycleOrPlayEmoteIdle &&
					a_p->pam->GetPlayerActionInputHoldTime(InputAction::kSpecialAction) <
					max(Settings::fSecsDefMinHoldTime, Settings::fSecsCyclingInterval) &&
					!a_p->coopActor->IsInRagdollState() && 
					a_p->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal && 
					!a_p->mm->lsMoved
				);
				if (canPlayEmoteIdle)
				{
					HelperFuncs::PlayEmoteIdle(a_p);
				}
			}
		}

		void Sprint(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Stop silent roll/shield charge/sprinting if not paragliding and not using M.A.R.F.

			// First check is for over-encumbrance.
			bool overencumbered = 
			(
				!Settings::bInfiniteCarryweight &&
				!a_p->isInGodMode &&
				a_p->mm->encumbranceFactor >= 1.0
			);
			// Already informed the player that they are overencumbered
			// when the sprint bind was first pressed, so just return here.
			if (overencumbered)
			{
				return;
			}

			// Cannot stop sprinting if paragliding or M.A.R.F-ing.
			if (a_p->mm->isParagliding || a_p->tm->isMARFing) 
			{
				return;
			}

			if (a_p->coopActor->IsSneaking() || a_p->pam->IsPerforming(InputAction::kBlock)) 
			{
				// Stop silent roll/shield charge animation with an action command.
				// Sprint stop.
				Util::RunPlayerActionCommand
				(
					RE::DEFAULT_OBJECT::kActionSprintStop, a_p->coopActor.get()
				);
			}
			else 
			{
				// Sprint stop action isn't processed by P1's mount.
				// Stop the mounted player actor sprint animation (hunched over) here instead.
				// Mount's Update() hook will update its speedmult when not sprinting.
				a_p->coopActor->NotifyAnimationGraph("SprintStop");
			}
		}

		void ZoomCam(const std::shared_ptr<CoopPlayer>& a_p)
		{
			// Reset adjustment mode to none and relinquish camera control.

			HelperFuncs::SetCameraAdjustmentMode(a_p->controllerID, InputAction::kZoomCam, false);
		}
	};
};

Scriptname ALYSLC_API Hidden
;=================================================
;======[Co-op API Functions For Mod Authors]======
;=================================================

; Include this file in your project to make use of these functions.
; More to come in the future.

;=================================================
;======[Constants]================================
;=================================================

; Input Actions:
; A set of indices for both inputs and actions.

; Controller Input Indices
Int Property Input_DPadU = 0 AutoReadOnly
Int Property Input_DPadD = 1 AutoReadOnly
Int Property Input_DPadL = 2 AutoReadOnly
Int Property Input_DPadR = 3 AutoReadOnly
Int Property Input_Start = 4 AutoReadOnly
Int Property Input_Back = 5 AutoReadOnly
Int Property Input_LThumb = 6 AutoReadOnly
Int Property Input_RThumb = 7 AutoReadOnly
Int Property Input_LShoulder = 8 AutoReadOnly
Int Property Input_RShoulder = 9 AutoReadOnly
Int Property Input_A = 10 AutoReadOnly
Int Property Input_B = 11 AutoReadOnly
Int Property Input_X = 12 AutoReadOnly
Int Property Input_Y = 13 AutoReadOnly
Int Property Input_LT = 14 AutoReadOnly
Int Property Input_RT = 15 AutoReadOnly
; Left and Right Stick movement.
Int Property Input_LS = 16 AutoReadOnly
Int Property Input_RS = 17 AutoReadOnly

; Total button and input counts.
Int Property ButtonTotal = 16 AutoReadOnly
Int Property InputTotal = 18 AutoReadOnly

; Player Action Indices.
Int Property Action_Activate = 18 AutoReadOnly
Int Property Action_ActivateAllOfType = 19 AutoReadOnly
Int Property Action_ActivateCancel = 20 AutoReadOnly
Int Property Action_AdjustAimPitch = 21 AutoReadOnly
Int Property Action_AttackLH = 22 AutoReadOnly
Int Property Action_AttackRH = 23 AutoReadOnly
Int Property Action_Bash = 24 AutoReadOnly
Int Property Action_Block = 25 AutoReadOnly
Int Property Action_CamLockOn = 26 AutoReadOnly
Int Property Action_CamManualPos = 27 AutoReadOnly
Int Property Action_CastLH = 28 AutoReadOnly
Int Property Action_CastRH = 29 AutoReadOnly
Int Property Action_ChangeDialoguePlayer = 30 AutoReadOnly
Int Property Action_CoopDebugMenu = 31 AutoReadOnly
Int Property Action_CoopIdlesMenu = 32 AutoReadOnly
Int Property Action_CoopMiniGamesMenu = 33 AutoReadOnly
Int Property Action_CoopSummoningMenu = 34 AutoReadOnly
Int Property Action_CycleAmmo = 35 AutoReadOnly
Int Property Action_CycleSpellCategoryLH = 36 AutoReadOnly
Int Property Action_CycleSpellCategoryRH = 37 AutoReadOnly
Int Property Action_CycleSpellLH = 38 AutoReadOnly
Int Property Action_CycleSpellRH = 39 AutoReadOnly
Int Property Action_CycleVoiceSlotMagic = 40 AutoReadOnly
Int Property Action_CycleWeaponCategoryLH = 41 AutoReadOnly
Int Property Action_CycleWeaponCategoryRH = 42 AutoReadOnly
Int Property Action_CycleWeaponLH = 43 AutoReadOnly
Int Property Action_CycleWeaponRH = 44 AutoReadOnly
Int Property Action_DebugRagdollPlayer = 45 AutoReadOnly
Int Property Action_DebugReEquipHandForms = 46 AutoReadOnly
Int Property Action_DebugRefreshPlayerManagers = 47 AutoReadOnly
Int Property Action_DebugResetPlayer = 48 AutoReadOnly
Int Property Action_DisableCoopCam = 49 AutoReadOnly
Int Property Action_Dismount = 50 AutoReadOnly
Int Property Action_Dodge = 51 AutoReadOnly
Int Property Action_FaceTarget = 52 AutoReadOnly
Int Property Action_Favorites = 53 AutoReadOnly
Int Property Action_GrabObject = 54 AutoReadOnly
Int Property Action_GrabRotateYZ = 55 AutoReadOnly
Int Property Action_HotkeyEquip = 56 AutoReadOnly
Int Property Action_Inventory = 57 AutoReadOnly
Int Property Action_Jump = 58 AutoReadOnly 
Int Property Action_MagicMenu = 59 AutoReadOnly
Int Property Action_MapMenu = 60 AutoReadOnly
Int Property Action_MoveCrosshair = 61 AutoReadOnly
Int Property Action_Pause = 62 AutoReadOnly
Int Property Action_PowerAttackDual = 63 AutoReadOnly
Int Property Action_PowerAttackLH = 64 AutoReadOnly
Int Property Action_PowerAttackRH = 65 AutoReadOnly
Int Property Action_QuickSlotCast = 66 AutoReadOnly
Int Property Action_QuickSlotItem = 67 AutoReadOnly
Int Property Action_ResetAim = 68 AutoReadOnly
Int Property Action_RotateCam = 69 AutoReadOnly
Int Property Action_RotateLeftForearm = 70 AutoReadOnly
Int Property Action_RotateLeftHand = 71 AutoReadOnly
Int Property Action_RotateLeftShoulder = 72 AutoReadOnly
Int Property Action_RotateRightForearm = 73 AutoReadOnly
Int Property Action_RotateRightHand = 74 AutoReadOnly
Int Property Action_RotateRightShoulder = 75 AutoReadOnly
Int Property Action_Sheathe = 76 AutoReadOnly
Int Property Action_Shout = 77 AutoReadOnly
Int Property Action_Sneak = 78 AutoReadOnly
Int Property Action_SpecialAction = 79 AutoReadOnly
Int Property Action_Sprint = 80 AutoReadOnly
Int Property Action_StatsMenu = 81 AutoReadOnly
Int Property Action_TeleportToPlayer = 82 AutoReadOnly
Int Property Action_TradeWithPlayer = 83 AutoReadOnly
Int Property Action_TweenMenu = 84 AutoReadOnly
Int Property Action_WaitMenu = 85 AutoReadOnly
Int Property Action_ZoomCam = 86 AutoReadOnly

; Total action count. Nice.
Int Property ActionTotal = 69 AutoReadOnly

;========================================================================================================================================================================================================================
;=====[General Player And Co-op Session State Functions]=================================================================================================================================================================
;========================================================================================================================================================================================================================

; Get the actor for the player with the given controller ID [0, 3].
; If the controller given by the ID is controlling a player, 
; return that player's character.
; Otherwise, return None.
Actor Function GetALYSLCPlayerByCID(Int a_controllerID) Global Native

; Get the controllable actor for the player with the given player ID.
; Player 1 always has a player ID of 0, and all active companion players' IDs 
; are assigned sequentially in the order of their XInput controller IDs (CIDs).
; Player IDs keep track of player-specific settings and ignore gaps in assigned CIDs.
; 0 -> Player 1
; 1 -> Player 2
; 2 -> Player 3
; 3 -> Player 4
; If the player ID is in the range [0, 3] and the corresponding player is active,
; return that player's actor.
; Otherwise, return NONE
Actor Function GetALYSLCPlayerByPID(Int a_controllerID) Global Native

; Get the ID for the controller controlling the given actor.
; If the given actor is controlled by an active player (co-op session started), 
; return the ID [0, 3] of the controller controlling the actor.
; Otherwise, return -1.
Int Function GetALYSLCPlayerCID(Actor a_actor) Global Native

; Get the ID for the player controlling the given actor.
; If the given actor is controlled by an active player (co-op session started), 
; return the ID [0, 3] of the player controlling the actor.
; Otherwise, return -1.
Int Function GetALYSLCPlayerPID(Actor a_actor) Global Native

; Check if the given actor corresponds to a actor that is controllable by a co-op player.
; A co-op session does not have to be active.
; True if a co-op character (P1 or companion player NPC), false otherwise.
Bool Function IsALYSLCCharacter(Actor a_actor) Global Native

; Check if the given actor corresponds to an active co-op player.
; True if an active co-op player actor (P1 or companion player NPC), false otherwise.
Bool Function IsALYSLCPlayer(Actor a_actor) Global Native

; Check if there is an active local co-op session.
; True if companion players have been summoned.
; False if no players have been summoned yet or all players were dismissed.
Bool Function IsSessionActive() Global Native

;========================================================================================================================================================================================================================
;=====[Get Equip, Movement, Player Action, and Targeting State Info]=================================================================================================================================================================
;========================================================================================================================================================================================================================

; Check if the player controlling the given player actor 
; is performing the player action corresponding to the given action index.
; True if the player's actor is valid and active, 
; the index is valid and corresponds to a player action, 
; and the player is performing the action.
; See above for all supported player action indices.
Bool Function IsPlayerActorPerformingAction(Actor a_playerActor, Int a_playerActionIndex) Global Native

; Check if the player with the given controller ID 
; is performing the player action corresponding to the given action index.
; True if the player's CID is between 0 and 3 and is controlling an active player, 
; the index is valid and corresponds to a player action, 
; and the player is performing the action.
; See above for all supported player action indices.
Bool Function IsPlayerCIDPerformingAction(Int a_controllerID, Int a_playerActionIndex) Global Native

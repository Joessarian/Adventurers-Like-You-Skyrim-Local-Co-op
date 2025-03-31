Scriptname ALYSLC_API Hidden
;=================================================
;======[Co-op API Functions For Mod Authors]======
;=================================================

; Include this file in your project to make use of these functions.
; More to come in the future.

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

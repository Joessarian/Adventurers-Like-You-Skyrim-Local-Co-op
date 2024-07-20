Scriptname __ALYSLC_ControlPlayerRef extends ReferenceAlias  

__ALYSLC_InitializeCoop Property InitializeScript  Auto
Actor Property PlayerRef Auto
GlobalVariable Property CanStartCoopGlobVar Auto
Keyword Property CoopPlayerKeyword Auto
ObjectReference Property CoopSummonPortal Auto
Bool Property CompletedLoad = False Auto
Int Property ControllerID = 0 Auto
Int Property CurrentCompanionsCount = 0 Auto

; Redirect to intialization event below.
Event OnInit()
	OnPlayerLoadGame()
EndEvent

; Prepare player 1 for co-op. Gets called each time
; the game loads.
Event OnPlayerLoadGame()
	; Wait until player 1 loads in.
	While (!PlayerRef)
		PlayerRef = Game.GetPlayer() as Actor
	EndWhile
	
	; Set teleport portal
	StorageUtil.SetFormValue(None, "Teleportal", CoopSummonPortal.GetBaseObject())

	Debug.Notification("[ALYSLC] Loaded save. Starting cleanup process...")
	ALYSLC.Log("[CP1R SCRIPT] Loaded save. Starting cleanup process...")
	; Quest script function called to perform intialization.
	InitializeScript.Init()
	PlayerRef = Game.GetPlayer() as Actor

	; Unregister an previous menu registrations first.
	UnregisterForAllMenus()
	; Register for menu and co-op start events.
	RegisterForMenu("BarterMenu")
	RegisterForMenu("ContainerMenu")
	RegisterForMenu("Dialogue Menu")
	RegisterForMenu("LevelUp Menu")
	RegisterForMenu("Lockpicking Menu")
	RegisterForMenu("MagicMenu")
	RegisterForMenu("MapMenu")
	RegisterForMenu("Training Menu")
	RegisterForMenu("TweenMenu")
	RegisterForModEvent("ALYSLC_Player1CoopStartEvent", "OnCoopStart")
EndEvent  
  
; Prepare player 1 for co-op.
Event OnCoopStart()
	ALYSLC.Log("[CP1R SCRIPT] OnCoopStart for Player 1")
	; Attempt to refresh P1 property if invalid for some reason. No idea what causes this to occur at times.
	Float SecsWaited = 0.0
	While (!PlayerRef && SecsWaited < 2.0)
		ALYSLC.Log("[CP1R SCRIPT] P1 invalid; attempting to get P1 again.")
		PlayerRef = Game.GetPlayer() as Actor
		Utility.Wait(0.1)
		SecsWaited += 0.1
	EndWhile
	
	If (PlayerRef != Game.GetPlayer())
		Debug.MessageBox("[ALYSLC] Critical Error: P1's actor is invalid. Cannot run P1 reference alias script.")
		ALYSLC.Log("[CP1R SCRIPT] Critical Error: P1's actor is invalid. Cannot run P1 reference alias script.")
		Return
	EndIf

	CompletedLoad = False
	ControllerID = StorageUtil.GetIntValue(PlayerRef, "ALYSLC_PlayerControllerID",  0)

	; Add co-op player keyword to player 1.
	CoopPlayerKeyword = StorageUtil.GetFormValue(None, "ALYSLC_CoopPlayer" + PO3_SKSEFunctions.IntToString(ControllerID + 1, False) + "Keyword") as Keyword
	PO3_SKSEFunctions.AddKeywordToRef(PlayerRef, CoopPlayerKeyword)
	Utility.Wait(0.1)
	CompletedLoad = True
	ALYSLC.Log("[CP1R SCRIPT] CP1R script is done with initialization")
EndEvent

; Triggered when any player dies or when only player 1 remains. Reset co-op state and end the co-op session if 
; all summoned players are dead or when player 1 dies. 
Event OnCoopEnd(Form akCompanion, Int aiControllerID)
    ControllerID = aiControllerID
	ALYSLC.Log("[CP1R SCRIPT] OnCoopEnd: " + akCompanion)
	; Remove companion from list
	If (akCompanion as ObjectReference != PlayerRef)
		ALYSLC.Log("[CP1R SCRIPT] " + (akCompanion as Actor).GetDisplayName() + " is being removed from companions list.")
		ALYSLC.Log("[CP1R SCRIPT] Companions count before: " + CurrentCompanionsCount)
		If (StorageUtil.FormListPluck(None, "ALYSLC_CompanionsList", StorageUtil.FormListFind(None, "ALYSLC_CompanionsList", akCompanion), None))
			CurrentCompanionsCount = StorageUtil.GetIntValue(None, "ALYSLC_NumCompanions", 0) - 1
		EndIf
		StorageUtil.SetIntValue(None, "ALYSLC_NumCompanions", CurrentCompanionsCount)
		ALYSLC.Log("[CP1R SCRIPT] Companions count after: " + CurrentCompanionsCount)
	Else 
		;============================================================================
    	;=============================[Co-op End Check]==============================
    	;============================================================================
		ALYSLC.Log("[CP1R SCRIPT] Co-op session is about to end. Dismissing P1 and all other players.")
		; If only player 1 remains, end the session.
		CanStartCoopGlobVar.SetValue(0.00)
		; Have player 1's listeners wait for start of next co-op session.
		ALYSLC.RequestStateChange(ControllerID, 0)
		If (CoopPlayerKeyword)
			PO3_SKSEFunctions.RemoveKeywordFromRef(PlayerRef, CoopPlayerKeyword)
		EndIf
		
		; Re-enable all controls
		Game.EnablePlayerControls()
		PlayerRef.ClearLookAt()
		PlayerRef.ClearKeepOffsetFromActor()

		; Indicate that co-op session is over and 
		; pause listener threads for refresh if needed.
		ALYSLC.Log("[CP1R SCRIPT] Dismissed P1.")
		; Must complete load process again when next session starts.
		CompletedLoad = False
	EndIf

	; All companions dismissed and co-op has ended. Can start new co-op session.
	If (StorageUtil.GetIntValue(None, "ALYSLC_NumCompanions", 0) == 0)
		ALYSLC.ChangeCoopSessionState(False)
		CanStartCoopGlobVar.SetValue(1.00)
		ALYSLC.Log("[CP1R SCRIPT] Co-op session ended. Re-summon your companions when out of combat.")
	EndIf
EndEvent




; Handles initialization of co-op entities and co-op state variables each time a save is loaded.
Scriptname __ALYSLC_InitializeCoop extends Quest  
Actor Property PlayerRef Auto
Actor[] Property DefCoopCompanionsList Auto
Actor[] Property SelectionBlacklist Auto
EffectShader Property AbsorbCompanionShader Auto
EffectShader Property UsePortalShader Auto
GlobalVariable Property CanStartCoopGlobVar Auto
GlobalVariable Property CoopIsSummoningPlayers Auto
Keyword[] Property CoopPlayerKeywords Auto
ObjectReference Property CoopSummonExitPortal Auto
ReferenceAlias Property Player1ReferenceAlias Auto
; Maximum time to wait for players to move back to their editor location.
Float Property MAX_WAIT_TIME_SECS = 2.0 AutoReadOnly

; Initialization function run on save load.
Function Init()
	ALYSLC.Log("[INIT SCRIPT] INIT")
	; Prevent summoning until initialization finishes.
	CanStartCoopGlobVar.SetValue(0.00)
	; Initialize global co-op data in the plugin.
	Bool FirstTimeInit = ALYSLC.InitializeGlobalData(Player1ReferenceAlias)
	; Reset summoning request state each time a save is loaded.
    CoopIsSummoningPlayers.SetValue(0)
	StorageUtil.SetIntValue(None, "ALYSLC_CoopStarted", -1)
	ALYSLC.Log("[INIT SCRIPT] Starting cleanup...")
	If (!Player1ReferenceAlias)
		Debug.MessageBox("[ALYSLC] P1 reference alias is invalid. Aborting. Please notify the mod author of his incompetence.")
		ALYSLC.Log("[INIT SCRIPT] P1 ref alias is invalid. Cannot start co-op. Aborting.")
		Return
	EndIf

	; Attempt to refresh P1 property if invalid for some reason. No idea what causes this to occur at times.
	Float SecsWaited = 0.0
	While (!PlayerRef && SecsWaited < 2.0)
		ALYSLC.Log("[INIT SCRIPT] P1 invalid; attempting to get P1 again.")
		PlayerRef = Game.GetPlayer() as Actor
		Utility.Wait(0.1)
		SecsWaited += 0.1
	EndWhile
	
	If (PlayerRef != Game.GetPlayer())
		Debug.MessageBox("[ALYSLC] Critical Error: P1's actor is invalid. Cannot initialize co-op data.")
		ALYSLC.Log("[INIT SCRIPT] Critical Error: P1's actor is invalid. Cannot initialize co-op data.")
		Return
	EndIf

	; Ensure that player 1 can move because this call persists through save games.
	; If summoning companions and then saving before the co-op session begins,
	; loading the save will result in player 1 still being stuck in the "don't move" state.
	; Also remove the essential flag, just in case it was set during co-op if the revive
	; system is enabled.
	PlayerRef.SetDontMove(False)
	PlayerRef.SetActorValue("SpeedMult" , 100.0)
	PlayerRef.ModActorValue("CarryWeight" , -0.01)
	PlayerRef.ModActorValue("CarryWeight" , 0.01)
	PlayerRef.SetActorValue("WeaponSpeedMult", 0.0)
	PlayerRef.SetActorValue("attackDamageMult", 1.0)
	PlayerRef.GetActorBase().SetEssential(False)

	; Set package stacks/formlists (default, combat override pairs) for each default companion
	; since these package stacks are coupled to the companions in the CK.
	; Player 1's packages always start at index 0.

	StorageUtil.SetIntValue(None, "ALYSLC_PackageFormListStartIndex" + PO3_SKSEFunctions.IntToString(PlayerRef.GetFormID(), False), 0)
	Int Iter = 0
	While (Iter < DefCoopCompanionsList.Length)
		StorageUtil.SetIntValue(None, "ALYSLC_PackageFormListStartIndex" + PO3_SKSEFunctions.IntToString(DefCoopCompanionsList[Iter].GetFormID(), False), 2 * (Iter + 1))
		Iter += 1
	EndWhile
	
	; Ensure that the camera is reset to default.
	; If cam target was somehow set to another actor when saving,
	; and that actor is not loaded when this script fires,
	; all the world geometry will load in at the lowest LOD
	; and no objects will be visible.
	Game.ForceThirdPerson()
	Game.SetPlayerAIDriven(False)
	Game.SetCameraTarget(PlayerRef)
	Game.EnablePlayerControls()
	Utility.Wait(0.5)

	; Weird crashes sometimes occur if any loaded co-op entity does not have collision enabled 
	; when the the game loads in.
	ALYSLC.ToggleCoopEntityCollision(True)
	; Indicate that co-op session is over and 
	; pause listener threads for refresh if needed.
	ALYSLC.SignalWaitForUpdate(True)

	;!!! Could cause crashes later on. Remove if crashing on save load. !!!
	Int KIter = 0
	While (KIter < 4)
		If (PlayerRef.HasKeyword(CoopPlayerKeywords[KIter]))
			ALYSLC.Log("[INIT SCRIPT] " + PlayerRef.GetDisplayName() + ": Removing keyword " + CoopPlayerKeywords[KIter].GetString())
			PO3_SKSEFunctions.RemoveKeywordFromRef(PlayerRef, CoopPlayerKeywords[KIter])
		EndIf
		KIter += 1
	EndWhile

	Game.SetCameraTarget(PlayerRef)
	; Remove straggling co-op companions and their COSs and force resummoning when out of combat.
	; Done to prevent save scumming during difficult combat encounters.
	Float WaitTimeElapsed = 0.0
	Iter = 0
	While (Iter < DefCoopCompanionsList.Length)
		If (DefCoopCompanionsList[Iter])
			ObjectReference CompanionTemp = DefCoopCompanionsList[Iter] as ObjectReference
			KIter = 0
			While (KIter < 4)
				If (CompanionTemp.HasKeyword(CoopPlayerKeywords[KIter]))
					ALYSLC.Log("[INIT SCRIPT] " + CompanionTemp.GetDisplayName() + ": Removing keyword " + CoopPlayerKeywords[KIter].GetString())
					PO3_SKSEFunctions.RemoveKeywordFromRef(CompanionTemp, CoopPlayerKeywords[KIter])
				EndIf
				KIter += 1
			EndWhile

			While (!CoopSummonExitPortal)
				Utility.Wait(0.5)
			EndWhile

			If (CompanionTemp.Is3DLoaded())
				CompanionTemp.PlaceAtMe(CoopSummonExitPortal.GetBaseObject())
				UsePortalShader.Play(CompanionTemp, 3.0)
				Utility.Wait(0.25)
				AbsorbCompanionShader.Play(PlayerRef, 1.0)
				CompanionTemp.MoveToMyEditorLocation()
			EndIf

			ALYSLC.Log("[INIT SCRIPT] About to move " + CompanionTemp.GetDisplayName() + " away.")
			WaitTimeElapsed = 0.0
			While (CompanionTemp.Is3DLoaded() && WaitTimeElapsed < MAX_WAIT_TIME_SECS)
				ALYSLC.Log("[INIT SCRIPT] I AM A PATIENT BOY. I WAIT. I WAIT. I WAIT. I WAIT.")
				CompanionTemp.MoveToMyEditorLocation()
				Utility.Wait(0.25)
				WaitTimeElapsed += 0.25
			EndWhile
			ALYSLC.Log("[INIT SCRIPT] Sent " + CompanionTemp.GetDisplayName() + " to editor location.")
		EndIf

		Iter += 1
	EndWhile
	
	; Reset global formlists and values
	StorageUtil.FormListClear(None, "ALYSLC_CompanionScripts")
	StorageUtil.FormListClear(None, "ALYSLC_CompanionsList")
	
	StorageUtil.SetFormValue(None, "ALYSLC_AimTargetLinkedRef1", PlayerRef)
	StorageUtil.SetFormValue(None, "ALYSLC_AimTargetLinkedRef2", DefCoopCompanionsList[0])
	StorageUtil.SetFormValue(None, "ALYSLC_AimTargetLinkedRef3", DefCoopCompanionsList[1])
	StorageUtil.SetFormValue(None, "ALYSLC_AimTargetLinkedRef4", DefCoopCompanionsList[2])
	StorageUtil.SetFormValue(None, "ALYSLC_CoopPlayer1Keyword", CoopPlayerKeywords[0])
	StorageUtil.SetFormValue(None, "ALYSLC_CoopPlayer2Keyword", CoopPlayerKeywords[1])
	StorageUtil.SetFormValue(None, "ALYSLC_CoopPlayer3Keyword", CoopPlayerKeywords[2])
	StorageUtil.SetFormValue(None, "ALYSLC_CoopPlayer4Keyword", CoopPlayerKeywords[3])

	StorageUtil.SetIntValue(None, "ALYSLC_ActivatingController", -1)
	StorageUtil.SetIntValue(None, "ALYSLC_CoopCamEnabled", 0)
	StorageUtil.SetIntValue(None, "ALYSLC_CoopControllerCount", 0)
	StorageUtil.SetIntValue(None, "ALYSLC_NumCompanions", 0)
	StorageUtil.SetIntValue(None, "ALYSLC_PlayerOpeningMenu", -1)
	StorageUtil.SetFormValue(None, "ALYSLC_TargetActivated", None)
	StorageUtil.SetFormValue(None, "ALYSLC_PlayerActivatingObject", None)

	Utility.Wait(0.25)
	Debug.Notification("[ALYSLC] Cleanup complete! Feel free to summon co-op companions.")
	CanStartCoopGlobVar.SetValue(1.00)
	ALYSLC.Log("[INIT SCRIPT] Initialization complete.")
		
	; If a save was loaded for the first time, notify the players of how to trigger the Summoning Menu to start co-op.
	If (FirstTimeInit)
		Debug.MessageBox("[ALYSLC] Done initializing!\nTo summon other players:\n1. Ensure Player 1 is not in combat.\n2. Hold the 'Wait' bind on Player 1's controller.\n3. Press the 'Pause' bind on Player 1's controller.")
	EndIf
EndFunction

; Run Init().
Event OnInit()
	Init()
EndEvent


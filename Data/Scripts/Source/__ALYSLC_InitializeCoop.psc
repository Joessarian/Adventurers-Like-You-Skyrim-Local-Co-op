; Handles initialization of co-op entities and co-op state variables each time a save is loaded.
Scriptname __ALYSLC_InitializeCoop extends Quest  
Actor Property PlayerRef Auto
Actor[] Property CompanionPlayerCharactersList Auto
GlobalVariable Property CanStartCoopGlobVar Auto
GlobalVariable Property CoopIsSummoningPlayers Auto
Keyword[] Property CoopPlayerKeywords Auto
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
		Debug.MessageBox("[ALYSLC]\nP1 reference alias is invalid. Aborting. Please notify the mod author of his incompetence.")
		ALYSLC.LogError("[INIT SCRIPT] P1 ref alias is invalid. Cannot start co-op. Aborting.")
		Return
	EndIf

	; Attempt to refresh P1 property if invalid for some reason. No idea what causes this to occur at times.
	If (!PlayerRef)
		ALYSLC.Log("[INIT SCRIPT] P1 invalid; attempting to get P1 again.")
		PlayerRef = Game.GetPlayer()
	EndIf
	
	If (PlayerRef != Game.GetPlayer())
		Debug.MessageBox("[ALYSLC]\nCritical Error: P1's actor is invalid. Cannot initialize co-op data.")
		ALYSLC.LogError("[INIT SCRIPT] Critical Error: P1's actor is invalid. Cannot initialize co-op data.")
		Return
	EndIf

	; Ensure that player 1 can move because this call persists through save games.
	; If summoning companions and then saving before the co-op session begins,
	; loading the save will result in player 1 still being stuck in the "don't move" state.
	PlayerRef.SetDontMove(False)
	PlayerRef.SetActorValue("SpeedMult" , 100.0)
	PlayerRef.ModActorValue("CarryWeight" , -0.01)
	PlayerRef.ModActorValue("CarryWeight" , 0.01)
	PlayerRef.SetActorValue("WeaponSpeedMult", 0.0)
	PlayerRef.SetActorValue("attackDamageMult", 1.0)

	; Ensure that the camera is reset to default.
	; If cam target was somehow set to another actor when saving,
	; and that actor is not loaded when this script fires,
	; all the world geometry will load in at the lowest LOD
	; and no objects will be visible.
	Game.ForceThirdPerson()
	Game.SetPlayerAIDriven(False)
	Game.SetCameraTarget(PlayerRef)
	Game.EnablePlayerControls()
	ALYSLC.Wait(0.25)

	; Weird crashes sometimes occur if any loaded co-op entity does not have collision enabled 
	; when the the game loads in.
	ALYSLC.EnableCoopEntityCollision()
	; Indicate that co-op session is over and pause player managers for refresh if needed.
	ALYSLC.SignalWaitForUpdate(True)

	; Remove straggling co-op companions and force resummoning.
	Float WaitTimeElapsed = 0.0
	Int Iter = 0
	ALYSLC.Log("[INIT SCRIPT] " + CompanionPlayerCharactersList.Length + " default companion player characters.")
	While (Iter < CompanionPlayerCharactersList.Length)
		If (CompanionPlayerCharactersList[Iter])
			Actor CompanionTemp = CompanionPlayerCharactersList[Iter] as Actor
			ALYSLC.Log("[INIT SCRIPT] " + CompanionTemp.GetDisplayName() + ": at index " + Iter)
			If (CompanionTemp.Is3DLoaded())
				ALYSLC.TeleportToP1OrAway(CompanionTemp, False)
				ALYSLC.Log("[INIT SCRIPT] Sent " + CompanionTemp.GetDisplayName() + " to editor location.")
			EndIf
		EndIf

		Iter += 1
	EndWhile

	; Reset global formlists and values
	StorageUtil.FormListClear(None, "ALYSLC_CompanionsList")
	StorageUtil.SetIntValue(None, "ALYSLC_NumCompanions", 0)

	ALYSLC.Wait(0.25)
	Debug.Notification("[ALYSLC] Cleanup complete! Feel free to summon co-op companions.")
	CanStartCoopGlobVar.SetValue(1.00)
	ALYSLC.Log("[INIT SCRIPT] Initialization complete.")
	
	; If a save was loaded for the first time, notify the players of how to trigger the Summoning Menu to start co-op.
	If (FirstTimeInit)
		Debug.MessageBox("[ALYSLC]\nDone initializing!\nTo assign Player 1's controller and summon other players:\n1. Ensure Player 1 is not in combat.\n2. Hold the 'Wait' bind on Player 1's controller.\n3. Press and release the 'Pause/Journal' bind on Player 1's controller.\n\nThe summoning menu will open and a tri-colored border overlay will indicate which player has control of the menu.\nSee the mod's MCM for additional information and to customize settings.\nHave fun!")
	EndIf
EndFunction

; Run Init().
Event OnInit()
	ALYSLC.Log("[INIT SCRIPT] ONINIT. Run Init()")
	Init()
EndEvent


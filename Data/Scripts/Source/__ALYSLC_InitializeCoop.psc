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
	; Update reference alias used by the plugin in case it wasn't initialized earlier.
	ALYSLC.SetPlayer1RefAlias(Player1ReferenceAlias)
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

	; Reset global formlists and values
	StorageUtil.FormListClear(None, "ALYSLC_CompanionsList")
	StorageUtil.SetIntValue(None, "ALYSLC_NumCompanions", 0)

	Debug.Notification("[ALYSLC] Cleanup complete! Feel free to summon co-op companions.")
	CanStartCoopGlobVar.SetValue(1.00)
	ALYSLC.Log("[INIT SCRIPT] Initialization complete.")
EndFunction

; Run Init().
Event OnInit()
	ALYSLC.Log("[INIT SCRIPT] ONINIT. Run Init()")
	Init()
EndEvent


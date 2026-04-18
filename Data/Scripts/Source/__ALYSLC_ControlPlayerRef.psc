Scriptname __ALYSLC_ControlPlayerRef extends ReferenceAlias  

__ALYSLC_InitializeCoop Property InitializeScript  Auto
Actor Property PlayerRef Auto

; Redirect to intialization event below.
Event OnInit()
	OnPlayerLoadGame()
EndEvent

; Prepare player 1 for co-op. Gets called each time
; the game loads.
Event OnPlayerLoadGame()
	Debug.Notification("[ALYSLC] Loaded save. Starting cleanup process...")
	ALYSLC.Log("[CP1R SCRIPT] Loaded save. Starting cleanup process...")
	; Quest script function called to perform intialization.
	InitializeScript.Init()
	PlayerRef = Game.GetPlayer()
	If (!PlayerRef)
		Debug.MessageBox("[ALYSLC]\nCritical Error: Player reference is invalid on load. Errors will likely occur when starting co-op.")
		ALYSLC.LogError("[CP1R SCRIPT] Player reference is invalid on load.")
	EndIf
EndEvent  

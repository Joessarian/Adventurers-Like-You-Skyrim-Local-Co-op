
; NOTE: Could be obsolete now, aside from directing the player to their interaction package's entry point.
; COS = Carrot on a stick -> an NPC that functions as a movement/aim target for 
; co-op players. Chose an NPC over a movable static object in order to make 
; use of the KeepOffsetFromActor() function for 360 movement targeting and 
; the ability to play particle shaders on the NPC that display on top of geometry
; (like the Detect Life shader).
Scriptname __ALYSLC_ControlCOS extends Actor  


GlobalVariable Property CoopStartedGlobVar Auto
Bool Property IsActive = False Auto
; Must move all active (linked to players) COSes when the cell changes.
Event OnCellDetach()
	If (Self)
		If (IsActive && CoopStartedGlobVar.GetValue() == 1.00)
			ALYSLC.Log("[CC SCRIPT] Should move COS" + Self.GetDisplayName() + " on cell detach.")
		ElseIf (!IsActive && Self.GetCurrentLocation() != Self.GetEditorLocation())
			Self.MoveToMyEditorLocation()
			Self.Disable()
		EndIf
	Else
		ALYSLC.Log("[CC SCRIPT] ERROR: COS is invalid on cell detach.")
	EndIf
EndEvent

; Add script to load check pool.
Event OnInit()
	StorageUtil.SetFormValue(None, "ALYSLC_ControlCOSScript" +  PO3_SKSEFunctions.IntToString(Self.GetFormID(), True), Self)
EndEvent
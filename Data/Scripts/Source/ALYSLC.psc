Scriptname ALYSLC Hidden
;=====================================
;======[Co-op Library Functions]======
;=====================================

;========================================================================================================================================================================================================================
Bool Function InitializeGlobalData(ReferenceAlias a_player1Ref) Global Native
Int[] Function GetConnectedCoopControllerIDs() Global Native
Bool Function InitializeCoop(Int a_numCompanions, Int[] a_controllerIDs, Actor[] a_coopActors, Int[] a_packageFormListIndicesList) Global Native
;========================================================================================================================================================================================================================

Function AssignPlayer1CID() Global Native
Function ChangeCoopSessionState(Bool a_start) Global Native
Function CopyNPCAppearanceToPlayer(Int a_controllerID, ActorBase a_baseToCopy, Bool a_setUseOppositeGenderAnims) Global Native
Form[] Function GetAllAppearancePresets(Race a_race, Bool a_female) Global Native
Form[] Function GetAllClasses() Global Native
String[] Function GetAllCyclableEmoteIdleEvents() Global Native
Form[] Function GetAllSelectableRaces(Int a_selectableRaceTypeFilter) Global Native
Form[] Function GetAllVoiceTypes(Bool a_female) Global Native
String[] Function GetFavoritedEmoteIdles(Int a_controllerID) Global Native
Function RequestMenuControl(Int a_controllerID, String a_menuName) Global Native
Function RequestStateChange(Int a_controllerID, Int a_newState) Global Native
Function RescaleAVsOnBaseSkillAVChange(Actor a_playerActor) Global Native
Function SetCoopPlayerClass(Actor a_playerActor, Class a_class) Global Native
Function SetCoopPlayerRace(Actor a_playerActor, Race a_race) Global Native
Function SetFavoritedEmoteIdles(Int a_controllerID, String[] a_emoteIdlesList) Global Native
Function SetGifteePlayerActor(Actor a_playerActor) Global Native
Function SetPartyInvincibility(Bool a_shouldSet) Global Native
Function SignalCamEventHandled() Global Native
Function SignalWaitForUpdate(bool a_shouldDimiss) Global Native
Function StartPlayerManagers() Global Native
Function ToggleCoopCamera(Bool a_enable) Global Native
Function ToggleCoopEntityCollision(Bool a_enable) Global Native
Function ToggleSetupMenuControl(Int a_controllerID, int a_playerID, Bool a_shouldEnter) Global Native
Function UpdateAllSerializedCompanionPlayerFIDKeys() Global Native
Function UpdateGenderAndBody(Int a_controllerID, Bool a_setFemale, Bool a_setUseOppositeGenderAnims) Global Native

Function Log(String a_messsage) Global Native

;==============================================
;==================[Debug]=====================
;==============================================
Function DisableGodModeForAllCoopPlayers() Global Native
Function DisableGodModeForPlayer(Int a_controllerID) Global Native
Function EnableGodModeForAllCoopPlayers() Global Native
Function EnableGodModeForPlayer(Int a_controllerID) Global Native
Function MoveAllCoopActorsToP1() GLobal Native
Function ReEquipHandForms(Int a_controllerID) Global Native
Function RefreshAllPlayerManagers() Global Native
Function RefreshPlayerManagers(Int a_controllerID) Global Native
Function ResetCoopCompanion(Int a_controllerID, Bool a_unequipAll, Bool a_reattachHavok) Global Native
Function ResetPlayer1AndCamera() Global Native
Function ResetPlayer1State() Global Native
Function RestartCoopCamera() Global Native
Function StopAllCombatOnCoopPlayers(Bool a_clearBounties) Global Native
Function StopMenuInputManager() Global Native

;==============================================
;=================[Utility]====================
;==============================================

; NOTE: Unused, but keeping around for reference.
; Co-op players' actors will attack cancel randomly when attacking
; with melee weapons and also stutter step when moving at certain angles
; after changing cells or when summoned. 
; This function serves as a workaround to reset the equip state and 
; fix the problem. Player actor must be disabled for the changes to take effect.
Function FixStutterAnimsAfterEquipBug(Actor akActor) Global
    akActor.ClearKeepOffsetFromActor()
    akActor.ClearLookAt()
	akActor.SetDontMove(True)
	
	Form LHObj = akActor.GetEquippedObject(0)
	Form RHObj = akActor.GetEquippedObject(1)
	Bool HasLHObjEquipped = LHObj
	Bool HasRHObjEquipped = RHObj
	If (HasLHObjEquipped)
		If (LHObj as Spell)
			akActor.UnequipSpell(LHObj as Spell, 0)
		Else
			akActor.UnequipItemEx(LHObj, 2, False)
		EndIf
		Utility.Wait(0.1)
	EndIf

	If (HasRHObjEquipped)
		If (RHObj as Spell)
			akActor.UnequipSpell(RHObj as Spell, 1)
		Else
			akActor.UnequipItemEx(RHObj, 1, False)
		EndIf
		Utility.Wait(0.1)
	EndIf

	While (akActor.IsEnabled())
		akActor.Disable()
		Utility.Wait(0.1)
	EndWhile
	Utility.Wait(0.5)

	While (!akActor.IsEnabled())
		akActor.Enable(True)
		Utility.Wait(0.1)
	EndWhile

	Utility.Wait(0.1)
	If (HasLHObjEquipped)
		If (LHObj as Spell)
			akActor.EquipSpell(LHObj as Spell, 0)
		Else
			akActor.EquipItemEx(LHObj, 2, True, False)
		EndIf
		Utility.Wait(0.1)
	EndIf

	If (HasRHObjEquipped)
		If (RHObj as Spell)
			akActor.EquipSpell(RHObj as Spell, 1)
		Else
			akActor.EquipItemEx(RHObj, 1, True, False)
		EndIf
	EndIf

	akActor.SetDontMove(False)
EndFunction

; Set default customization options for this character, if there are none saved.
; Set saved customization options on this save if they've never been set before.
Function SetInitialCustomizationOptions(Actor akPlayerActor) Global
    ActorBase Base = akPlayerActor.GetActorBase()
    ; Set saved default customization options if not set yet.
    String CurrentName = StorageUtil.GetStringValue(akPlayerActor, "ALYSLC_Name", "")
    If (CurrentName == "")
        ALYSLC.Log("[ALYSLC SCRIPT] SetInitialCustomizationOptions: Setting default name to " + akPlayerActor.GetDisplayName())
        StorageUtil.SetStringValue(akPlayerActor, "ALYSLC_Name", akPlayerActor.GetDisplayName())
    EndIf

    Class CurrentClass = StorageUtil.GetFormValue(akPlayerActor, "ALYSLC_Class", None) as Class
    If (!CurrentClass)
        ALYSLC.Log("[ALYSLC SCRIPT] SetInitialCustomizationOptions: Setting default class to " + Base.GetClass())
        StorageUtil.SetFormValue(akPlayerActor, "ALYSLC_Class", Base.GetClass())
    EndIf

    Race CurrentRace = StorageUtil.GetFormValue(akPlayerActor, "ALYSLC_Race", None) as Race
    If (!CurrentRace)
        ALYSLC.Log("[ALYSLC SCRIPT] SetInitialCustomizationOptions: Setting default race to " + Base.GetRace())
        StorageUtil.SetFormValue(akPlayerActor, "ALYSLC_Race", Base.GetRace())
    EndIf

    Int CurrentGenderOption = StorageUtil.GetIntValue(akPlayerActor, "ALYSLC_GenderOption", -1)
    If (CurrentGenderOption == -1)
        Int Sex = Base.GetSex()
        If (Sex == -1)
            Sex = 2
        EndIf
        ALYSLC.Log("[ALYSLC SCRIPT] SetInitialCustomizationOptions: Setting default gender option to " + (1 - Sex))
        StorageUtil.SetIntValue(akPlayerActor, "ALYSLC_GenderOption", 1 - Sex)
    EndIf

    ActorBase CurrentPreset = StorageUtil.GetFormValue(akPlayerActor, "ALYSLC_AppearancePreset", None) as ActorBase
    If (!CurrentPreset)
        ALYSLC.Log("[ALYSLC SCRIPT] SetInitialCustomizationOptions: Setting default preset to " + Base)
        StorageUtil.SetFormValue(akPlayerActor, "ALYSLC_AppearancePreset", Base)
    EndIf

    VoiceType CurrentVoiceType = StorageUtil.GetFormValue(akPlayerActor, "ALYSLC_VoiceType", None) as VoiceType
    If (!CurrentVoiceType)
        ALYSLC.Log("[ALYSLC SCRIPT] SetInitialCustomizationOptions: Setting default voice type to " + Base.GetVoiceType())
        StorageUtil.SetFormValue(akPlayerActor, "ALYSLC_VoiceType", Base.GetVoiceType())
    EndIf

    Float CurrentHeightMult = StorageUtil.GetFloatValue(akPlayerActor, "ALYSLC_HeightMultiplier", -1.0)
    If (CurrentHeightMult == -1)
        ALYSLC.Log("[ALYSLC SCRIPT] SetInitialCustomizationOptions: Setting default height to 1.0")
        StorageUtil.SetFloatValue(akPlayerActor, "ALYSLC_HeightMultiplier", 1.0)
    EndIf

    Float CurrentWeight = StorageUtil.GetFloatValue(akPlayerActor, "ALYSLC_Weight", -1.0)
    If (CurrentWeight == -1.0)
        ALYSLC.Log("[ALYSLC SCRIPT] SetInitialCustomizationOptions: Setting default weight to 50.0")
        StorageUtil.SetFloatValue(akPlayerActor, "ALYSLC_Weight", 50.0)
    EndIf
EndFunction

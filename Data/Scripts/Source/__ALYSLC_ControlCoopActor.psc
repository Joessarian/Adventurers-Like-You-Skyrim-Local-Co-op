Scriptname __ALYSLC_ControlCoopActor extends Actor  

Actor Property PlayerRef Auto
EffectShader Property AbsorbCompanionShader Auto
EffectShader Property ReEquipShader Auto
EffectShader Property UsePortalShader Auto
Keyword Property CoopPlayerKeyword Auto
ObjectReference Property CoopSummonPortal Auto

ObjectReference Property InventoryChest Auto
String Property FormID Auto
Bool Property CompletedLoad = False Auto
; ID for the input device controlling this player character.
; [0, 3] for controllers, 4+ for keyboards + mice.
Int Property DeviceID = -1 Auto
; Player ID for this player (never 0 since not P1).
; Always in the range [0, 3].
Int Property PlayerID = -1 Auto

; Move the co-op companion back to its editor location
; and reset its state (clear shaders/movement targets and reset inventory).
Function SendCoopPlayerHome()
    ; Remove co-op player keyword on dismissal.
    If (CoopPlayerKeyword)
        PO3_SKSEFunctions.RemoveKeywordFromRef(Self, CoopPlayerKeyword)
    EndIF
    ; Remove movement target and pathing.
    Self.ClearKeepOffsetFromActor()
    Self.ClearLookAt()

    ; Spawn portal and set shaders.
    AbsorbCompanionShader.Play(PlayerRef, 1.0)
    Self.PlaceAtMe(CoopSummonPortal.GetBaseObject())
    Self.MoveToMyEditorLocation()
    ALYSLC.Log("[CCA SCRIPT] Sent " + Self.GetDisplayName() + " home.")
EndFunction

; Refresh customization options on the co-op player's actor,
; since these changes do not persist between save games.
Function SetCustomizationOptions()
    ALYSLC.Log("[CCA SCRIPT] Set customization options for " + Self.GetDisplayName())
    ALYSLC.SetInitialCustomizationOptions(Self)

    ActorBase Base = Self.GetActorBase()
    Float SecondsWaited = 0.0
    Float TimeoutSeconds = 2.0
    ; Name
    String NewName = StorageUtil.GetStringValue(Self, "ALYSLC_Name", Self.GetDisplayName())
    If (NewName != Self.GetName())
        ALYSLC.Log("[CCA SCRIPT] Set name: " + NewName)
        Base.SetName(NewName)
        Self.SetName(NewName)
        Self.SetDisplayName(NewName)

        SecondsWaited = 0.0
        While (Self.GetDisplayName() != NewName && SecondsWaited < TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] Waiting on name change to " + NewName + " for " + Self.GetName() + ".")
            ALYSLC.Wait(0.5)
            SecondsWaited += 0.5
        EndWhile

        If (SecondsWaited >= TimeoutSeconds)
            ALYSLC.LogError("[CCA SCRIPT] ERR: Check for change timed out.")
        EndIf
    EndIf

    ; Class
    Class NewClass = StorageUtil.GetFormValue(Self, "ALYSLC_Class", None) as Class
    If (NewClass && NewClass != Base.GetClass())
        ALYSLC.Log("[CCA SCRIPT] Set class to " + NewClass)
        Base.SetClass(NewClass)
        ALYSLC.SetCoopPlayerClass(Self, NewClass, False)
                
        ; Notify the player that their perks were refunded, and that all players have had their shared perks refunded.
        ALYSLC.RequestMenuControl(DeviceID, PlayerID, "MessageBoxMenu")
        Debug.MessageBox("[ALYSLC]\n" + Self.GetDisplayName() + "'s base stats were modified on class change.\nAll of their perks were refunded, and all shared perks have also been refunded to all players."); Notify the player that their perks were refunded, and that all players have had their shared perks refunded.
        ; Have to wait for message box prompt to open.
        SecondsWaited = 0.0
        While (!UI.IsMenuOpen("MessageBoxMenu") && SecondsWaited < 2.0)
            ALYSLC.Wait(0.5)
            SecondsWaited += 0.5
        EndWhile

        ; Once open, wait until closed.
        While (UI.IsMenuOpen("MessageBoxMenu"))
            ALYSLC.Wait(0.1)
        EndWhile

        ; Wait for the class to change, or until the max wait time elapses.
        SecondsWaited = 0.0
        While (Base.GetClass() != NewClass && SecondsWaited < TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] Waiting on class change to " + NewClass + " for " + Self.GetName() + ".")
            ALYSLC.Wait(0.5)
            SecondsWaited += 0.5
        EndWhile

        If (SecondsWaited >= TimeoutSeconds)
            ALYSLC.LogError("[CCA SCRIPT] ERR: Check for change timed out.")
        EndIf
    EndIf

    ; Race
    Race NewRace = StorageUtil.GetFormValue(Self, "ALYSLC_Race", None) as Race
    Bool ShouldChangeRace = NewRace && NewRace != Base.GetRace()
    If (ShouldChangeRace)
        ALYSLC.Log("[CCA SCRIPT] Set race to " + NewRace + " from base race " + Base.GetRace() + ".")
        ALYSLC.SetCoopPlayerRace(Self, NewRace, False)
    EndIf

    ; Appearance preset and gender option
    Int GenderOption = StorageUtil.GetIntValue(Self, "ALYSLC_GenderOption", -1)
    Bool SetUseOppositeGenderAnims = GenderOption >= 2
    Bool SetFemale = (GenderOption == 0 || GenderOption == 2) || (GenderOption == -1 && Base.GetSex() == 1)
    ActorBase Preset = StorageUtil.GetFormValue(Self, "ALYSLC_AppearancePreset", None) as ActorBase
    ALYSLC.Log("[CCA SCRIPT] Saved preset sex: " + Preset.GetSex() + ", current sex: " + Base.GetSex() + ", gender option to set: " + GenderOption + "(female: " + SetFemale + ").")
    ALYSLC.Log("[CCA SCRIPT] Saved preset is " + Preset.GetName() + " (" + Preset + "), current base is " + Base.GetName() + " (" + Base + ").")
    If (!Preset)
        ; No preset to set, so set to the default racial preset, change gender, anims, and update face/body skin tone.
        ALYSLC.Log("[CCA SCRIPT] Set sex to female: " + SetFemale + " and update body to racial default, no valid preset. Gender option: " + GenderOption)
        ALYSLC.SetDefaultRacialAppearance(PlayerID, SetFemale, SetUseOppositeGenderAnims)
    ElseIf ((Preset && Preset != Base) || ((Base.GetSex() == -1) || (Base.GetSex() == 0 && SetFemale) || (Base.GetSex() == 1 && !SetFemale)))
        If (Preset && Preset != Base)
            ALYSLC.Log("[CCA SCRIPT] Set appearance preset to " + Preset.GetName() + ", use opposite gender animations: " + SetUseOppositeGenderAnims + ", gender option: " + GenderOption)
        Else
            ALYSLC.Log("[CCA SCRIPT] Gender mismatch. Current sex: " + Base.GetSex() + ". Set sex to female: " + SetFemale + ". Gender option: " + GenderOption)
        EndIf

        ALYSLC.CopyNPCAppearanceToPlayer(PlayerID, Preset, SetUseOppositeGenderAnims)
    EndIf

	; Apply custom appearance preset afterward, if any.
    If (Preset == Base)
		ALYSLC.LoadPlayerCharacterPreset(Self)
    EndIf

    ; Voice Type
    VoiceType CurrentVoiceType = Base.GetVoiceType()
    VoiceType NewVoiceType = StorageUtil.GetFormValue(Self, "ALYSLC_VoiceType", None) as VoiceType
    If (NewVoiceType && NewVoiceType != CurrentVoiceType)
        ALYSLC.Log("[CCA SCRIPT] Set voice type to " + NewVoiceType)
        Base.SetVoiceType(NewVoiceType)
        SecondsWaited = 0.0
        While (Base.GetVoiceType() != NewVoiceType && SecondsWaited < TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] Waiting on voice type change to " + NewVoiceType + " for " + Self.GetName() + ".")
            ALYSLC.Wait(0.5)
            SecondsWaited += 0.5
        EndWhile
        
        If (SecondsWaited >= TimeoutSeconds)
            ALYSLC.LogError("[CCA SCRIPT] ERR: Check for change timed out.")
        EndIf
    EndIf

    ; Weight
    Float NewWeight = StorageUtil.GetFloatValue(Self, "ALYSLC_Weight", Base.GetWeight())
    If (NewWeight != Base.GetWeight())
        ALYSLC.Log("[CCA SCRIPT] Set weight.")
        Base.SetWeight(NewWeight)
        SecondsWaited = 0.0
        While (Base.GetWeight() != NewWeight && SecondsWaited < TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] Waiting on weight change to " + NewWeight + " for " + Self.GetName() + ".")
            ALYSLC.Wait(0.5)
            SecondsWaited += 0.5
        EndWhile
        
        If (SecondsWaited >= TimeoutSeconds)
            ALYSLC.LogError("[CCA SCRIPT] ERR: Check for change timed out.")
        EndIf
    EndIf
    
    ; Height
    ALYSLC.Log("[CCA SCRIPT] Set height.")
    Self.SetScale(StorageUtil.GetFloatValue(Self, "ALYSLC_HeightMultiplier", 1.0))
EndFunction
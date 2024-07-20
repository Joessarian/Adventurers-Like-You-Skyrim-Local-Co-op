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
Int Property ControllerID = -1 Auto

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

    ; Stop sneaking and highlight shader.
    ConsoleUtil.SetSelectedReference(Self)
    ConsoleUtil.ExecuteCommand("SetForceSneak 0")
    Debug.SendAnimationEvent(Self, "SneakStop")

    ; Spawn portal and set shaders.
    AbsorbCompanionShader.Play(PlayerRef, 1.0)
    UsePortalShader.Play(Self, 1.0)
    Self.PlaceAtMe(CoopSummonPortal.GetBaseObject())
    Utility.Wait(1.0)
    ; Move player away first.
    While (Self.Is3DLoaded())
        Self.MoveToMyEditorLocation()
        Utility.Wait(0.1)
    EndWhile

    ; Then remove all items from the dead player's inventory and save in a chest.
    ; Resurrect the player, clear out the generated items the game gives them,
    ; and move all their items back into their inventory.
    ; If (Self.IsDead())
    ;     ; Save all the co-op player's items temporarily to chest
    ;     Self.RemoveAllItems(InventoryChest, True, True)
    ;     Utility.Wait(0.25)
    ;     Self.Resurrect()
    ;     Utility.Wait(0.25)
    ;     ; Clear out regenerated inventory
    ;     Utility.Wait(0.25)
    ;     Self.RemoveAllItems(None, True, True)
    ;     Utility.Wait(0.25)
    ;     ; Move items back
    ;     InventoryChest.RemoveAllItems(Self, True, True)
    ; EndIf

    Self.Resurrect()
    ALYSLC.Log("[CCA SCRIPT] GET EM OUTTA HEEUH")
    Self.ResetHealthAndLimbs()
    ;Self.Disable()
EndFunction

; Refresh customization options on the co-op player's actor,
; since these changes do not persist between save games.
Function SetCustomizationOptions()
    ALYSLC.Log("[CCA SCRIPT] Set customization options for P" + (ControllerID + 1))
    ALYSLC.SetInitialCustomizationOptions(Self)

    ActorBase Base = Self.GetActorBase()
    Float SecondsWaited = 0.0
    Float TimeoutSeconds = 2.0
    ; Name
    String NewName = StorageUtil.GetStringValue(Self, "ALYSLC_Name", Self.GetDisplayName())
    If (NewName != Self.GetDisplayName())
        ALYSLC.Log("[CCA SCRIPT] Set name: " + NewName)
        Base.SetName(NewName)
        Self.SetName(NewName)
        Self.SetDisplayName(NewName)

        SecondsWaited = 0.0
        While (Self.GetDisplayName() != NewName && SecondsWaited < TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] Waiting on name change to " + NewName + " for " + Self.GetName() + ".")
            Utility.Wait(0.1)
            SecondsWaited += 0.1
        EndWhile

        If (SecondsWaited >= TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] ERR: Check for change timed out.")
        EndIf
    EndIf

    ; Class
    Class NewClass = StorageUtil.GetFormValue(Self, "ALYSLC_Class", None) as Class
    If (NewClass && NewClass != Base.GetClass())
        ALYSLC.Log("[CCA SCRIPT] Set class to " + NewClass)
        Base.SetClass(NewClass)
        ALYSLC.SetCoopPlayerClass(Self, NewClass)
                
        ; Notify the player that their perks were refunded, and that all players have had their shared perks refunded.
        ALYSLC.RequestMenuControl(ControllerID, "MessageBoxMenu")
        Debug.MessageBox("[ALYSLC] " + Self.GetDisplayName() + "'s base stats were modified on class change.\nAll of their perks were refunded, and all shared perks have also been refunded to all players."); Notify the player that their perks were refunded, and that all players have had their shared perks refunded.
        ; Have to wait for message box prompt to open.
        SecondsWaited = 0.0
        While (!UI.IsMenuOpen("MessageBoxMenu") && SecondsWaited < 2.0)
            Utility.Wait(0.1)
            SecondsWaited += 0.1
        EndWhile

        ; Once open, wait until closed.
        While (UI.IsMenuOpen("MessageBoxMenu"))
            Utility.WaitMenuMode(0.1)
        EndWhile

        ; Wait for the class to change, or until the max wait time elapses.
        SecondsWaited = 0.0
        While (Base.GetClass() != NewClass && SecondsWaited < TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] Waiting on class change to " + NewClass + " for " + Self.GetName() + ".")
            Utility.Wait(0.1)
            SecondsWaited += 0.1
        EndWhile

        If (SecondsWaited >= TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] ERR: Check for change timed out.")
        EndIf
    EndIf

    ; Race
    Race NewRace = StorageUtil.GetFormValue(Self, "ALYSLC_Race", None) as Race
    Bool ShouldChangeRace = NewRace && NewRace != Base.GetRace()
    If (ShouldChangeRace)
        ALYSLC.Log("[CCA SCRIPT] Set race to " + NewRace)
        ALYSLC.SetCoopPlayerRace(Self, NewRace)

        ; Notify the player that their perks were refunded, and that all players have had their shared perks refunded.
        ALYSLC.RequestMenuControl(ControllerID, "MessageBoxMenu")
        Debug.MessageBox("[ALYSLC] " + Self.GetDisplayName() + "'s base stats were modified on race change.\nAll of their perks were refunded, and all shared perks have also been refunded to all players."); Notify the player that their perks were refunded, and that all players have had their shared perks refunded.
        ; Have to wait for message box prompt to open.
        SecondsWaited = 0.0
        While (!UI.IsMenuOpen("MessageBoxMenu") && SecondsWaited < 2.0)
            Utility.Wait(0.1)
            SecondsWaited += 0.1
        EndWhile

        ; Once open, wait until closed.
        While (UI.IsMenuOpen("MessageBoxMenu"))
            Utility.WaitMenuMode(0.1)
        EndWhile

        ; NOTE: must wait until race fully changes and the new actor base data is transferred over.
        ; Will cause issues with setting gender or appearance related data below if race swap is not complete.
        SecondsWaited = 0.0
        While (Base.GetRace() != NewRace && SecondsWaited < TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] Waiting on race change to " + NewRace + " for " + Self.GetName() + ".")
            Utility.Wait(0.1)
            SecondsWaited += 0.1
        EndWhile
        
        If (SecondsWaited >= TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] ERR: Check for change timed out.")
        EndIf
    EndIf

    ; Gender option
    Int GenderOption = StorageUtil.GetIntValue(Self, "ALYSLC_GenderOption", -1)
    Bool SetUseOppositeGenderAnims = GenderOption >= 2
    Bool SetFemale = (GenderOption == 0 || GenderOption == 2) || (GenderOption == -1 && Base.GetSex() == 1)

    ; Appearance Preset
    ActorBase Preset = StorageUtil.GetFormValue(Self, "ALYSLC_AppearancePreset", None) as ActorBase
    If (Preset && Preset != Base)
        ALYSLC.Log("[CCA SCRIPT] Set appearance preset to " + Preset.GetName() + ", use opposite gender animations: " + SetUseOppositeGenderAnims)
        ALYSLC.CopyNPCAppearanceToPlayer(ControllerID, Preset, SetUseOppositeGenderAnims)
    Else
        ; No preset to set, so change gender, anims, and face/body skin tone.
        ALYSLC.Log("[CCA SCRIPT] Set sex to female: " + SetFemale + " and update body to race default, no valid preset. Gender option: " + GenderOption)
        ALYSLC.UpdateGenderAndBody(ControllerID, SetFemale, SetUseOppositeGenderAnims)
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
            Utility.Wait(0.1)
            SecondsWaited += 0.1
        EndWhile
        
        If (SecondsWaited >= TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] ERR: Check for change timed out.")
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
            Utility.Wait(0.1)
            SecondsWaited += 0.1
        EndWhile
        
        If (SecondsWaited >= TimeoutSeconds)
            ALYSLC.Log("[CCA SCRIPT] ERR: Check for change timed out.")
        EndIf
    EndIf
    
    ; Height
    ALYSLC.Log("[CCA SCRIPT] Set height.")
    Self.SetScale(StorageUtil.GetFloatValue(Self, "ALYSLC_HeightMultiplier", 1.0))
EndFunction

; Event sent to all co-op players when a co-op player is dismissed
; or when player 1 dies.
Event OnCoopEnd(Form akCompanion, Int aiControllerID)
    ControllerID = aiControllerID
    ALYSLC.Log("[CCA SCRIPT] OnCoopEnd for " + Self.GetDisplayName() + " controller ID " + ControllerID)
    ; Make sure the companion being dismissed is self.
    If (akCompanion as ObjectReference == Self)
        CompletedLoad = False
        SendCoopPlayerHome()
        ALYSLC.Log("[CCA SCRIPT] Co-op player " + Self.GetDisplayName() + " has returned to their home universe.")
    EndIf
EndEvent

; Re-equip all objects equipped before the last dismissal.
; Done post-summoning due to the game's tendency to equip default/optimal
; gear after the co-op companion is disabled and enabled or resurrected.
Event OnCoopStart(Form akCoopPlayer)
    ALYSLC.Log("[CCA SCRIPT] OnCoopStart " + Self.GetDisplayName())
    If (Self.GetFormID() == akCoopPlayer.GetFormID() && StorageUtil.GetIntValue(None, "ALYSLC_CoopStarted", -1) == 0)
        ALYSLC.Log("[CCA SCRIPT] Before initializing")
        StorageUtil.FormListAdd(None, "ALYSLC_CompanionScripts", Self)
        CompletedLoad = False
        ; Get co-op session specific data for the player.
        ControllerID = StorageUtil.GetIntValue(Self, "ALYSLC_PlayerControllerID")
        CoopPlayerKeyword = StorageUtil.GetFormValue(None, "ALYSLC_CoopPlayer" + PO3_SKSEFunctions.IntToString(ControllerID + 1, False) + "Keyword") as Keyword
        FormID = PO3_SKSEFunctions.IntToString(Self.GetFormID(), True)

        ; Spawn in portal.
        Self.PlaceAtMe(CoopSummonPortal.GetBaseObject())
        Self.Enable(True)
        ; Reset equip state before performing the reset of the initialization.
        Form LHObj = Self.GetEquippedObject(0)
        Form RHObj = Self.GetEquippedObject(1)
        Bool HasLHObjEquipped = LHObj
        Bool HasRHObjEquipped = RHObj
        If (HasLHObjEquipped)
            If (LHObj as Spell)
                Self.UnequipSpell(LHObj as Spell, 0)
            Else
                Self.UnequipItemEx(LHObj, 2, False)
            EndIf
            Utility.Wait(0.1)
        EndIf

        If (HasRHObjEquipped)
            If (RHObj as Spell)
                Self.UnequipSpell(RHObj as Spell, 1)
            Else
                Self.UnequipItemEx(RHObj, 1, False)
            EndIf
            Utility.Wait(0.1)
        EndIf

        Self.Disable()
        While (Self.IsEnabled())
            Utility.Wait(0.1)
        EndWhile
        Utility.Wait(0.1)

        Self.Enable(True)
        While (!Self.IsEnabled())
            Utility.Wait(0.1)
        EndWhile

        ReEquipShader.Play(Self)
        Self.SetAlpha(0.35, True)

        Utility.Wait(0.1)
        If (HasLHObjEquipped)
            If (LHObj as Spell)
                Self.EquipSpell(LHObj as Spell, 0)
            Else
                Self.EquipItemEx(LHObj, 2, True, False)
            EndIf
            Utility.Wait(0.1)
        EndIf

        If (HasRHObjEquipped)
            If (RHObj as Spell)
                Self.EquipSpell(RHObj as Spell, 1)
            Else
                Self.EquipItemEx(RHObj, 1, True, False)
            EndIf
        EndIf

        Self.SetDontMove(False)
        PO3_SKSEFunctions.AddKeywordToRef(Self, CoopPlayerKeyword)
        ALYSLC.Log(Self.GetDisplayName() + "'s player keyword is: " + CoopPlayerKeyword)
        
        ; Prevent combat from starting with player 1.
        ConsoleUtil.SetSelectedReference(Self as ObjectReference)
        ConsoleUtil.ExecuteCommand("sifh 1")
        ConsoleUtil.ExecuteCommand("setrelationshiprank player 3")
        ConsoleUtil.ExecuteCommand("player.setrelationshiprank player 3")

        ; Add to current follower faction.
        Self.SetRelationshipRank(PlayerRef as Actor, 3)
        (PlayerRef as Actor).SetRelationshipRank(Self, 3)
        ; Clear target.
        Self.ClearLookAt()
        ; Stop combat.
         Self.StopCombat()
        ; Ensure that the player actor will not cower when threatened.
        Self.SetActorValue("Aggression", 0.0)
        Self.SetActorValue("Confidence", 4.0)
        Self.SetHeadTracking(True)

        StorageUtil.SetFormValue(None, "ALYSLC_CCAScript" + FormID, Self)
        ALYSLC.Log("[CCA SCRIPT] Done initializing")
        ; Set customization options.
        SetCustomizationOptions()
        PO3_SKSEFunctions.StopAllShaders(Self)
        CompletedLoad = True
        ALYSLC.Log("[CCA SCRIPT] Completed load for " + Self.GetDisplayName())
    Else
      ALYSLC.Log("[CCA SCRIPT] ERR: Co-op not started or not the target co-op player for this event.")
    EndIf
EndEvent

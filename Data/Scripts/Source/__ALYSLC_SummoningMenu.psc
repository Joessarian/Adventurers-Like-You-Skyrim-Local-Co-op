Scriptname __ALYSLC_SummoningMenu extends ReferenceAlias  
Actor Property PlayerRef Auto
Actor Property SelectedCharacter Auto
; Current co-op actors to summon and control.
Actor[] Property CoopActors Auto
; Co-op classes.
Form[] Property CoopClasses Auto
; Switchable NPC appearances.
Form[] Property CoopNPCAppearancePresets Auto
; Races
Form[] Property CoopRaces Auto
; Voice types.
Form[] Property CoopVoiceTypes Auto
; Holds all summonable co-op companion player NPCs (3 total by default).
FormList Property CoopCharactersFormList Auto
; Initialization done and can summon players.
GlobalVariable Property CanStartCoopGlobVar Auto
; Is a summoning event being handled?
GlobalVariable Property CoopIsSummoningPlayers Auto
; Contains the lists of binds for players 2-4.
Message Property CoopCompanionStatsMenu1 Auto
Message Property CoopCompanionStatsMenu2 Auto
Message Property CoopCompanionStatsMenu3 Auto
; Message box that gives options for setting up a new co-op session.
Message Property CoopSetupMenu Auto
; Message boxes to view stats for chosen companion.
Message[] Property StatsMenuArr Auto
; For summoning the camera operator.
ObjectReference Property CoopSummonEnterPortal Auto
; Player 1 reference alias.
ReferenceAlias Property Player1RefrAlias Auto
; Finished selecting characters to summon.
Bool Property ShouldExit  = False Auto
; If the current player has entered the character selection menu before.
Bool Property WasSelectingCharacter = False Auto
; Distance from player 1 at which to place
; co-op companions when summoned.
Float Property SummonDistance = 150.0 Auto
; Number of connected controllers for non-P1 players.
Int Property CoopCompanionControllersCount = 0 Auto
; Current number of companions.
Int Property CurrentCompanionsCount = 0 Auto
; Current controller ID for the player controlling menus.
Int Property CurrentMenuControllerID = 0 Auto
; Current selectable race type filter:
; -1: None selected.
; 0: Only races with the 'Playable' flag.
; 1: Only races that have the 'ActorTypeNPC' keyword and are used by at least 1 actor base.
; 2: Has 'ActorTypeNPC' keyword in the CK (may not have any actor bases that use it).
; 3: Only races used by at least 1 actor base.
; 4: Include all races.
Int Property CurrentSelectableRaceType = 0 Auto
; Selected customization menu entry index.
Int Property SelectedOptionIndex = -1 Auto
; Current controller IDs list.
Int[] Property ControllerIDs Auto
; Selected customization menu entry name.
String Property SelectedString = "" Auto

; Entry indices in Customization menu.
Int Property CHARACTER_NAME_ENTRY_INDEX = 0 Auto
Int Property CLASS_ENTRY_INDEX = 1 Auto
Int Property APPEARANCE_ENTRY_INDEX = 2 Auto
Int Property VOICE_TYPE_ENTRY_INDEX = 3 Auto
Int Property HEIGHT_ENTRY_INDEX = 4 Auto
Int Property WEIGHT_ENTRY_INDEX = 5 Auto

; Customization options enumeration.
Int Property OPTION_NPC_APPEARANCE_PRESET = 0 Auto
Int Property OPTION_CLASS = 1 Auto
Int Property OPTION_GENDER = 2 Auto
Int Property OPTION_HEIGHT = 3 Auto
Int Property OPTION_NAME = 4 Auto
Int Property OPTION_RACE = 5 Auto
Int Property OPTION_VOICE_TYPE = 6 Auto
Int Property OPTION_WEIGHT = 7 Auto

; Initialize array and co-op session state variables.
Function Initialize()
    StatsMenuArr = new Message[3]
    StatsMenuArr[0] = CoopCompanionStatsMenu1
    StatsMenuArr[1] = CoopCompanionStatsMenu2
    StatsMenuArr[2] = CoopCompanionStatsMenu3

    SummonDistance = 150.0
    CurrentCompanionsCount = 0
EndFunction

; Call initialize function.
Event OnInit()
    Initialize()
EndEvent

; Send co-op start event to co-op NPC scripts.
; NOTE: Not for P1.
Function CompanionCoopStart()
    Int Iter = 0
    Int NumCompanions = StorageUtil.GetIntValue(None, "ALYSLC_NumCompanions", 0)
	Form[] CompanionsList = StorageUtil.FormListToArray(None, "ALYSLC_CompanionsList")
    While (Iter < NumCompanions)
        Actor Companion = CompanionsList[Iter] as Actor
        If (Companion)
            ALYSLC.Log("[SUMMON SCRIPT] Sending co-op start event to player " + Companion.GetDisplayName() + ".")
            (Companion as __ALYSLC_ControlCoopActor).OnCoopStart(Companion)
        Else 
            ALYSLC.Log("[SUMMON SCRIPT] ERR: Could not cast companion to actor.")
        EndIf

        Iter += 1
    EndWhile
EndFunction

; Sent to co-op actor scripts to handle dismissal of a companion player.
Function EndCoopForPlayer(ObjectReference akCompanion)
    Int Handle = ModEvent.Create("ALYSLC_CoopEndEvent")
    If (Handle)
        ModEvent.PushForm(Handle, akCompanion)
        ModEvent.Send(Handle)
    EndIf
EndFunction

; Gives a list of the array indices of the packages array (sent to the plugin)
; for the first of each player's packages (4 each: default/combat override/ranged/interaction).
Int[] Function GetPackageFormListIndicesList(Actor[] CoopCompanions)
    Int Iter = 0
    Int[] PackageFormListIndicesList = new Int[4]
    ; Set player 1's package start index first.
    While (Iter < CoopCompanions.Length)
        If (CoopCompanions[Iter])
            PackageFormListIndicesList[Iter] = StorageUtil.GetIntValue(None, "ALYSLC_PackageFormListStartIndex" + PO3_SKSEFunctions.IntToString(CoopCompanions[Iter].GetFormID(), False), -1)
        EndIf
        Iter += 1
    EndWhile

    Return PackageFormListIndicesList
EndFunction

; Precondition: A character has already been selected and is valid.
; Postcondition: No customization options entry or an invalid entry is selected.
; Allow the current companion player to customize their character.
Function HandleCharacterCustomization()
    ActorBase Base = SelectedCharacter.GetActorBase()
    ALYSLC.SetInitialCustomizationOptions(SelectedCharacter)
    ; Repeatedly display the customization options menu until no option or an invalid option is chosen.
    ShowCustomizationOptionsMenu()
    While (SelectedOptionIndex != -1)
        ALYSLC.Log("[SUMMON SCRIPT] Selected option index from customization menu: " + SelectedOptionIndex + ", string: " + SelectedString)
        ; Name
        If (SelectedOptionIndex == 0)
            Debug.MessageBox("Please input a new name. In order to have your naming changes reflected in some UI components, " + \
            "such as Party Combat Parameter's character cards, you must save the game after changing the player's name and reload.")

            While (UI.IsMenuOpen("MessageBoxMenu"))
                Utility.WaitMenuMode(0.5)
            EndWhile

            String ResultStr = OpenUITextMenu()

            If (ResultStr != "" && ResultStr != SelectedCharacter.GetDisplayName())
                StorageUtil.SetStringValue(SelectedCharacter, "ALYSLC_Name", ResultStr)
                Base.SetName(ResultStr)
                SelectedCharacter.SetName(ResultStr)
                Bool Renamed = SelectedCharacter.SetDisplayName(ResultStr, True)
                If (Renamed)
                    ALYSLC.Log("[SUMMON SCRIPT] New name is '" + ResultStr + "'.")
                Else
                    ALYSLC.Log("[SUMMON SCRIPT] Could not rename player.")
                EndIf
            EndIf
        ; Class
        ElseIf (SelectedOptionIndex == 1)
            Class NewClass = ShowClassSelectionMenu()
            ALYSLC.Log("[SUMMON SCRIPT] Class chosen: " + SelectedString + ", index " + SelectedOptionIndex)
            If (NewClass)
                ALYSLC.Log("[SUMMON SCRIPT] Class chosen: " + NewClass.GetName())
                StorageUtil.SetFormValue(SelectedCharacter, "ALYSLC_Class", NewClass)
                ; Show stats menu with new class-dependent base skill values.
                ShowCharacterStatsMenu()
            EndIf
        ; Appearance
        ElseIf (SelectedOptionIndex == 2)
            Race CurrentRace = None
            Int CurrentGenderOption = -1
            Bool IsCurrentlyFemale = False
            ShowAppearanceCustomizationOptionsMenu()
            While (SelectedOptionIndex != -1)
                CurrentRace = StorageUtil.GetFormValue(SelectedCharacter, "ALYSLC_Race", Base.GetRace()) as Race
                CurrentGenderOption = StorageUtil.GetIntValue(SelectedCharacter, "ALYSLC_GenderOption", 1 - Base.GetSex())
                IsCurrentlyFemale = CurrentGenderOption == 0 || CurrentGenderOption == 2
                ALYSLC.Log("[SUMMON SCRIPT] Selected option index from appearance customization menu: " + SelectedOptionIndex + ", string: " + SelectedString)
                ; Race
                If (SelectedOptionIndex == 0)
                    Bool ReshowTypeFilterMenu = True
                    While (ReshowTypeFilterMenu)
                        CurrentSelectableRaceType = ShowSelectableRaceTypeFilterMenu() - 1
                        ; Chose race type option. 
                        ; Do not reshow.
                        If (CurrentSelectableRaceType > -1)
                            ReshowTypeFilterMenu = False
                            Race NewRace = ShowRaceSelectionMenu()
                            If (NewRace)
                                ALYSLC.Log("[SUMMON SCRIPT] Race chosen: " + NewRace)
                                StorageUtil.SetFormValue(SelectedCharacter, "ALYSLC_Race", NewRace)
                                ; Show stats menu with new race-dependent base skill values.
                                ShowCharacterStatsMenu()
                                If (NewRace != CurrentRace)
                                    ALYSLC.Log("[SUMMON SCRIPT] Race changed from " + CurrentRace.GetName() + " to " + NewRace.GetName() + ". Refreshing dependent appearance presets list. Current gender option: " + CurrentGenderOption)
                                    CoopNPCAppearancePresets = ALYSLC.GetAllAppearancePresets(NewRace, IsCurrentlyFemale)
                                    ActorBase Preset = None 
                                    If (CoopNPCAppearancePresets.Length > 0)
                                        Preset = CoopNPCAppearancePresets[0] as ActorBase
                                    EndIf
                                    
                                    If (Preset && Preset != Base)
                                        ALYSLC.Log("[SUMMON SCRIPT] Race change, set appearance preset to first one: " + Preset.GetName())
                                        StorageUtil.SetFormValue(SelectedCharacter, "ALYSLC_AppearancePreset", Preset)
                                    Else
                                        ALYSLC.Log("[SUMMON SCRIPT] Race change, but there are no presets. Set preset as current base: " + Base.GetName())
                                        StorageUtil.SetFormValue(SelectedCharacter, "ALYSLC_AppearancePreset", Base)
                                    EndIf
                                EndIf
                             EndIf
                            ALYSLC.Log("[SUMMON SCRIPT] Race chosen: " + SelectedString + ", index " + SelectedOptionIndex)
                        ElseIf (CurrentSelectableRaceType == -1)
                            ; Chose menu info entry.
                            ; Reshow.
                            ReshowTypeFilterMenu = True
                        Else
                            ; Exited menu without choosing an option.
                            ; Do not reshow.
                            ReshowTypeFilterMenu = False
                        EndIf
                    EndWhile
                ; Gender
                ElseIf (SelectedOptionIndex == 1)
                    Int NewGenderOption = ShowGenderSelectionMenu()
                    If (NewGenderOption != -1)
                        ALYSLC.Log("[SUMMON SCRIPT] Gender option chosen: " + NewGenderOption)
                        StorageUtil.SetIntValue(SelectedCharacter, "ALYSLC_GenderOption", NewGenderOption)
                        If (NewGenderOption != CurrentGenderOption)
                            ALYSLC.Log("[SUMMON SCRIPT] Gender changed from " + CurrentGenderOption + " to " + NewGenderOption + ". Refreshing dependent appearance presets list. Current race: " + CurrentRace.GetName())
                            Bool IsNowFemale = NewGenderOption == 0 || NewGenderOption == 2
                            CoopNPCAppearancePresets = ALYSLC.GetAllAppearancePresets(CurrentRace, IsNowFemale)
                            ActorBase Preset = None 
                            If (CoopNPCAppearancePresets.Length > 0)
                                Preset = CoopNPCAppearancePresets[0] as ActorBase
                            EndIf

                            If (Preset && Preset != Base)
                                ALYSLC.Log("[SUMMON SCRIPT] Gender change, set appearance preset to first one: " + Preset.GetName())
                                StorageUtil.SetFormValue(SelectedCharacter, "ALYSLC_AppearancePreset", Preset)
                            Else
                                ALYSLC.Log("[SUMMON SCRIPT] Gender change, but there are no presets. Set preset as current base: " + Base.GetName())
                                StorageUtil.SetFormValue(SelectedCharacter, "ALYSLC_AppearancePreset", Base)
                            EndIf
                        EndIf
                    EndIf
                    ALYSLC.Log("[SUMMON SCRIPT] Gender option chosen: " + SelectedString + ", index " + SelectedOptionIndex)
                ; Appearance Preset
                ElseIf (SelectedOptionIndex == 2)
                    ActorBase NewPreset = ShowAppearancePresetSelectionMenu()
                    If (NewPreset || SelectedString == "NONE")
                        ALYSLC.Log("[SUMMON SCRIPT] NPC appearance preset chosen: " + NewPreset.GetName())
                        StorageUtil.SetFormValue(SelectedCharacter, "ALYSLC_AppearancePreset", NewPreset)
                    EndIf
                    ALYSLC.Log("[SUMMON SCRIPT] NPC appearance preset chosen: " + SelectedString + ", index " + SelectedOptionIndex)
                Else
                    ALYSLC.Log("[SUMMON SCRIPT] ERR: Invalid option chosen from appearance customization menu.")
                EndIf

                ShowAppearanceCustomizationOptionsMenu()
            EndWhile
        ; Voice Type
        ElseIf (SelectedOptionIndex == 3)
            VoiceType NewVoiceType = ShowVoiceTypeSelectionMenu()
            If (NewVoiceType)
                ALYSLC.Log("[SUMMON SCRIPT] New voice type: " + NewVoiceType.GetName())
                StorageUtil.SetFormValue(SelectedCharacter, "ALYSLC_VoiceType", NewVoiceType)
            EndIf
            ALYSLC.Log("[SUMMON SCRIPT] New voice type: " + SelectedString + ", selected option index " + SelectedOptionIndex)
        ; Height
        ElseIf (SelectedOptionIndex == 4)
            Int ResultHeight = -1
            Float CurrentHeightMult = StorageUtil.GetFloatValue(SelectedCharacter, "ALYSLC_HeightMultiplier", 1.0)
            While (ResultHeight < 0 || ResultHeight > 1000)
                Debug.MessageBox("Input height is divided by 100 to get a floating point multiplier. Current height is " + \
                ((100 * CurrentHeightMult) as Int) + ". Please input a new height (0-1000, without a decimal point).")
                
                While (UI.IsMenuOpen("MessageBoxMenu"))
                    Utility.WaitMenuMode(0.5)
                EndWhile
                ResultHeight = PO3_SKSEFunctions.StringToInt(OpenUITextMenu())
            EndWhile

            Float NewHeightMult = (ResultHeight as Float) / 100.0
            ALYSLC.Log("[SUMMON SCRIPT] New height multiplier is " + NewHeightMult)
            StorageUtil.SetFloatValue(SelectedCharacter, "ALYSLC_HeightMultiplier", NewHeightMult)
        ; Weight
        ElseIf (SelectedOptionIndex == 5)
            Int ResultWeight = -1
            Float CurrentWeight = StorageUtil.GetFloatValue(SelectedCharacter, "ALYSLC_Weight", Base.GetWeight())
            While (ResultWeight < 0 || ResultWeight > 100)
                Debug.MessageBox("Current weight is " + CurrentWeight + ". Please input a new weight (0-100, without a decimal point).")
                
                While (UI.IsMenuOpen("MessageBoxMenu"))
                    Utility.WaitMenuMode(0.5)
                EndWhile
                ResultWeight = PO3_SKSEFunctions.StringToInt(OpenUITextMenu())
            EndWhile
            
            ALYSLC.Log("[SUMMON SCRIPT] New weight is " + (ResultWeight as Float))
            StorageUtil.SetFloatValue(SelectedCharacter, "ALYSLC_Weight", ResultWeight as Float)
        Else
            ALYSLC.Log("[SUMMON SCRIPT] No valid customization option selected")
        EndIf

        ; Show customiation options menu.
        ShowCustomizationOptionsMenu()
    EndWhile
EndFunction

; Open a UIExtensions text entry menu for the current player controlling menus and return the input result.
String Function OpenUITextMenu()
    UITextEntryMenu TextMenu = UIExtensions.GetMenu("UITextEntryMenu") as UITextEntryMenu
    ALYSLC.RequestMenuControl(CurrentMenuControllerID, TextMenu.ROOT_MENU)
    TextMenu.OpenMenu()
    String ResultString = TextMenu.GetResultString()

    Return ResultString
EndFunction

; Signal player 1 reference alias script to start its initialization.
Function Player1CoopStart()
    (Player1RefrAlias as __ALYSLC_ControlPlayerRef).OnCoopStart()
EndFunction

; Precondition: the selected character's actor base is valid.
; Populate and show a UIExtensions list menu based on the chosen customization option index.
Function PopulateAndShowListMenu(String PrefixTag, Int CustomizationOptionIndex, Form[] FormsList, Int PageIndex)
    ALYSLC.Log("[SUMMON SCRIPT] PopulateAndShowListMenu for " + PrefixTag + ", " + FormsList.Length + " forms.")
    UIListMenu ListMenu = UIExtensions.GetMenu("UIListMenu") as UIListMenu
    ActorBase Base = SelectedCharacter.GetActorBase()
    Form CurrentOption = None
    If (CustomizationOptionIndex == OPTION_NPC_APPEARANCE_PRESET)
        ActorBase LastSavedPreset = StorageUtil.GetFormValue(SelectedCharacter, "ALYSLC_AppearancePreset", None) as ActorBase
        If (LastSavedPreset)
            CurrentOption = LastSavedPreset
        ElseIf (Base)
            CurrentOption = Base
        EndIf
    ElseIf (CustomizationOptionIndex == OPTION_CLASS)
        Class CurrentClass = Base.GetClass()
        Class LastSavedClass = StorageUtil.GetFormValue(SelectedCharacter, "ALYSLC_Class", None) as Class
        If (LastSavedClass)
            CurrentOption = LastSavedClass
        ElseIf (CurrentClass)
            CurrentOption = CurrentClass
        EndIf
    ElseIf (CustomizationOptionIndex == OPTION_RACE)
        Race CurrentRace = Base.GetRace()
        Race LastSavedRace = StorageUtil.GetFormValue(SelectedCharacter, "ALYSLC_Race", None) as Race
        If (LastSavedRace)
            CurrentOption = LastSavedRace
        ElseIf (CurrentRace)
            CurrentOption = CurrentRace
        EndIf
    ElseIf (CustomizationOptionIndex == OPTION_VOICE_TYPE)
        VoiceType CurrentVoiceType = Base.GetVoiceType()
        VoiceType LastSavedVoiceType = StorageUtil.GetFormValue(SelectedCharacter, "ALYSLC_VoiceType", None) as VoiceType
        If (LastSavedVoiceType)
            CurrentOption = LastSavedVoiceType
        ElseIf (CurrentVoiceType)
            CurrentOption = CurrentVoiceType
        EndIf
    EndIf

    ; Total number of pages needed to display all forms:
    ; If forms list is less than or equal to 127 in length: 1 page, 127 entries.
    ; Otherwise:
    ; 126 for first and last pages, 125 for other pages.
    Int TotalPageCount = 1
    Int TotalEntryCount = FormsList.Length
    Int LastPageEntryCount = 0
    Int SelectableOptionsStartIndex = 0
    If (TotalEntryCount >= 128)
        Int CurrentPageIndex = 0
        Int PageEntriesRemaining = TotalEntryCount
        While (PageEntriesRemaining > 0)
            If (CurrentPageIndex == PageIndex)
                SelectableOptionsStartIndex = TotalEntryCount - PageEntriesRemaining
            EndIf

            If (CurrentPageIndex == 0)
                PageEntriesRemaining -= 126
            Else
                If (PageEntriesRemaining - 126 > 0)
                    PageEntriesRemaining -= 125
                Else
                    LastPageEntryCount = PageEntriesRemaining
                    PageEntriesRemaining -= 126
                EndIf
            EndIf
            CurrentPageIndex += 1
            ALYSLC.Log("[SUMMON SCRIPT] Checking Page " + CurrentPageIndex + " next. Remaining page entries: " + PageEntriesRemaining)
        EndWhile

        TotalPageCount = CurrentPageIndex
    EndIf

    Int ThisPageEntryCount = 0
    If (TotalEntryCount < 128)
        ThisPageEntryCount = TotalEntryCount
    ElseIf (PageIndex == TotalPageCount - 1)
        ThisPageEntryCount = LastPageEntryCount
    ElseIf (PageIndex == 0)
        ThisPageEntryCount = 126
    Else
        ThisPageEntryCount = 125
    EndIf
    ALYSLC.Log("[SUMMON SCRIPT] " + TotalPageCount + " pages to show " + FormsList.Length + " " + PrefixTag + " entries. Last page entry count: " + LastPageEntryCount + ", this page (" + (PageIndex + 1) + ") entry count: " + ThisPageEntryCount + ".")

    ; First option.
    ; Always shows currently applied selection.
    If (CurrentOption)
        If (StringUtil.GetLength(CurrentOption.GetName()) != 0)
            ListMenu.AddEntryItem("Current " + PrefixTag + ": " + CurrentOption.GetName())
        Else
            ListMenu.AddEntryItem("Current " + PrefixTag + ": " + PO3_SKSEFunctions.GetFormEditorID(CurrentOption))
        EndIf
    Else
        ListMenu.AddEntryItem("Current " + PrefixTag + ": INVALID")
    EndIf

    ; Selectable options.
    ALYSLC.Log("[SUMMON SCRIPT] Populating page " + (PageIndex + 1) + ": start index: " + SelectableOptionsStartIndex + ", total selectable entries: " + ThisPageEntryCount)
    Int Index = SelectableOptionsStartIndex
    While (Index < SelectableOptionsStartIndex + ThisPageEntryCount)
        Form FormEntry = FormsList[Index]
        String Name = FormEntry.GetName()
        If (StringUtil.GetLength(Name) == 0)
            Name = PO3_SKSEFunctions.GetFormEditorID(FormEntry)
        EndIf
        ListMenu.AddEntryItem("[" + PrefixTag + "] " + Name)
        Index += 1
    EndWhile

    ; Last option(s)
    If (TotalEntryCount >= 128)
        If (ThisPageEntryCount == 125)
            ; Has previous and next page.
            ListMenu.AddEntryItem("Previous Page")
            ListMenu.AddEntryItem("Next Page")
        ElseIf (ThisPageEntryCount == 126)
            If (PageIndex == 0)
                ; First page, full.
                ListMenu.AddEntryItem("Next Page")
            Else
                ; Last page, full.
                ListMenu.AddEntryItem("Previous Page")
            EndIf
        Else
            ; Last page, not full.
            ListMenu.AddEntryItem("Previous Page")
        EndIf
    EndIf

    ALYSLC.RequestMenuControl(CurrentMenuControllerID, ListMenu.ROOT_MENU)
    ListMenu.OpenMenu()
    SelectedString = ListMenu.GetResultString()
    ; Want to use this result integer to index into the forms array, so subtract 1 (should ignore the current form top entry)
    SelectedOptionIndex = ListMenu.GetResultInt() - 1
    
    ALYSLC.Log("[SUMMON SCRIPT] Selected index for " + PrefixTag + ": " + SelectedString + " at index " + SelectedOptionIndex)
    Bool MoveToPrevPage = SelectedString == "Previous Page"
    Bool MoveToNextPage = SelectedString == "Next Page"
    Bool StayOnThisPage = SelectedOptionIndex == -1
    If (MoveToPrevPage)
        PageIndex -= 1
    ElseIf (MoveToNextPage)
        PageIndex += 1
    EndIf

    If (PageIndex < 0)
        PageIndex = 0
    ElseIf (PageIndex > TotalPageCount - 1)
        PageIndex = TotalPageCount - 1
    EndIf
    
    ; Should continue prompting for selection if the first option or prev/next page options are chosen.
    If (StayOnThisPage || MoveToNextPage || MoveToPrevPage)
        ALYSLC.Log("[SUMMON SCRIPT] Moving to Page " + (PageIndex + 1) + " of " + TotalPageCount)
        PopulateAndShowListMenu(PrefixTag, CustomizationOptionIndex, FormsList, PageIndex)
    Else   
        ALYSLC.Log("[SUMMON SCRIPT] Selected index for " + PrefixTag + " in the forms list is " + (SelectedOptionIndex + SelectableOptionsStartIndex) + ", which is index " + SelectedOptionIndex + " on page " + (PageIndex + 1) + " and start index " + SelectableOptionsStartIndex + ".")
        SelectedOptionIndex = (SelectedOptionIndex + SelectableOptionsStartIndex)
        Return
    EndIf
EndFunction

; Register P1 and companion players for co-op start and end events.
Function RegisterPlayersForCoopEvents()
    ALYSLC.Log("[SUMMON SCRIPT] RegisterPlayersForCoopEvents")
    Player1RefrAlias.RegisterForModEvent("ALYSLC_CoopStartEvent", "OnCoopStart")
    Player1RefrAlias.RegisterForModEvent("ALYSLC_CoopEndEvent", "OnCoopEnd")
    Int Iter = 0
    Int NumCompanions = CoopCharactersFormList.ToArray().Length
    Form[] CompanionsList = CoopCharactersFormList.ToArray()
    While (Iter < NumCompanions)
        __ALYSLC_ControlCoopActor Companion = CompanionsList[Iter] as __ALYSLC_ControlCoopActor
        ALYSLC.Log("[SUMMON SCRIPT] RegisterPlayersForCoopEvents: companion " + (Iter + 1) + " is " + Companion)
        If (Companion)
            Companion.RegisterForModEvent("ALYSLC_CoopEndEvent", "OnCoopEnd")
            Companion.RegisterForModEvent("ALYSLC_CoopStartEvent", "OnCoopStart")
        Else 
            ALYSLC.Log("[SUMMON SCRIPT] ERR: Could not cast companion " + Companion.GetDisplayName() + " to co-op start script type. Cannot summon this player character.")
        EndIf

        Iter += 1
    EndWhile
EndFunction

; Set list of active player actors to send to plugin.
Function SetActiveCoopPlayers()
    Int NumCompanions = StorageUtil.GetIntValue(None, "ALYSLC_NumCompanions", 0)
    CoopActors = new Actor[4]
    ; Add player 1 first.
    ALYSLC.Log("[SUMMON SCRIPT] Player 1 is " + PlayerRef + ". CID: " + ControllerIDs[0])
    CoopActors[0] = PlayerRef
    StorageUtil.SetIntValue(PlayerRef, "ALYSLC_PlayerControllerID",  ControllerIDs[0])

    Int CompanionIndex = 0
    While (CompanionIndex < NumCompanions)
        ; Get co-op player NPC and add to actors list.
        Actor CoopPlayerToSummon = (StorageUtil.FormListGet(None, "ALYSLC_CompanionsList", CompanionIndex)) as Actor
        StorageUtil.SetIntValue(CoopPlayerToSummon, "ALYSLC_PlayerControllerID",  ControllerIDs[CompanionIndex + 1])
        ALYSLC.Log("[SUMMON SCRIPT] Co-op Companion Player " + CoopPlayerToSummon.GetDisplayName() + " is " + CoopPlayerToSummon + ". CID: " + ControllerIDs[CompanionIndex + 1])
        ; Add co-op actor to list of actors.
        CoopActors[CompanionIndex + 1] = CoopPlayerToSummon
        CompanionIndex += 1 
    EndWhile
EndFunction

; Show a UIExtensions list menu with appearance customization options to choose.
Function ShowAppearanceCustomizationOptionsMenu()
    ; Setup list menu with nested lists for each customization option.
    ; Max number of items per list menu = 128
    UIListMenu AppearanceCustomizationMenu = UIExtensions.GetMenu("UIListMenu") as UIListMenu
    ; Add Name, Class, Appearance, Voice, Height, and Weight options.
    ; Customization options:
    ; 0
    AppearanceCustomizationMenu.AddEntryItem("Race", -1, -1, False)
    ; 1
    AppearanceCustomizationMenu.AddEntryItem("Gender", -1, -1, False)
    ; 2
    AppearanceCustomizationMenu.AddEntryItem("Appearance Presets", -1, -1, False)

    ALYSLC.RequestMenuControl(CurrentMenuControllerID, AppearanceCustomizationMenu.ROOT_MENU)
    AppearanceCustomizationMenu.OpenMenu()
    SelectedString = AppearanceCustomizationMenu.GetResultString()
    SelectedOptionIndex = AppearanceCustomizationMenu.GetResultInt()
EndFunction

; Show a UIExtensions list menu with a list of selectable NPC appearance presets based on the player's race and gender.
ActorBase Function ShowAppearancePresetSelectionMenu()
    ; Fallback to P1 actor base if invalid.
    ActorBase CurrentActorBase = SelectedCharacter.GetActorBase()
    If (!CurrentActorBase)
        ALYSLC.Log("[SUMMON SCRIPT] Error: could not get actor base for " + SelectedCharacter.GetName())
        ALYSLC.Log("[SUMMON SCRIPT] Set actor base to P1's actor base")
        CurrentActorBase = PlayerRef.GetActorBase()
    EndIf

    ; Get forms lists for customization before opening customization menu and its submenus.
    ; Used saved race changed via the customization menu, not the current, soon-to-be-swapped-out race.
    Race SavedRace = StorageUtil.GetFormValue(SelectedCharacter, "ALYSLC_Race", None) as Race
    ; Fallback to current actor base if invalid.
    If (!SavedRace)
        SavedRace = CurrentActorBase.GetRace()
    EndIf

    ; Fallback to P1 race if still invalid.
    If (!SavedRace)
        ALYSLC.Log("[SUMMON SCRIPT] Error: could not get race for " + SelectedCharacter.GetName())
        ALYSLC.Log("[SUMMON SCRIPT] Set saved race to P1's race")
        SavedRace = PlayerRef.GetRace()
    EndIf

    Int GenderOption = StorageUtil.GetIntValue(SelectedCharacter, "ALYSLC_GenderOption", 1 - SelectedCharacter.GetActorBase().GetSex())
    Bool IsFemale = (GenderOption == 0 || GenderOption == 2)

    CoopNPCAppearancePresets = ALYSLC.GetAllAppearancePresets(SavedRace, IsFemale)
    If (CoopNPCAppearancePresets.Length > 0)
        PopulateAndShowListMenu("Preset", OPTION_NPC_APPEARANCE_PRESET, CoopNPCAppearancePresets, 0)
        Return (CoopNPCAppearancePresets[SelectedOptionIndex]) as ActorBase
    Else
        ALYSLC.Log("[SUMMON SCRIPT] ERR: no appearance preset forms were found. Should still set preset to None.")
        SelectedOptionIndex = 0
        SelectedString = "NONE"
    EndIf

    Return None
EndFunction

; Show selectable characters list.
Function ShowCharactersListMenu()
    UISelectionMenu CharSelectionMenu = UIExtensions.GetMenu("UISelectionMenu") as UISelectionMenu
    ALYSLC.RequestMenuControl(CurrentMenuControllerID, CharSelectionMenu.ROOT_MENU)
    CharSelectionMenu.OpenMenu(CoopCharactersFormList)
    SelectedCharacter = CharSelectionMenu.GetResultForm() as Actor
EndFunction

; Precondition: A character has been selected and is valid.
; Show a UIExtensions stats menu for selected player character.
Function ShowCharacterStatsMenu()
    ; Show menu updated stats after setting new class.
    If (SelectedCharacter)
        ; Set class first to update stats.
        ActorBase Base = SelectedCharacter.GetActorBase()
        ; Set new class and/or race before rescaling AVs and showing the stats menu.
        If (Base)
            Class NewClass = StorageUtil.GetFormValue(SelectedCharacter, "ALYSLC_Class", Base.GetClass()) as Class
            If (NewClass && NewClass != Base.GetClass())
                ALYSLC.Log("[SUMMON SCRIPT] About to show stats menu for " + SelectedCharacter.GetDisplayName() + ". Set class to " + NewClass.GetName() + " first.")
                Base.SetClass(NewClass)
                ALYSLC.SetCoopPlayerClass(SelectedCharacter, NewClass)
                
                ; Notify the player that their perks were refunded, and that all players have had their shared perks refunded.
                ALYSLC.RequestMenuControl(CurrentMenuControllerID, "MessageBoxMenu")
                Debug.MessageBox("[ALYSLC] " + SelectedCharacter.GetDisplayName() + "'s base stats were modified on class change.\nAll of their perks were refunded, and all shared perks have also been refunded to all players.")
                ; Have to wait for message box prompt to open.
                Float SecsWaited = 0.0
                While (!UI.IsMenuOpen("MessageBoxMenu") && SecsWaited < 2.0)
                    Utility.Wait(0.1)
                    SecsWaited += 0.1
                EndWhile

                ; Once open, wait until closed.
                While (UI.IsMenuOpen("MessageBoxMenu"))
                    Utility.WaitMenuMode(0.1)
                EndWhile
            EndIf

            Race NewRace = StorageUtil.GetFormValue(SelectedCharacter, "ALYSLC_Race", Base.GetRace()) as Race
            If (NewRace && NewRace != Base.GetRace())
                ALYSLC.Log("[SUMMON SCRIPT] About to show stats menu for " + SelectedCharacter.GetDisplayName() + ". Set race to " + NewRace.GetName() + " first.")
                ALYSLC.SetCoopPlayerRace(SelectedCharacter, NewRace)

                ; Notify the player that their perks were refunded, and that all players have had their shared perks refunded.
                ALYSLC.RequestMenuControl(CurrentMenuControllerID, "MessageBoxMenu")
                Debug.MessageBox("[ALYSLC] " + SelectedCharacter.GetDisplayName() + "'s base stats were modified on race change.\nAll of their perks were refunded, and all shared perks have also been refunded to all players.")
                ; Have to wait for message box prompt to open.
                Float SecsWaited = 0.0
                While (!UI.IsMenuOpen("MessageBoxMenu") && SecsWaited < 2.0)
                    Utility.Wait(0.1)
                    SecsWaited += 0.1
                EndWhile

                ; Once open, wait until closed.
                While (UI.IsMenuOpen("MessageBoxMenu"))
                    Utility.WaitMenuMode(0.1)
                EndWhile
            EndIf

            ALYSLC.RescaleAVsOnBaseSkillAVChange(SelectedCharacter)
        Else
            ALYSLC.Log("[SUMMON SCRIPT] ERR: Could not get actor base for " + SelectedCharacter.GetDisplayName() + " before showing stats menu.")
        EndIf
    EndIf

    UIStatsMenu CharacterStatsMenu = UIExtensions.GetMenu("UIStatsMenu") as UIStatsMenu
    ALYSLC.RequestMenuControl(CurrentMenuControllerID, CharacterStatsMenu.ROOT_MENU)
    CharacterStatsMenu.OpenMenu(SelectedCharacter)
    While (UI.IsMenuOpen("CustomMenu"))
        Utility.WaitMenuMode(0.5)
    EndWhile
EndFunction

; Show a UIExtensions list menu with selectable combat classes.
Class Function ShowClassSelectionMenu()
    CoopClasses = ALYSLC.GetAllClasses()
    If (CoopClasses.Length > 0)
        PopulateAndShowListMenu("Class", OPTION_CLASS, CoopClasses, 0)
        Return (CoopClasses[SelectedOptionIndex]) as Class
    Else
        ALYSLC.Log("[SUMMON SCRIPT] ERR: no class forms were found.")
    EndIf

    Return None
EndFunction

; Allow co-op players to choose their characters.
Function ShowCoopSetupMenu()
    CurrentCompanionsCount = StorageUtil.GetIntValue(None, "ALYSLC_NumCompanions", 0)
    ALYSLC.Log("[SUMMON SCRIPT] Current companions: " + CurrentCompanionsCount + ", current active controllers: " + (CoopCompanionControllersCount + 1) + ", should exit: " + ShouldExit)    
    ; Exit when each active controller has chosen a character or if the exit button is pressed.
    If (CurrentCompanionsCount >= CoopCompanionControllersCount || ShouldExit)
        If (CoopCompanionControllersCount <= 0)
            ALYSLC.RequestMenuControl(-1, "MessageBoxMenu")
            Debug.MessageBox("[ALYLSC] Please ensure at least two XInput-recognizable controllers are plugged in before starting co-op.")
        EndIf
        Return
    EndIf

    CurrentMenuControllerID = ControllerIDs[CurrentCompanionsCount + 1]
    If (!WasSelectingCharacter)
        ; Allow co-op player to choose by giving them control over the menu.
        StorageUtil.SetIntValue(None, "ALYSLC_PlayerOpeningMenu", CurrentCompanionsCount + 1)
        ALYSLC.ToggleSetupMenuControl(ControllerIDs[CurrentCompanionsCount + 1], CurrentCompanionsCount + 1, True)
        WasSelectingCharacter = True
    EndIf

    ALYSLC.RequestMenuControl(CurrentMenuControllerID, "MessageBoxMenu")
    Int ChosenOptionIndex = CoopSetupMenu.Show(CurrentCompanionsCount + 2)
    ; Select a character.
    If (ChosenOptionIndex == 0)
        ; Show character list.
        ShowCharactersListMenu()

        If (SelectedCharacter)
             ; Check same choice of companion
            Bool FoundDuplicate = StorageUtil.FormListHas(None, "ALYSLC_CompanionsList", SelectedCharacter)
            ; Add companion to list of co-op session NPCs if it was not already selected by another player.
            If (!FoundDuplicate)
                StorageUtil.FormListAdd(None, "ALYSLC_CompanionsList", SelectedCharacter, False)
                StorageUtil.SetIntValue(None, "ALYSLC_NumCompanions", StorageUtil.FormListCount(None, "ALYSLC_CompanionsList"))
                ALYSLC.Log("[SUMMON SCRIPT] Added " + SelectedCharacter.GetDisplayName() + " to the companion players list.")
            Else
                ALYSLC.RequestMenuControl(CurrentMenuControllerID, "MessageBoxMenu")
                Debug.MessageBox("[ALYSLC] Cannot select the same co-op character as another player.")
            EndIf

            ; Allow next co-op player to choose their character, so shut down listener thread for the current player.
            ALYSLC.ToggleSetupMenuControl(CurrentMenuControllerID, CurrentCompanionsCount + 1, False)
            ; Let the next player select their character.
            WasSelectingCharacter = False
        EndIf

        ; Go back to the setup menu.
        ShowCoopSetupMenu()
    ; Customize character.
    ElseIf (ChosenOptionIndex == 1)
        ; Get character to customize first.
        ShowCharactersListMenu()
        If (SelectedCharacter && SelectedCharacter.GetActorBase())
            HandleCharacterCustomization()
        Else
            ALYSLC.Log("[SUMMON SCRIPT] No character selected or invalid actor base. Going back to co-op setup menu.")
        EndIf
        
        ; Pull up the customization menu again if an option was chosen;
        ; otherwise, go back to the setup menu.
        ShowCoopSetupMenu()
    ; Show stats.
    ElseIf (ChosenOptionIndex == 2)
        ; Get character to show stats for first.
        ShowCharactersListMenu()
        ; Show stats menu.
        ShowCharacterStatsMenu()
        ; Go back to the setup menu.
        ShowCoopSetupMenu()
    Else
        ShouldExit = True
        ; Remove menu control for the co-op player.
        ALYSLC.RequestMenuControl(-1, "MessageBoxMenu")
        ALYSLC.ToggleSetupMenuControl(CurrentMenuControllerID, CurrentCompanionsCount + 1, False)
    EndIf
EndFunction

; Precondition: A character has been selected and is valid.
; Postcondition: Selected entry index and entry string are stored.
; Show a UIExtensions list menu with all customizable character options.
Function ShowCustomizationOptionsMenu()
    ; Setup list menu with nested lists for each customization option.
    ; Max number of items per list menu = 128
    UIListMenu CustomizationMenu = UIExtensions.GetMenu("UIListMenu") as UIListMenu
    ; Add Name, Class, Appearance, Voice, Height, and Weight options.
    ; Customization options:
    ; 0
    CustomizationMenu.AddEntryItem("Character Name", -1, -1, False)
    ; 1
    CustomizationMenu.AddEntryItem("Class", -1, -1, False)
    ; 2
    CustomizationMenu.AddEntryItem("Appearance", -1, -1, False)
    ; 3
    CustomizationMenu.AddEntryItem("Voice Type", -1, -1, False)
    ; 4
    CustomizationMenu.AddEntryItem("Height", -1, -1, False)
    ; 5
    CustomizationMenu.AddEntryItem("Weight", -1, -1, False)

    ALYSLC.RequestMenuControl(CurrentMenuControllerID, CustomizationMenu.ROOT_MENU)
    CustomizationMenu.OpenMenu()
    SelectedString = CustomizationMenu.GetResultString()
    SelectedOptionIndex = CustomizationMenu.GetResultInt()
EndFunction

; Show a UIExtensions list menu with all selectable gender options.
Int Function ShowGenderSelectionMenu()
     ; Setup list menu with nested lists for each customization option.
    ; Max number of items per list menu = 128
    UIListMenu GenderSelectionMenu = UIExtensions.GetMenu("UIListMenu") as UIListMenu
    ; Female + Female animations
    ; Male + Male animations
    ; Female + Male animations
    ; Male + Female animations
    Int CurrentGenderOptionIndex = StorageUtil.GetIntValue(SelectedCharacter, "ALYSLC_GenderOption", -1)
    String CurrentGenderOptionStr = "None"
    ; Gender option not previously customized and character had valid gender.
    If (CurrentGenderOptionIndex == -1 && SelectedCharacter.GetActorBase().GetSex() != -1)
        CurrentGenderOptionIndex = 1 - SelectedCharacter.GetActorBase().GetSex()
    EndIf

    If (CurrentGenderOptionIndex == 0)
        CurrentGenderOptionStr = "Female + Female Animations"
    ElseIf (CurrentGenderOptionIndex == 1)
        CurrentGenderOptionStr = "Male + Male Animations"
    ElseIf (CurrentGenderOptionIndex == 2)
        CurrentGenderOptionStr = "Female + Male Animations"
    ElseIf (CurrentGenderOptionIndex == 3)
        CurrentGenderOptionStr = "Male + Female Animations"
    EndIf

    GenderSelectionMenu.AddEntryItem("Current Gender: " + CurrentGenderOptionStr, -1, -1, False)
    GenderSelectionMenu.AddEntryItem("[Gender] Female + Female Animations", -1, -1, False)
    GenderSelectionMenu.AddEntryItem("[Gender] Male + Male Animations", -1, -1, False)
    GenderSelectionMenu.AddEntryItem("[Gender] Female + Male Animations", -1, -1, False)
    GenderSelectionMenu.AddEntryItem("[Gender] Male + Female Animations", -1, -1, False)

    ALYSLC.RequestMenuControl(CurrentMenuControllerID, GenderSelectionMenu.ROOT_MENU)
    GenderSelectionMenu.OpenMenu()
    SelectedString = GenderSelectionMenu.GetResultString()
    SelectedOptionIndex = GenderSelectionMenu.GetResultInt()


    ALYSLC.Log("[SUMMON SCRIPT] Gender selection menu. Selected option " + SelectedOptionIndex)
    If (SelectedOptionIndex == -1)
        Return SelectedOptionIndex
    Else
        Return SelectedOptionIndex - 1
    EndIf
EndFunction

; Show a UIExtensions list menu with all selectable races for the current race type filter.
Race Function ShowRaceSelectionMenu()
    CoopRaces = ALYSLC.GetAllSelectableRaces(CurrentSelectableRaceType)
    If (CoopRaces.Length > 0)
        PopulateAndShowListMenu("Race", OPTION_RACE, CoopRaces, 0)
        Return (CoopRaces[SelectedOptionIndex]) as Race
    Else
        ALYSLC.Log("[SUMMON SCRIPT] ERR: no race forms were found.")
    EndIf

    Return None
EndFunction

; Show a UIExtensions list menu with all choosable race filter types.
Int Function ShowSelectableRaceTypeFilterMenu()
   UIListMenu SelectableRaceTypeFilterMenu = UIExtensions.GetMenu("UIListMenu") as UIListMenu
   SelectableRaceTypeFilterMenu.AddEntryItem("[Selectable Race Type]", -1, -1, False)
   ; 0: Only races with the 'Playable' flag.
   SelectableRaceTypeFilterMenu.AddEntryItem("Playable Races", -1, -1, False)
   ; 1: Only races that have the 'ActorTypeNPC' keyword and are used by at least 1 actor base.
   SelectableRaceTypeFilterMenu.AddEntryItem("Races Used By NPC Actor Base", -1, -1, False)
   ; 2: Has 'ActorTypeNPC' keyword in the CK (may not have any actor bases that use it).
   SelectableRaceTypeFilterMenu.AddEntryItem("Races With The NPC Keyword", -1, -1, False)
   ; 3: Only races used by at least 1 actor base.
   SelectableRaceTypeFilterMenu.AddEntryItem("Races Used By Any Actor Base", -1, -1, False)
   ; 4: Include all races.
   SelectableRaceTypeFilterMenu.AddEntryItem("All Races", -1, -1, False)

   ALYSLC.RequestMenuControl(CurrentMenuControllerID, SelectableRaceTypeFilterMenu.ROOT_MENU)
   SelectableRaceTypeFilterMenu.OpenMenu()
   SelectedString = SelectableRaceTypeFilterMenu.GetResultString()
   SelectedOptionIndex = SelectableRaceTypeFilterMenu.GetResultInt()
   Return SelectedOptionIndex
EndFunction

; Show a UIExtensions list menu with all selectable voice types for the player's chosen gender.
VoiceType Function ShowVoiceTypeSelectionMenu()
    Int GenderOption = StorageUtil.GetIntValue(SelectedCharacter, "ALYSLC_GenderOption", 1 - SelectedCharacter.GetActorBase().GetSex())
    Bool IsFemale = (GenderOption == 0 || GenderOption == 2)
    CoopVoiceTypes = ALYSLC.GetAllVoiceTypes(IsFemale)
    If (CoopVoiceTypes.Length > 0)
        PopulateAndShowListMenu("Voice Type", OPTION_VOICE_TYPE, CoopVoiceTypes, 0)
        Return (CoopVoiceTypes[SelectedOptionIndex]) as VoiceType
    Else
        ALYSLC.Log("[SUMMON SCRIPT] ERR: no voice type forms were found.")
    EndIf

    Return None
EndFunction

; Spawn in all the chosen co-op characters.
Function SummonCoopPlayers()
    ; Spawn in co-op companions after disabling them.
    ; The companion player initialization script will enable them when it runs before co-op starts.
    Int CompanionIndex = 1
    While (CompanionIndex < CoopActors.Length)
        If (CoopActors[CompanionIndex] && CoopActors[CompanionIndex] as Actor)
            ; Get co-op player NPC and add to actors list.
            Actor CoopPlayerToSummon = CoopActors[CompanionIndex] as Actor
            ; Ensure they're alive before teleporting over.
            While (CoopPlayerToSummon.IsDead())
                ALYSLC.Log("[SUMMON SCRIPT] Resurrecting dead companion " + CoopPlayerToSummon + " before summoning.")
                CoopPlayerToSummon.Resurrect()
                Utility.Wait(0.1)
            EndWhile
            ALYSLC.Log("[SUMMON SCRIPT] Summoning companion " + CoopPlayerToSummon + ".")
            ; Set invisible before moving.
            CoopPlayerToSummon.Disable()
            While (!CoopPlayerToSummon.IsNearPlayer())
                CoopPlayerToSummon.MoveTo(PlayerRef, 0.0, 100.0, 0.0, True)
                Utility.Wait(0.1)
            EndWhile
        EndIf

        CompanionIndex += 1 
    EndWhile
EndFunction

; A request to summon companion players drives the teardown and startup of co-op sessions.
Event OnSummoningMenuRequest()
    ALYSLC.Log("[SUMMON SCRIPT] OnSummoningMenuRequest() Event Received.")
    ; PlayerRef not always valid.
	; Attempt to refresh P1 property if invalid for some reason. No idea what causes this to occur at times.
	Float SecsWaited = 0.0
	While (!PlayerRef && SecsWaited < 2.0)
		ALYSLC.Log("[SUMMON SCRIPT] P1 invalid; attempting to get P1 again.")
		PlayerRef = Game.GetPlayer() as Actor
		Utility.Wait(0.1)
		SecsWaited += 0.1
	EndWhile
	
	If (PlayerRef != Game.GetPlayer())
		Debug.MessageBox("[ALYSLC] Critical Error: P1's actor is invalid. Cannot summon players.")
		ALYSLC.Log("[SUMMON SCRIPT] Critical Error: P1's actor is invalid. Cannot summon players. P1 actor set as " + PlayerRef + ", game player set as " + Game.GetPlayer())
		Return
	EndIf

    ; Ensure initialization is done.
    If (CanStartCoopGlobVar.GetValue() == 0)
        Debug.Notification("[ALYSLC] Please wait... Initialization in progress.")
        ALYSLC.Log("[SUMMON SCRIPT] Cannot start summoning until initialization is complete.")
        Return
    EndIf

    If (CoopIsSummoningPlayers.GetValue() == 1.0)
        Debug.Notification("[ALYSLC] Summoning menu already open.")
        ALYSLC.Log("[SUMMON SCRIPT] Summoning menu already open.")
        Return
    EndIf

    ; Can handle request.
    ; Set as summoning, so the plugin doesn't attempt to open another summoning menu.
    CoopIsSummoningPlayers.SetValue(1.0)
    ; Putting the clean-up code here because
    ; the VM runs the effect finish and effect start event
    ; code concurrently and the interweaved code that
    ; results is difficult to impose any kind of 
    ; sequential ordering on.

    Initialize()

    ; Clear out old data from previous sessions and stop any listener threads.
    StorageUtil.SetIntValue(None, "ALYSLC_CoopStarted", 0)
    StorageUtil.FormListClear(None, "ALYSLC_CompanionScripts")
    StorageUtil.SetIntValue(None, "ALYSLC_PlayerOpeningMenu", -1)
    ; End any active co-op session, pause all listener threads, dismiss any companions.
    ALYSLC.SignalWaitForUpdate(True)

    ; Wait until all companions are unloaded from the cell 
    ; before opening the character selection menu.
    Form[] CompanionsList = StorageUtil.FormListToArray(None, "ALYSLC_CompanionsList")
    Int Index = 0
    Bool Player3DLoaded = False
    Bool PlayerIsDead = False
    While (Index < CompanionsList.Length)
        ; Check that any previously summoned players do not have their 3D loaded
        ; and are not dead. Otherwise, wait until all companions are unloaded and resurrected.
        If (!CompanionsList[Index])
            Index += 1
        Else 
            Actor PlayerActor = (CompanionsList[Index] as Actor)
            Player3DLoaded = PlayerActor.Is3DLoaded()
            PlayerIsDead = PlayerActor.IsDead()
            If (Player3DLoaded || PlayerIsDead)
                ALYSLC.Log("[SUMMON SCRIPT] Waiting for " + PlayerActor.GetDisplayName() + " to be dismissed before summoning players. Is loaded: " + Player3DLoaded + ", is dead: " + PlayerIsDead + ".")
                ; Ensure player is alive.
                If (PlayerIsDead)
                    PlayerActor.Resurrect()
                EndIf

                ; Call dismissal function.
                ; May already be running here, but acts as a layer of redundancy to ensure the player is dismissed.
                __ALYSLC_ControlCoopActor CoopActor = PlayerActor as __ALYSLC_ControlCoopActor
                If (CoopActor)
                    ALYSLC.Log("[SUMMON SCRIPT] Sending " + CoopActor.GetDisplayName() + " home.")
                    CoopActor.CompletedLoad = False
                    CoopActor.SendCoopPlayerHome()
                EndIf

                Utility.Wait(1.0)
            Else
                Index += 1
            EndIf
        EndIf
    EndWhile

    ; Game settings to change for co-op.
    ; Set the minimum jump fall height for NPCs (default 450.0)
    ; to be the same as player 1's
    Game.SetGameSettingFloat("fJumpFallHeightMinNPC", 600.000000)
    ; Prevent co-op companion player actors from healing faster than the player outside of combat.
    Game.SetGameSettingFloat("fEssentialNonCombatHealRateBonus", 0.000000)
    ; The following seems to speed up recognition of changes
    ; to package swaps and procedure data changes made via plugin.
    ; Default 15.000000 (ms?).
    Game.SetGameSettingFloat("fEvaluatePackageTimer", 1.000000)
    ; Default 3.000000 (ms?)
    Game.SetGameSettingFloat("fEvaluateProcedureTimer", 1.000000)
    
    ; Update serialized companion player FIDs before showing the summoning menu.
    ALYSLC.UpdateAllSerializedCompanionPlayerFIDKeys()

    ; Select companion(s).
    WasSelectingCharacter = False
    ShouldExit = False
    ; First, get a list of active controllers; then show the character selection menu.
    ControllerIDs = ALYSLC.GetConnectedCoopControllerIDs()
    StorageUtil.SetIntValue(None, "ALYSLC_CoopControllerCount", ControllerIDs.Length - 1)
    StorageUtil.SetIntValue(None, "ALYSLC_NumCompanions", 0)
    CoopCompanionControllersCount = ControllerIDs.Length - 1
    RegisterPlayersForCoopEvents()
    ShowCoopSetupMenu()
    
    ; If the number of selected companions is zero, end the co-op session.
    ; Otherwise, summon the characters and signal player 1's reference
    ; alias scripts to initialize.
    If (StorageUtil.GetIntValue(None, "ALYSLC_NumCompanions", 0) == 0)
        ; End co-op for player 1.
        EndCoopForPlayer(PlayerRef)
        ALYSLC.RequestMenuControl(-1, "MessageBoxMenu")
        CoopIsSummoningPlayers.SetValue(0)
        Return
    EndIf

    ; Get active players and package start indices before initializing co-op session data in the plugin.
    SetActiveCoopPlayers()
    Int[] PackageFormListIndicesList = GetPackageFormListIndicesList(CoopActors)
    ; P1 is not always valid for some reason.
    Bool Success = CoopActors[0] == Game.GetPlayer()
    If (!Success)
        CoopActors[0] = Game.GetPlayer() as Actor
        Success = CoopActors[0] == Game.GetPlayer()
        ALYSLC.Log("[SUMMON SCRIPT] ERR: P1 not valid. Attempting to set P1 again. Set as " + CoopActors[0])
    EndIf

    If (Success)
        Success = ALYSLC.InitializeCoop(CurrentCompanionsCount, ControllerIDs, CoopActors, PackageFormListIndicesList)
    EndIf

    ; Initialization failed. Do not continue.
    If (!Success)
        ALYSLC.Log("[SUMMON SCRIPT] ERR: Initialization failed. Stopping summoning process.")
        StorageUtil.FormListClear(None, "ALYSLC_CompanionsList")
        StorageUtil.SetIntValue(None, "ALYSLC_NumCompanions", 0)
        EndCoopForPlayer(PlayerRef)
        ALYSLC.RequestMenuControl(-1, "MessageBoxMenu")
        CoopIsSummoningPlayers.SetValue(0)
        Return
    EndIf

    ; Teleport co-op companions to P1.
    SummonCoopPlayers()

    ALYSLC.Log("[SUMMON SCRIPT] About to wait for all players to load in.")
    ; Wait until all players load in.
    While (Iter < CoopActors.Length)
        If (CoopActors[Iter] && !CoopActors[Iter].Is3DLoaded())
            ALYSLC.Log("[SUMMON SCRIPT] Waiting for " + CoopActors[Iter].GetDisplayName() + " to load in.")
            Utility.Wait(0.1)
        EndIf
        Iter += 1
    EndWhile

    ALYSLC.Log("[SUMMON SCRIPT] Send summoning requests to active players.")
    Player1CoopStart()
    CompanionCoopStart()

    Form[] CoopActorScripts = StorageUtil.FormListToArray(None, "ALYSLC_CompanionScripts")
    ; Failure if no scripts queued themselves.
    If (CoopActorScripts.Length == 0)
        ALYSLC.Log("[SUMMON SCRIPT] ERR: Failed to send co-op start event to co-op companion players(s). Please try re-summoning.")
        StorageUtil.FormListClear(None, "ALYSLC_CompanionsList")
        StorageUtil.SetIntValue(None, "ALYSLC_NumCompanions", 0)
        EndCoopForPlayer(PlayerRef)
        ALYSLC.RequestMenuControl(-1, "MessageBoxMenu")
        CoopIsSummoningPlayers.SetValue(0)
        Return
    EndIf
    
    ; Ensure all co-op companion scripts have checked in by adding themselves to the script list (1 per player but this could change).
    While (CoopActorScripts.Length < (StorageUtil.GetIntValue(None, "ALYSLC_NumCompanions", 0)))
        CoopActorScripts = StorageUtil.FormListToArray(None, "ALYSLC_CompanionScripts")
        ALYSLC.Log("[SUMMON SCRIPT] Waiting for scripts to check in. " + CoopActorScripts.Length + " loaded out of " + (StorageUtil.GetIntValue(None, "ALYSLC_NumCompanions", 0)))
        Utility.Wait(0.1)
    EndWhile

    ; Prevent players from taking damage until summoning is complete.
    ALYSLC.SetPartyInvincibility(True)
    
    ; Loop until all scripts notify this script that they have finished initialization.
    Bool CheckLoadStatus = True
    Int Iter = 0
    While (CheckLoadStatus)
        Iter = 0
        CheckLoadStatus = False
        While (Iter < CoopActorScripts.Length && !CheckLoadStatus)
            Form Script = CoopActorScripts[Iter]
            If (Script as __ALYSLC_ControlCoopActor)
                If (!(Script as __ALYSLC_ControlCoopActor).CompletedLoad)
                    ALYSLC.Log("[SUMMON SCRIPT] Waiting for CCA.")
                    CheckLoadStatus = True
                EndIf
            EndIf

            Iter += 1
        EndWhile

        ; Check player 1 load script separately since it is attached
        ; to a ReferenceAlias and cannot be added as a form to the scripts list.
        If (!(Player1RefrAlias as __ALYSLC_ControlPlayerRef).CompletedLoad)
            ALYSLC.Log("[SUMMON SCRIPT] Waiting for CP1R.")
            CheckLoadStatus = True
        EndIf
    EndWhile

    ; Signal that the co-op session has started.
    StorageUtil.SetIntValue(None, "ALYSLC_CoopStarted", 1)
    ALYSLC.ChangeCoopSessionState(True)
    ; Enable the co-op camera.
    ALYSLC.ToggleCoopCamera(True)
    ALYSLC.Log("[SUMMON SCRIPT] ALYSLC co-op session has started.")
    ; Pause for a bit to ensure the camera and plugin have
    ; finished setting up the session. Then remove invincibility.
    Utility.Wait(5.0)
    ALYSLC.SetPartyInvincibility(False)
    ALYSLC.Log("[SUMMON SCRIPT] Party invincibility grace period over.")

    ; Done summoning.
    CoopIsSummoningPlayers.SetValue(0)
    ALYSLC.Log("[SUMMON SCRIPT] Fulfilled summoning request. Have fun!")
EndEvent
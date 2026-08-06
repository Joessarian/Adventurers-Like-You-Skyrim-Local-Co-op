Scriptname __ALYSLC_CoopHelperMenuHandler extends ReferenceAlias  

Actor Property PlayerRef Auto
Actor Property PlayerInMenu Auto
FormList Property OtherActivePlayersList Auto
UIListMenu Property IdlesListMenu Auto
UISelectionMenu Property PlayerSelectionMenu Auto
UIStatsMenu Property CoopStatsMenu Auto
UIWheelMenu Property CoopIdlesAssignmentMenu Auto
UIWheelMenu Property CoopMiniGamesMenu Auto
; Device and player IDs for the player controlling menus.
Int Property PlayerInMenuDID Auto
Int Property PlayerInMenuPID Auto
Int Property IDLES_ASSIGNMENT_MENU = 0 AutoReadOnly
Int Property MINI_GAMES_MENU = 1 AutoReadOnly
Int Property STATS_MENU = 2 AutoReadOnly
Int Property TELEPORT_PLAYER_SELECTION_MENU = 3 AutoReadOnly
Int Property TRADE_PLAYER_SELECTION_MENU = 4 AutoReadOnly

Function SetupMenus(Int aiMenuType)
    ; Set up menus.
    If (aiMenuType == IDLES_ASSIGNMENT_MENU)
        CoopIdlesAssignmentMenu = UIExtensions.GetMenu("UIWheelMenu") as UIWheelMenu
        String[] SavedEmoteIdles = ALYSLC.GetFavoritedEmoteIdles(PlayerInMenuPID)
        Int Iter = 0
        While (Iter < SavedEmoteIdles.Length)
            CoopIdlesAssignmentMenu.SetPropertyIndexString("optionText", Iter, "Assign Emote Idle")
            CoopIdlesAssignmentMenu.SetPropertyIndexString("optionLabelText", Iter, SavedEmoteIdles[Iter])
            CoopIdlesAssignmentMenu.SetPropertyIndexBool("optionEnabled", Iter, True)
            Iter += 1
        EndWhile

        ; Idles Menu:
        ; Select idle to place in favorited idles wheel menu slot.
        String[] AllEmoteIdleEvents = ALYSLC.GetAllCyclableEmoteIdleEvents()
        IdlesListMenu = UIExtensions.GetMenu("UIListMenu") as UIListMenu
        Iter = 0
        While (Iter < AllEmoteIdleEvents.Length)
            IdlesListMenu.AddEntryItem(AllEmoteIdleEvents[Iter])
            Iter += 1
        EndWhile
    ElseIf (aiMenuType == MINI_GAMES_MENU)
        CoopMiniGamesMenu = UIExtensions.GetMenu("UIWheelMenu") as UIWheelMenu
        CoopMiniGamesMenu.SetPropertyIndexString("optionLabelText", 0, "Stewrim Ball")
        CoopMiniGamesMenu.SetPropertyIndexString("optionText", 0, "TODO: What's cooking? Cabbage?")
        CoopMiniGamesMenu.SetPropertyIndexString("optionLabelText", 1, "Flop-pin Bowling")
        CoopMiniGamesMenu.SetPropertyIndexString("optionText", 1, "TODO: Slip 'n slide for strikes")
        CoopMiniGamesMenu.SetPropertyIndexString("optionLabelText", 2, "Catch the Arrow")
        CoopMiniGamesMenu.SetPropertyIndexString("optionText", 2, "TODO: No milk-drinkers allowed")
        CoopMiniGamesMenu.SetPropertyIndexString("optionLabelText", 3, "Duel for Sweetrolls")
        CoopMiniGamesMenu.SetPropertyIndexString("optionText", 3, "TODO: Take them back")
        CoopMiniGamesMenu.SetPropertyIndexString("optionLabelText", 4, "Stop Mini Game")
        CoopMiniGamesMenu.SetPropertyIndexString("optionText", 4, "TODO: Ok, that's enough")
    
        CoopMiniGamesMenu.SetPropertyIndexBool("optionEnabled", 0, True)
        CoopMiniGamesMenu.SetPropertyIndexBool("optionEnabled", 1, True)
        CoopMiniGamesMenu.SetPropertyIndexBool("optionEnabled", 2, True)
        CoopMiniGamesMenu.SetPropertyIndexBool("optionEnabled", 3, True)
        CoopMiniGamesMenu.SetPropertyIndexBool("optionEnabled", 4, True)
    ElseIf (aiMenuType == STATS_MENU)
        CoopStatsMenu = UIExtensions.GetMenu("UIStatsMenu") as UIStatsMenu
    ElseIf (aiMenuType == TELEPORT_PLAYER_SELECTION_MENU || aiMenuType == TRADE_PLAYER_SELECTION_MENU)
        PlayerSelectionMenu = UIExtensions.GetMenu("UISelectionMenu") as UISelectionMenu
        SetAllOtherActivePlayers()
    EndIf
EndFunction

Function SetAllOtherActivePlayers()
    ALYSLC.Log("[CHMH SCRIPT] SetAllOtherActivePlayers: current size: " + OtherActivePlayersList.GetSize())
    OtherActivePlayersList.Revert()
    Int Iter = 0
    Int PreSize = OtherActivePlayersList.GetSize()
    Form[] Copy = OtherActivePlayersList.ToArray()
    While (Iter < PreSize)
        ALYSLC.Log("[CHMH SCRIPT] Removing " + Copy[Iter].GetName() + " from other active players form list.")
        OtherActivePlayersList.RemoveAddedForm(Copy[Iter])
        Iter += 1
    EndWhile

    If (PlayerInMenu == PlayerRef)
        OtherActivePlayersList.AddForms(StorageUtil.FormListToArray(None, "ALYSLC_CompanionsList")) 
        Iter = 0
        While (Iter < OtherActivePlayersList.GetSize())
            ALYSLC.Log("[CHMH SCRIPT]" + OtherActivePlayersList.GetAt(Iter).GetName() + " is on other active players form list.")
            Iter += 1
        EndWhile
    Else
        Form[] AllActiveCoopCompanions = StorageUtil.FormListToArray(None, "ALYSLC_CompanionsList")
        OtherActivePlayersList.AddForm(PlayerRef)
        Iter = 0
        While (Iter < AllActiveCoopCompanions.Length)
            If (AllActiveCoopCompanions[Iter] != PlayerInMenu)
                ALYSLC.Log("[CHMH SCRIPT] Adding " + AllActiveCoopCompanions[Iter].GetName() + " to other active players form list.")
                OtherActivePlayersList.AddForm(AllActiveCoopCompanions[Iter])
            EndIf
            Iter += 1
        EndWhile
    EndIf
    ALYSLC.Log("[CHMH SCRIPT] SetAllOtherActivePlayers: after size: " + OtherActivePlayersList.GetSize())
EndFunction

; Menu types are:
; 0 -> Idles Assignment
; 1 -> Mini Games
; 2 -> Stats
; 3 -> Teleport to Player
; 4 -> Trade with Player
Event OnCoopHelperMenuRequest(Actor akActorControllingMenu, Int aiMenuDID, Int aiMenuPID, Int aiMenuType)
    ALYSLC.Log("[CHMH SCRIPT] OnCoopHelperMenuRequest() Event Received. Type " + aiMenuType + ".")
	; Attempt to refresh P1 property if invalid for some reason. No idea what causes this to occur at times.
	Float SecsWaited = 0.0
	While (!PlayerRef && SecsWaited < 2.0)
		ALYSLC.LogError("[CHMH SCRIPT] P1 invalid; attempting to get P1 again.")
		PlayerRef = Game.GetPlayer()
		ALYSLC.Wait(0.5)
		SecsWaited += 0.5
	EndWhile
	
	If (PlayerRef != Game.GetPlayer())
		Debug.MessageBox("[ALYSLC]\nCritical Error: P1's actor is invalid. Cannot open helper menu.")
		ALYSLC.LogError("[CHMH SCRIPT] Critical Error: P1's actor is invalid. Cannot open helper menu. P1 actor set as " + PlayerRef + ", game player set as " + Game.GetPlayer())
		Return
	EndIf

    ; Set player controlling menu's info.
    PlayerInMenuDID = aiMenuDID
    PlayerInMenuPID = aiMenuPID
    PlayerInMenu = akActorControllingMenu
    ; Populate helper menu(s) with options.
    SetupMenus(aiMenuType)
    Bool IsP1 = PlayerInMenu == PlayerRef
    ; Idles
    If (aiMenuType == IDLES_ASSIGNMENT_MENU)
        ; Set player menu control.
        ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, CoopIdlesAssignmentMenu.ROOT_MENU)
        String[] SavedEmoteIdles = ALYSLC.GetFavoritedEmoteIdles(PlayerInMenuPID)
        Int FavoritesSlot = CoopIdlesAssignmentMenu.OpenMenu()
        ; Selected a valid slot, so open list of idles to place in the slot.
        While (FavoritesSlot != -1)
            ; Set player menu control.
            ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, IdlesListMenu.ROOT_MENU)
            IdlesListMenu.OpenMenu()
            String ChosenIdle = IdlesListMenu.GetResultString()
            ; Valid replacement event name chosen.
            If (ChosenIdle != "")
                ALYSLC.Log("[CHMH SCRIPT] Favorites slot " + FavoritesSlot + " with current idle " + SavedEmoteIdles[FavoritesSlot] + " is getting reassigned " + ChosenIdle + " as its saved idle.")
                CoopIdlesAssignmentMenu.SetPropertyIndexString("optionLabelText", FavoritesSlot, ChosenIdle)
                SavedEmoteIdles[FavoritesSlot] = ChosenIdle
                ; Send updated emote idles list.
                ALYSLC.SetFavoritedEmoteIdles(PlayerInMenuPID, SavedEmoteIdles)
            EndIf
            ; Re-open idles assignment menu.
            ; Set player menu control.
            ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, "ALYSLC Retain Menu Control")
            FavoritesSlot = CoopIdlesAssignmentMenu.OpenMenu()
        EndWhile
    ; !!!TODO!!! Mini Games
    ElseIf (aiMenuType == MINI_GAMES_MENU)
        ; Set player menu control.
        ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, CoopMiniGamesMenu.ROOT_MENU)
        Int OptionChosen = CoopMiniGamesMenu.OpenMenu(PlayerInMenu)
        ALYSLC.Log("[CHMH SCRIPT] TODO: " + PlayerInMenu.GetDisplayName() + " (PID " + PlayerInMenuPID + "): option chosen in mini games menu: " + OptionChosen)

        ; Stewrim Ball.
        ; Spawn a 'ball' and 'hoop' for now.
        If (OptionChosen == 0)
            Form HoopForm = Game.GetFormFromFile(0x12FE6, "Skyrim.esm")
            ObjectReference Hoop = None
            ObjectReference Ball = None
            If (HoopForm)
                Hoop = PlayerInMenu.PlaceAtMe(HoopForm, 1, False, True)
                ALYSLC.Wait(0.5)
                Hoop.MoveTo(PlayerInMenu, 0.0, 100.0, 0.0, False)
                Hoop.Enable()
            EndIf

            Form BallForm = Game.GetFormFromFile(0x64B3F, "Skyrim.esm")
            If (BallForm)
                Ball = PlayerInMenu.PlaceAtMe(BallForm, 1, False, True)
                ALYSLC.Wait(0.5)
                Ball.MoveTo(Hoop, 0.0, 0.0, 100.0)
                Ball.Enable()
            EndIf
        EndIf
    ; Stats
    ElseIf (aiMenuType == STATS_MENU)
        If (PlayerInMenu)
            ALYSLC.Log("[CHMH SCRIPT] Opening stats menu for " + PlayerInMenu.GetDisplayName() + ".")
            ; Set player menu control.
            ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, CoopStatsMenu.ROOT_MENU)
            CoopStatsMenu.OpenMenu(PlayerInMenu)
        EndIf
    ; Teleport to Player -or-
    ; Trade with Player
    ElseIf (aiMenuType == TELEPORT_PLAYER_SELECTION_MENU || aiMenuType == TRADE_PLAYER_SELECTION_MENU)
        If (PlayerInMenu)
            ; Only need to display submenu if there are more than 2 players.
            Bool DisplaySelectionMenu = OtherActivePlayersList.GetSize() > 1
            If (aiMenuType == TELEPORT_PLAYER_SELECTION_MENU)
                Actor TeleportTarget = None
                If (DisplaySelectionMenu)
                    ; Set player menu control.
                    ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, PlayerSelectionMenu.ROOT_MENU)
                    PlayerSelectionMenu.OpenMenu(OtherActivePlayersList)
                    TeleportTarget = PlayerSelectionMenu.GetResultForm() as Actor
                Else
                    TeleportTarget = OtherActivePlayersList.GetAt(0) as Actor
                EndIf

                If (TeleportTarget)
                    ALYSLC.Log("[CHMH SCRIPT] Teleporting " + PlayerInMenu.GetDisplayName() + " to " + TeleportTarget.GetDisplayName())
                    ALYSLC.TeleportPlayerToActor(PlayerInMenuPID, TeleportTarget)
                EndIf
            ElseIf (aiMenuType == TRADE_PLAYER_SELECTION_MENU)
                If (DisplaySelectionMenu)
                    ; Set player menu control.
                    ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, PlayerSelectionMenu.ROOT_MENU)
                    PlayerSelectionMenu.OpenMenu(OtherActivePlayersList)
                    Actor GifteePlayer = PlayerSelectionMenu.GetResultForm() as Actor
                    
                    If (GifteePlayer)
                        ; Set player menu control.
                        ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, "GiftMenu")
                        If (IsP1)
                            ALYSLC.Log("[CHMH SCRIPT] Gifting items from P1 " + PlayerRef.GetDisplayName() + " to " + GifteePlayer.GetDisplayName())
                            ; Gifts given directly to the giftee player from P1.
                            ALYSLC.SetGifteePlayerActor(GifteePlayer)
                            GifteePlayer.ShowGiftMenu(True, None, True, False)
                            ALYSLC.SetGifteePlayerActor(None)
                        Else
                            ALYSLC.Log("[CHMH SCRIPT] Gifting items from " + PlayerInMenu.GetDisplayName() + " to " + GifteePlayer.GetDisplayName() + " via P1.")
                            ; Gifts given to P1 and then transfered to the selected giftee player.
                            ; Inform plugin of giftee player.
                            ALYSLC.SetGifteePlayerActor(GifteePlayer)
                            GifteePlayer.ShowGiftMenu(True, None, True, False)
                            ;PlayerInMenu.ShowGiftMenu(False, None, True, False)
                            ; Clear giftee player once the menu closes.
                            ALYSLC.SetGifteePlayerActor(None)
                        EndIf
                    EndIf
                Else
                    If (IsP1)
                        Actor GifteePlayer = OtherActivePlayersList.GetAt(0) as Actor
                        If (GifteePlayer)
                            ALYSLC.Log("[CHMH SCRIPT] Gifting items from P1 " + PlayerRef.GetDisplayName() + " to " + GifteePlayer.GetDisplayName())
                            ; Set player menu control.
                            ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, "GiftMenu")
                            ALYSLC.SetGifteePlayerActor(GifteePlayer)
                            GifteePlayer.ShowGiftMenu(True, None, True, False)
                            ALYSLC.SetGifteePlayerActor(None)
                        EndIf
                    Else
                        ALYSLC.Log("[CHMH SCRIPT] Gifting items from " + PlayerInMenu.GetDisplayName() + " to P1 " + PlayerRef.GetDisplayName())
                        ; Set player menu control.
                        ALYSLC.RequestMenuControl(PlayerInMenuDID, PlayerInMenuPID, "GiftMenu")
                        ALYSLC.SetGifteePlayerActor(PlayerRef as Actor)
                        PlayerInMenu.ShowGiftMenu(True, None, True, False)
                        ALYSLC.SetGifteePlayerActor(None)
                    EndIf
                EndIf
            EndIf
        EndIf
    ; Invalid type
    Else
        ALYSLC.LogError("[CHMH SCRIPT] ERR: Helper menu type " + aiMenuType + " is invalid. Returning.")
        Return
    EndIf
EndEvent
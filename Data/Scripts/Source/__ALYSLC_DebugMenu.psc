Scriptname __ALYSLC_DebugMenu extends ReferenceAlias  
Actor Property PlayerRef Auto
Actor Property PlayerInMenu Auto
UIListMenu Property DebugMenu Auto
Int Property PlayerInMenuCID Auto
    
Event OnDebugMenuRequest(Actor akActorControllingMenu, Int aiMenuCID)
    ALYSLC.Log("[CDM SCRIPT] OnDebugMenuRequest() Event Received.")
	; Attempt to refresh P1 property if invalid for some reason. No idea what causes this to occur at times.
	Float SecsWaited = 0.0
	While (!PlayerRef && SecsWaited < 2.0)
		ALYSLC.Log("[CDM SCRIPT] P1 invalid; attempting to get P1 again.")
		PlayerRef = Game.GetPlayer() as Actor
		Utility.Wait(0.1)
		SecsWaited += 0.1
	EndWhile
	
	If (PlayerRef != Game.GetPlayer())
		Debug.MessageBox("[ALYSLC] Critical Error: P1's actor is invalid. Cannot open debug menu.")
		ALYSLC.Log("[CDM SCRIPT] Critical Error: P1's actor is invalid. Cannot open debug menu.")
		Return
	EndIf

    ; Set player controlling menu's info.
    PlayerInMenu = akActorControllingMenu
    PlayerInMenuCID = aiMenuCID
    ALYSLC.Log("[CDM SCRIPT] DEBUG: Actor in menu: " + PlayerInMenu.GetDisplayName() + ", menu CID: " + PlayerInMenuCID + ", player 1: " + PlayerRef.GetDisplayName())

    DebugMenu = UIExtensions.GetMenu("UIListMenu") as UIListMenu

    ; 0: Options that affect the entire co-op party.
    DebugMenu.AddEntryItem("Party Options", -1, -1, True)

    ; 1: Options that only affect the player controlling the menu.
    DebugMenu.AddEntryItem("Player Options", -1, -1, True)

    ; 2: Other options. Could be anything.
    DebugMenu.AddEntryItem("Miscellaneous Options", -1, -1, True)

    ; [Party]
    ; 3. Disable god mode for all players.
    DebugMenu.AddEntryItem("Disable God Mode", 0, -1, False)
    ; 4. Enable god mode for all players.
    ; Useful for debugging without having the co-op session come to an untimely end.
    ; Ok...
    ; I'm just bad at the game and tired of typing the same console commands on reload.
    DebugMenu.AddEntryItem("Enable God Mode", 0, -1, False)
    ; 5. Move all companion players to the menu-controlling player.
    ; Useful for times where a particular actor may be unloaded or unresponsive.
    DebugMenu.AddEntryItem("Move Players to Me", 0, -1, False)
    ; 6. Refresh all player managers' data.
    ; Useful when a player cannot move or perform actions after a
    ; co-op session starts.
    DebugMenu.AddEntryItem("Refresh Manager Data", 0, -1, False)
    ; 7. Stop combat all on all players.
    ; Useful for when a co-op player is stuck in combat.
    DebugMenu.AddEntryItem("Stop Combat", 0, -1, False)
    ; 8. Stop combat all on all players and remove all bounties.
    ; Note: Player 1 does not lose any gold, so this is a cheat option.
    DebugMenu.AddEntryItem("Stop Combat and Clear All Bounties", 0, -1, False)

    ; [Player]
    ; 9. Disable god mode for this player.
    DebugMenu.AddEntryItem("Disable God Mode", 1, -1, False)
    ; 10. Enable god mode for this player.
    ; Useful for debugging without having the co-op session come to an untimely end.
    ; Ok...
    ; I'm just bad at the game and tired of typing the same console commands on reload.
    DebugMenu.AddEntryItem("Enable God Mode", 1, -1, False)
    ; 11. Reset player to fix a couple equip state bugs:
    ; For co-op players, two bugs:
    ; 1. Jerky movement and cancelled attack animations with wonky hitboxes when swinging weapons.
    ; Source: not found.
    ; 2. Desired equip state and current equip state mismatches lead to loop of equip requests that
    ; freeze the player out of accessing all menus except for the debug menu.
    ; Source: bad equip code on my part in an attempt to handle the annoying, persistent auto-equipping for NPCs done by the game.
    ; For player 1 (when AI driven only):
    ; Highest damage weapon is auto equipped when equipping a spell or fails to unequip completely.
    ; Useful after players exhibit the above behaviors.
    ; A temporary band-aid solution until the bugs' causes are found.
    DebugMenu.AddEntryItem("Reset Player", 1, -1, False)
    ; 12. Re-equip items in this player's hand slots. 
    ; Useful for remedying some odd equip state bugs.
    DebugMenu.AddEntryItem("Re-equip Items in Hands", 1, -1, False)
    ; 13. Refresh this player's manager data.
    ; Useful when a player cannot move or perform actions after a
    ; co-op session starts.
    DebugMenu.AddEntryItem("Refresh Manager Data", 1, -1, False)
    ; 14. Reset this player's health/magicka/stamina actor values to their base values.
    ; Also remove all unlocked perks from this player and remove all shared perks from all active players.
    ; All removed perks have their perk points refunded.
    ; Allows this player to fully respec their character.
    DebugMenu.AddEntryItem("Respec Player (Skyrim Only)", 1, -1, False)

    ; [Misc]
    ; 15. Assign controller ID manually for player 1.
    ; Useful for when one controller is controlling multiple players or menus for multiple players or no players at all.
    ; Happens when Skyrim does not assign the controller with XInput index 0 (first controller) to player 1.
    ; Have yet to figure out how to get the XInput controller index for player 1 directly.
    DebugMenu.AddEntryItem("Assign Player 1 Controller ID", 2, -1, False)
    ; 16. Disable co-op cam, reset player 1 to default motion-driven state, reset controls, and remove any other
    ; temporary modifications made to player 1's state since the last co-op session started.
    DebugMenu.AddEntryItem("Reset Cam and Player 1", 2, -1, False)
    ; 17. Restart co-op camera.
    ; Useful for when camera begins to stutter or is stuck in place.
    DebugMenu.AddEntryItem("Restart Co-op Camera", 2, -1, False)
    ; 18. Stop menu input manager.
    ; Useful for when a co-op player still has control of any open menus when they should not.
    DebugMenu.AddEntryItem("Stop Menu Input Manager", 2, -1, False)

    ALYSLC.RequestMenuControl(PlayerInMenuCID, DebugMenu.ROOT_MENU)
    DebugMenu.OpenMenu()
    Int SelectedIndex = DebugMenu.GetResultInt()
    ; Re-open until exit menu bind is pressed.
    While (SelectedIndex != -1)
        If (SelectedIndex == 3)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: disable god mode for all players.")
            ALYSLC.DisableGodModeForAllCoopPlayers()
        ElseIf (SelectedIndex == 4)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: enable god mode for all players.")
            ALYSLC.EnableGodModeForAllCoopPlayers()
        ElseIf (SelectedIndex == 5)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: move all players to " + PlayerInMenu.GetName())
            ALYSLC.MoveAllPlayersToPlayer(PlayerInMenu)
        ElseIf (SelectedIndex == 6)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: stop, refresh, and restart player managers for all players.")
            ALYSLC.RefreshAllPlayerManagers()
        ElseIf (SelectedIndex == 7)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: stop combat on all players.")
            ALYSLC.StopAllCombatOnCoopPlayers(False)
        ElseIf (SelectedIndex == 8)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: stop combat on all players and clear bounties.")
            ALYSLC.StopAllCombatOnCoopPlayers(True)
        ElseIf (SelectedIndex == 9)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: disable god mode for "+ PlayerInMenu.GetDisplayName() + ".")
            If (PlayerInMenu)
                ALYSLC.DisableGodModeForPlayer(PlayerInMenuCID)
            EndIf
        ElseIf (SelectedIndex == 10)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: enable god mode for "+ PlayerInMenu.GetDisplayName() + ".")
            If (PlayerInMenu)
                ALYSLC.EnableGodModeForPlayer(PlayerInMenuCID)
            EndIf
        ElseIf (SelectedIndex == 11)
            If (PlayerInMenu && PlayerInMenu != PlayerRef && PlayerInMenuCID > -1)
                ALYSLC.Log("[CDM SCRIPT] DEBUG: resetting equip state for " + PlayerInMenu.GetDisplayName() + ".")
                ALYSLC.ResetCoopCompanion(PlayerInMenuCID, True, True)
            Else
                ALYSLC.Log("[CDM SCRIPT] DEBUG: resetting bugged equip state for P1 " + PlayerInMenu.GetDisplayName() + ".")
                ALYSLC.ResetPlayer1State()
            EndIf
        ElseIf (SelectedIndex == 12)
            If (PlayerInMenu)
                ALYSLC.Log("[CDM SCRIPT] DEBUG: re-equipping forms in " + PlayerInMenu.GetDisplayName() + "'s hands.")
                ALYSLC.ReEquipHandForms(PlayerInMenuCID)
            EndIf
        ElseIf (SelectedIndex == 13)
            If (PlayerInMenu)
                ALYSLC.Log("[CDM SCRIPT] DEBUG: stop, refresh, and restart player managers for " + PlayerInMenu.GetDisplayName() + ".")
                ALYSLC.RefreshPlayerManagers(PlayerInMenuCID)
            EndIf
        ElseIf (SelectedIndex == 14)
            If (PlayerInMenu)
                ALYSLC.Log("[CDM SCRIPT] DEBUG: respec " + PlayerInMenu.GetDisplayName() + " and remove all unlocked shared perks from all players.")
                ALYSLC.RespecPlayer(PlayerInMenuCID)
                ; Have to wait for message box prompt to open.
                SecsWaited = 0.0
                While (!UI.IsMenuOpen("MessageBoxMenu") && SecsWaited < 2.0)
                    Utility.Wait(0.1)
                    SecsWaited += 0.1
                EndWhile
    
                ; Once open, wait until closed.
                While (UI.IsMenuOpen("MessageBoxMenu"))
                    Utility.WaitMenuMode(0.1)
                EndWhile
            EndIf
        ElseIf (SelectedIndex == 15)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: assign player 1 CID.")
            ALYSLC.AssignPlayer1CID()
            ; Have to wait for message box prompt to open.
            SecsWaited = 0.0
            While (!UI.IsMenuOpen("MessageBoxMenu") && SecsWaited < 2.0)
                Utility.Wait(0.1)
                SecsWaited += 0.1
            EndWhile

            ; Once open, wait until closed.
            While (UI.IsMenuOpen("MessageBoxMenu"))
                Utility.WaitMenuMode(0.1)
            EndWhile
        ElseIf (SelectedIndex == 16)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: reset cam, player 1 controls, and state.")
            ALYSLC.ResetPlayer1AndCamera()
            Utility.Wait(1.0)
            Game.SetPlayerAIDriven(False)
            Game.EnablePlayerControls()
            PlayerRef.SetHeadTracking(False)
            PlayerRef.ClearLookAt()
            PlayerRef.ClearKeepOffsetFromActor()
            Game.ForceFirstPerson()
            Utility.Wait(1.0)
            Game.ForceThirdPerson()
            Game.SetCameraTarget(PlayerRef)
        ElseIf (SelectedIndex == 17)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: restart co-op camera.")
            ALYSLC.RestartCoopCamera()
        ElseIf (SelectedIndex == 18)
            ALYSLC.Log("[CDM SCRIPT] DEBUG: stop menu input manager.")
            ALYSLC.StopMenuInputManager()
        EndIf

        ; Re-open.
        ALYSLC.RequestMenuControl(PlayerInMenuCID, DebugMenu.ROOT_MENU)
        DebugMenu.OpenMenu()
        SelectedIndex = DebugMenu.GetResultInt()
    EndWhile
EndEvent
#include "MenuInputManager.h"
#include <mutex>
#include <thread>
#include <Compatibility.h>
#include <Controller.h>
#include <GlobalCoopData.h>

namespace ALYSLC 
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	using EventResult = RE::BSEventNotifyControl;

	MenuInputManager::MenuInputManager() 
		: Manager(ManagerType::kMIM)
	{
		// Device and player IDs.
		managerMenuDID = -1;
		managerMenuPID = -1;
		// Default to P1's PID because P1 is given control of menus
		// as a fallback if no companion player requested control.
		pmcPID = 0;

		// Handles for open container, source container, and co-op player actors.
		fromContainerHandle = RE::ObjectRefHandle();
		menuContainerHandle = RE::ObjectRefHandle();
		menuCoopActorHandle = RE::ActorHandle();
		gifteePlayerHandle = RE::ActorHandle();
		// Extra data for selected entry in menu.
		selectedExDataList = nullptr;
		// Form selected in menu.
		selectedForm = nullptr;
		// Equip index. Defaults to right hand.
		reqEquipIndex = EquipIndex::kRightHand;

		// Event type to handle next.
		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		// Opened menu info.
		openedMenuType = SupportedMenu::kDefault;
		menuName = "";

		// Ints
		managedCoopMenusCount = 0;
		// Bools
		dropBindPressed = false;
		inventoryChestOpen = false;
		isShowingInventory = false;
		placeholderMagicChanged = false;
		shouldDropItem.store(false);
		shouldRefreshMenu = false;
		shouldReloadMenuEntries = false;
		spellFavoriteStatusChanged = false;

		// Player menu control outline overlay alpha value.
		pmcFadeInterpData = std::make_unique<TwoWayInterpData>();
		pmcFadeInterpData->SetInterpInterval(1.0f, true);
		pmcFadeInterpData->SetInterpInterval(1.0f, false);

		// Clear pairs, maps, sets, vectors.
		dropReqPair = { nullptr, 0 };
		favMenuIndexToEntryMap.clear();
		favEntryEquipStates.clear();
		magEntryEquipStates.clear();
		magFormsList.clear();
		menuNamesHashSet.clear();
		menuControlMap.clear();
		// Current bind info.
		currentBindInfo = MenuBindInfo();

		// Control map
		controlMap = RE::ControlMap::GetSingleton();

		// Menus
		barterMenu = nullptr;
		bookMenu = nullptr;
		containerMenu = nullptr;
		dialogueMenu = nullptr;
		favoritesMenu = nullptr;
		giftMenu = nullptr;
		inventoryMenu = nullptr;
		journalMenu = nullptr;
		lockpickingMenu = nullptr;
		magicMenu = nullptr;
		mapMenu = nullptr;
		sleepWaitMenu = nullptr;
	}
	
	void MenuInputManager::MainTask()
	{
		if (newMenuAtopStack)
		{
			// Set menu control map and refresh data when a new menu is opened.
			SetMenuControlMap();
			SetMenuInputMappings();
			RefreshData();

			{
				std::unique_lock<std::mutex> lock(openedMenuMutex, std::try_to_lock);
				if (lock)
				{
					DBG
					(
						"Lock acquired and data updated (0x{:X}). "
						"Setting new menu opened flag to false.", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
					newMenuAtopStack = false;
				}
				else
				{
					DBG
					(
						"Could not acquire lock after updating data (0x{:X}). "
						"Better luck next time.", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
				}
			}
		}

		// Update controller state and event type to handle, if any.
		CheckControllerInput();

		// Update equip state when signalled to externally.
		if (equipEventRefreshReq) 
		{
			if (openedMenuType == SupportedMenu::kMagic)
			{
				shouldRefreshMenu = true;
			}
			else if (openedMenuType == SupportedMenu::kFavorites)
			{
				RefreshFavoritesMenuEquipState();
				shouldRefreshMenu = true;
			}

			{
				std::unique_lock<std::mutex> lock(equipEventMutex, std::try_to_lock);
				if (lock)
				{
					DBG
					(
						"Lock acquired and data updated (0x{:X}). "
						"Resetting refresh equip state flag from {}, to false.",
						std::hash<std::jthread::id>()(std::this_thread::get_id()), 
						equipEventRefreshReq
					);

					// Equip state refresh request fired before delayed equip refresh request, 
					// so we can clear the delayed one.
					equipEventRefreshReq = false;
					shouldRefreshMenu = true;
				}
				else
				{
					DBG
					(
						"Could not acquire lock after updating data (0x{:X}). "
						"Better luck next time.",
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
				}
			}
		}
		
		// Update whether or not the player is viewing their inventory via the Container Menu
		// or Barter Menu before potentially refreshing the menu.
		UpdateShowingInventoryFlags();

		// Refresh menu after sending event.
		if (shouldRefreshMenu)
		{
			RefreshMenu();
		}
	}

	void MenuInputManager::PrePauseTask()
	{
		DBG("PrePauseTask.");

		// Update favorited physical forms if the player was in their inventory.
		// Since the player may have (un)favorited new forms while in their inventory, 
		// update favorited form data.
		// No need to update magic favorites because there's no way 
		// of modifying them through the container menu.
		if (managerMenuPID != -1 && inventoryChestOpen)
		{
			glob.coopPlayers[managerMenuPID]->em->UpdateFavoritedFormsLists(true);
		}

		// Release all inputs if menus closed when input(s) were being held.
		for (auto& [xMask, info] : menuControlMap)
		{
			if (info.eventType == MenuInputEventType::kEmulateInput)
			{
				if (xMask != XMASK_LS && xMask != XMASK_RS)
				{
					// Release button.
					std::unique_ptr<RE::InputEvent* const> buttonEvent = 
					(
						std::make_unique<RE::InputEvent* const>
						(
							RE::ButtonEvent::Create
							(
								info.device, info.eventName, info.idCode, 0.0f, info.heldTimeSecs
							)
						)
					);
					// Set pad to indicate that the event should be ignored.
					(*buttonEvent.get())->AsIDEvent()->pad24 = 0xDEAD;
					Util::SendInputEvent(buttonEvent);
				}
				else
				{
					// Center LS and RS.
					auto thumbstickEvent = std::make_unique<RE::InputEvent* const>
					(
						Util::CreateThumbstickEvent(info.eventName, 0.0f, 0.0f, xMask == XMASK_LS)
					);
					// Set pad to indicate that the event should be ignored.
					(*thumbstickEvent)->AsIDEvent()->pad24 = 0xDEAD;
					Util::SendInputEvent(thumbstickEvent);
				}
			}

			info.eventType = MenuInputEventType::kReleasedNoEvent;
			info.value = 0.0f;
			info.heldTimeSecs = 0.0f;
		}

		// Clear out opened menus set and stack.
		managedCoopMenusCount = 0;
		menuNamesHashSet.clear();
		menuNamesStack.clear();

		// Reset menu control DID/PID before pausing.
		managerMenuDID = -1;
		managerMenuPID = -1;
	}

	void MenuInputManager::PreStartTask()
	{
		return;
	}

	void MenuInputManager::RefreshData()
	{
		// Refresh all menu-related data.

		if (managerMenuDID < 0) 
		{
			DBG("Got invalid device ID ({}).", managerMenuDID);
			return;
		}

		// Get companion player's handle if in co-op.
		if (glob.coopSessionActive)
		{
			menuCoopActorHandle = glob.coopPlayers[managerMenuPID]->coopActor->GetHandle();
		}

		// Reset general menu data.
		currentMenuInputEventType = MenuInputEventType::kReleasedNoEvent;
		reqEquipIndex = EquipIndex::kRightHand;
		dropBindPressed = false;
		equipEventRefreshReq = false;
		inventoryChestOpen = false;
		isShowingInventory = false;
		placeholderMagicChanged = false;
		shouldRefreshMenu = false;
		shouldReloadMenuEntries = false;
		spellFavoriteStatusChanged = false;
		fromContainerHandle = RE::ObjectRefHandle();
		menuContainerHandle = RE::ObjectRefHandle();
		selectedExDataList = nullptr;
		selectedForm = nullptr;
		lastEquipStateRefreshReqTP = SteadyClock::now();

		// Initialize menu-specific data.
		if (containerMenu)
		{
			// Get the container reference and check if it is the co-op player.
			// If so, the co-op player is accessing their inventory,
			// and the handling of certain button presses changes:
			// e.g. the "X" button does not take all but instead drops the selected item.
			RE::NiPointer<RE::TESObjectREFR> containerRefrPtr{ };
			bool succ = RE::TESObjectREFR::LookupByHandle
			(
				RE::ContainerMenu::GetTargetRefHandle(), containerRefrPtr
			);
			menuContainerHandle = 
			(
				containerRefrPtr ? 
				containerRefrPtr->GetHandle() :
				RE::ObjectRefHandle()
			);
			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(menuCoopActorHandle);
			inventoryChestOpen = 
			(
				glob.coopSessionActive &&
				containerMenu->GetContainerMode() == RE::ContainerMenu::ContainerMode::kNPCMode && 
				pIndex != -1 && 
				Util::HandleIsValid(menuContainerHandle) &&
				containerRefrPtr == glob.coopPlayers[pIndex]->em->inventoryChest	
			);
			isShowingInventory = inventoryChestOpen;
			dropReqPair = { nullptr, 0 };
			RefreshMenu();
		}
		else if (giftMenu)
		{
			// Reload entries after importing companion player's inventory.
			inventoryChestOpen = false;
			isShowingInventory = true;
			shouldReloadMenuEntries = true;
			RefreshMenu();
		}
		else if (favoritesMenu)
		{
			// Set initial equip states for favorited items.
			InitFavoritesEntries();
			// Refresh menu after initializing equip states/favorited indices.
			RefreshMenu();
		}
		else if (magicMenu)
		{
			// Set the initial magic item equip states.
			InitMagicMenuEquippedStates();
			// Refresh menu after initializing equip states/favorited indices.
			RefreshMenu();
		}

		// Set controlmap initial input states so that inputs already held as the menu opens
		// do not trigger any input events until they are released and pressed again.
		XINPUT_STATE buttonState{ };
		ZeroMemory(&buttonState, sizeof(buttonState));
		if (XInputGetState(glob.coopPlayers[managerMenuPID]->deviceID, &buttonState) == 
			ERROR_SUCCESS)
		{
			for (auto iter = menuControlMap.begin(); iter != menuControlMap.end(); ++iter)
			{
				if ((buttonState.Gamepad.wButtons & iter->first) == 0)
				{
					continue;
				}

				DBG("0x{:X} was held when starting the MIM.", iter->first);
				// Set as pressed without emulating input event to prevent previous event type 
				// from triggering.
				auto& bindInfo = menuControlMap[iter->first];
				bindInfo.value = 1.0f;
				bindInfo.firstPressTP = SteadyClock::now();
				bindInfo.eventType = MenuInputEventType::kPressedNoEvent;
			}
		}
	}

	const ManagerState MenuInputManager::ShouldSelfPause()
	{
		// Wait until data is refreshed if the player is loading a save.
		if (glob.loadingASave)
		{
			return ManagerState::kAwaitingRefresh;		
		}

		// Pause self if the menu controller's state is inaccessible.
		XINPUT_STATE buttonState{ };
		ZeroMemory(&buttonState, sizeof(buttonState));
		auto err = XInputGetState(managerMenuDID, &buttonState);
		if (managerMenuDID < 0 || err != ERROR_SUCCESS)
		{
			// Leave error message before returning.
			if (err != ERROR_SUCCESS && managerMenuDID != -1)
			{
				DBG
				(
					"Could not get XINPUT state for device ID {}. Pausing menu input manager.", 
					managerMenuDID
				);
			}

			return ManagerState::kPaused;
		}

		// Switch to P1 control if the current active container menu tab is P1's inventory.
		// NOTE: 
		// If calling Invoke() here instead of in a UI task causes crashes,
		// switch to running the below code in a UI task.
		// Right now, we need the result straight away in order to pause the MIM as necessary.
		auto ui = RE::UI::GetSingleton(); 
		if (!ui)
		{
			return currentState;
		}

		containerMenu = ui->GetMenu<RE::ContainerMenu>(); 
		if (!containerMenu)
		{
			return currentState;
		}

		auto view = containerMenu->uiMovie; 
		if (!view)
		{
			return currentState;
		}

		RE::GFxValue result{ };
		view->Invoke("_root.Menu_mc.isViewingContainer", std::addressof(result), nullptr, 0);
		// Viewing a container, not P1's inventory.
		if (bool isViewingContainer = result.GetBool(); isViewingContainer) 
		{
			return currentState;
		}
		
		// Is viewing P1's inventory from container.
		RE::NiPointer<RE::TESObjectREFR> containerRefr{ };
		RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle(), containerRefr);
		// Container is not a companion player's inventory chest.
		if (!GlobalCoopData::IsCoopPlayerInventoryChest(containerRefr)) 
		{
			return currentState;
		}
		
		isShowingInventory = false;
		// Restore P1's data first.
		DBG("Switching tabs over to P1's inventory.");
		GlobalCoopData::CopyOverCoopPlayerData
		(
			false, RE::ContainerMenu::MENU_NAME, menuCoopActorHandle
		);
		// The container is a co-op companion's inventory and the tab has been switched
		// to display P1's inventory, so we pause here and given P1 control.
		GlobalCoopData::SetMenuPlayerIDs(0);
		// Have to find out how the game reloads entries
		// (not you 'ItemList::Update()', you're too laggy).
		// Until then, manually set when switching over.
		UpdateMenuEntryEquipStates(true, true);
		return ManagerState::kPaused;
	}

	const ManagerState MenuInputManager::ShouldSelfResume()
	{
		// Switch back to co-op companion control 
		// if the current active container menu tab is their inventory.
		auto taskInterface = SKSE::GetTaskInterface(); 
		if (!taskInterface)
		{
			return currentState;
		}

		auto ui = RE::UI::GetSingleton(); 
		if (!ui)
		{
			return currentState;
		}

		containerMenu = ui->GetMenu<RE::ContainerMenu>(); 
		if (!containerMenu)
		{
			return currentState;
		}

		auto view = containerMenu->uiMovie; 
		if (!view)
		{
			return currentState;
		}

		RE::GFxValue result{ };
		view->Invoke("_root.Menu_mc.isViewingContainer", std::addressof(result), nullptr, 0);
		bool isViewingContainer = result.GetBool(); 
		// Viewing P1's inventory, so do not resume.
		if (!isViewingContainer)
		{
			return currentState;
		}
		
		// Is viewing a container and not P1's inventory.
		RE::NiPointer<RE::TESObjectREFR> containerRefr{ };
		RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle(), containerRefr);
		auto pIndex = GlobalCoopData::GetCoopPlayerIndexFromChest(containerRefr); 
		// The container is not a companion player's inventory chest.
		if (pIndex == -1) 
		{
			return currentState;
		}
		
		// The container is a companion player's inventory, so we should resume here
		// after giving the player control of menus.
		isShowingInventory = true;
		managerMenuPID = pIndex;
		managerMenuDID = glob.coopPlayers[managerMenuPID]->deviceID;
		// Import companion player's data first.
		DBG("Switching tabs back to {}'s inventory.", 
			glob.coopPlayers[managerMenuPID]->coopActor->GetName());
		GlobalCoopData::CopyOverCoopPlayerData
		(
			true, 
			RE::ContainerMenu::MENU_NAME, 
			glob.coopPlayers[managerMenuPID]->coopActor->GetHandle()
		);
		GlobalCoopData::SetMenuPlayerIDs(managerMenuPID);
		// Add the Container Menu back to the stack of handled menus, 
		// since it was removed when the MIM paused earlier.
		SetOpenedMenu(RE::ContainerMenu::MENU_NAME, true);
		// Re-apply companion player's equip state with an entry list refresh.
		UpdateMenuEntryEquipStates(true, false);
		return ManagerState::kRunning;
	}

	void MenuInputManager::CheckControllerInput()
	{
		// Update controller input state and set menu event type to handle.
		
		if (managerMenuDID < 0 || managerMenuDID >= ALYSLC_MAX_CONTROLLER_COUNT)
		{
			return;
		}

		auto& paInfo = glob.paInfoHolder;
		XINPUT_STATE buttonState{ };
		ZeroMemory(&buttonState, sizeof(buttonState));
		auto err = XInputGetState(managerMenuDID, &buttonState);
		if (err != ERROR_SUCCESS)
		{
			return;
		}

		// Hardcoded deadzone equal to half of the trigger's depressible range.
		const BYTE triggerDeadzone = UCHAR_MAX / 2;
		// Check for button state changes.
		for (auto& [xMask, bindInfo] : menuControlMap) 
		{
			// Three separate input types (triggers, buttons, analog sticks).
			bool handleTriggerPress = 
			(
				(xMask == XMASK_LT && buttonState.Gamepad.bLeftTrigger > triggerDeadzone) ||
				(xMask == XMASK_RT && buttonState.Gamepad.bRightTrigger > triggerDeadzone)
			);
			bool handleButtonPress = 
			(
				(!handleTriggerPress) && (xMask & buttonState.Gamepad.wButtons)
			);
			bool handleAnalogStickMovement = false;
			bool shouldCheckAnalogStick = (xMask == XMASK_LS || xMask == XMASK_RS);
			if (shouldCheckAnalogStick)
			{
				handleAnalogStickMovement = 
				(
					glob.cdh->GetAnalogStickState(managerMenuDID, xMask == XMASK_LS).normMag > 0.0f
				);
			}

			// Button/trigger pressed or analog stick moved.
			if (handleButtonPress || handleTriggerPress || handleAnalogStickMovement)
			{
				bool justPressed = bindInfo.value == 0.0f;
				if (justPressed)
				{
					// Set as just pressed.
					bindInfo.value = 1.0f;
					bindInfo.firstPressTP = SteadyClock::now();
				}

				// Update held time.
				bindInfo.heldTimeSecs = Util::GetElapsedSeconds(bindInfo.firstPressTP);
				// Special case (on hold):
				// Preview the hotkey to set for the selected Favorites Menu entry.
				if (openedMenuType == SupportedMenu::kFavorites && 
					xMask == XINPUT_GAMEPAD_LEFT_THUMB)
				{
					DBG("Update hotkey preview index");
					HotkeyFavoritedForm(false);
				}

				if (SetEmulatedInputEventInfo(xMask, bindInfo))
				{
					// Update menu event to send if just pressed.
					if (justPressed) 
					{
						// Default to no event.
						currentMenuInputEventType = MenuInputEventType::kReleasedNoEvent;
						if (handleButtonPress || handleTriggerPress)
						{
							switch (openedMenuType)
							{
							case SupportedMenu::kBarter:
							{
								ProcessBarterMenuButtonInput(bindInfo.eventName);
								break;
							}
							case SupportedMenu::kBook:
							{
								ProcessBookMenuButtonInput(bindInfo.eventName);
								break;
							}
							case SupportedMenu::kContainer:
							{
								ProcessContainerMenuButtonInput(bindInfo.eventName);
								break;
							}
							case SupportedMenu::kDialogue:
							{
								ProcessDialogueMenuButtonInput(bindInfo.eventName);
								break;
							}
							case SupportedMenu::kFavorites:
							{
								ProcessFavoritesMenuButtonInput(xMask, bindInfo.eventName);
								break;
							}
							case SupportedMenu::kGift:
							{
								ProcessGiftMenuButtonInput(bindInfo.eventName);
								break;
							}
							case SupportedMenu::kInventory:
							{
								ProcessInventoryMenuButtonInput(bindInfo.eventName);
								break;
							}
							case SupportedMenu::kLoot:
							{
								ProcessLootMenuButtonInput(xMask, bindInfo.eventName);
								break;
							}
							case SupportedMenu::kMagic:
							{
								ProcessMagicMenuButtonInput(bindInfo.eventName);
								break;
							}
							default:
							{
								break;
							}
							}
						}

						// Event type to send was not changed while processing above, 
						// so emulate input.
						if (currentMenuInputEventType == MenuInputEventType::kReleasedNoEvent) 
						{
							currentMenuInputEventType = MenuInputEventType::kEmulateInput;
						}
					}
					else if (bindInfo.eventType == MenuInputEventType::kEmulateInput)
					{
						// Continue to send input events while the button/trigger is held
						// or the analog stick is not centered.
						currentMenuInputEventType = MenuInputEventType::kEmulateInput;
					}
				}
				else 
				{
					// If unable to set bind info, set event type to none.
					currentMenuInputEventType = MenuInputEventType::kReleasedNoEvent;
				}

				if (currentMenuInputEventType == MenuInputEventType::kEmulateInput)
				{
					if (handleAnalogStickMovement) 
					{
						const auto& stickData = glob.cdh->GetAnalogStickState
						(
							managerMenuDID, xMask == XMASK_LS
						);
						const auto& xComp = stickData.xComp;
						const auto& yComp = stickData.yComp;
						const auto& stickMag = stickData.normMag;
						// Enqueue thumbstick event.
						auto thumbstickEvent = std::make_unique<RE::InputEvent* const>
						(
							Util::CreateThumbstickEvent
							(
								bindInfo.eventName,
								xComp * stickMag, 
								yComp * stickMag,
								xMask == XMASK_LS
							)
						);
						// Set pad to indicate that the co-op player sent the input, not P1.
						(*thumbstickEvent)->AsIDEvent()->pad24 = 0xCA11;
						queuedInputEvents.emplace_back(std::move(thumbstickEvent));
					}
					else
					{
						// Enqueue button input event.
						auto buttonEvent = std::make_unique<RE::InputEvent* const>
						(
							RE::ButtonEvent::Create
							(
								bindInfo.device, 
								bindInfo.eventName, 
								bindInfo.idCode, 
								bindInfo.value, 
								bindInfo.heldTimeSecs
							)
						);
						// Set pad to indicate that the co-op player sent the input, not P1.
						(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
						queuedInputEvents.emplace_back(std::move(buttonEvent));
					}
				}

				// Update linked event type for this binding.
				// Stored for handling later on release.
				menuControlMap[xMask].eventType = currentMenuInputEventType;
				currentBindInfo = menuControlMap[xMask];
				// Handle the resolved event type.
				HandleMenuEvent();
			}
			else
			{
				// Analog stick centered or button/trigger just released.
				// Update press value on release.
				if (bindInfo.value == 1.0f)
				{
					bindInfo.value = 0.0f;
					// Special case:
					// Set the previously previewed hotkey for the selected Favorites Menu entry.
					if ((openedMenuType == SupportedMenu::kFavorites) && 
						(xMask == XINPUT_GAMEPAD_LEFT_THUMB || xMask == XMASK_RS))
					{
						DBG("Set hotkey index: {}", xMask);
						HotkeyFavoritedForm(true);
					}
				}

				// Only send release event if an event was handled before.
				if (bindInfo.eventType != MenuInputEventType::kReleasedNoEvent)
				{
					// If unable to set bind info, set event type to none.
					if (!SetEmulatedInputEventInfo(xMask, bindInfo))
					{
						currentMenuInputEventType = MenuInputEventType::kReleasedNoEvent;
					}
					else
					{
						if (bindInfo.eventType == MenuInputEventType::kEmulateInput)
						{
							// Send button released/analog stick centered event.
							currentMenuInputEventType = MenuInputEventType::kEmulateInput;
							if (shouldCheckAnalogStick)
							{
								// Enqueue thumbstick event.
								auto thumbstickEvent = std::make_unique<RE::InputEvent* const>
								(
									Util::CreateThumbstickEvent
									(
										bindInfo.eventName, 0.0f, 0.0f, xMask == XMASK_LS
									)
								);
								// Set pad to indicate that the co-op player sent the input,
								// not P1.
								(*thumbstickEvent)->AsIDEvent()->pad24 = 0xCA11;
								queuedInputEvents.emplace_back(std::move(thumbstickEvent));
							}
							else
							{
								// Enqueue button input event.
								auto buttonEvent = std::make_unique<RE::InputEvent* const>
								(
									RE::ButtonEvent::Create
									(
										bindInfo.device,
										bindInfo.eventName,
										bindInfo.idCode,
										bindInfo.value, 
										bindInfo.heldTimeSecs
									)
								);
								// Set pad to indicate that the co-op player sent the input,
								// not P1.
								(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
								queuedInputEvents.emplace_back(std::move(buttonEvent));
							}
						}
						else
						{
							// No P1 emulation event on release is tied to this bind.
							// Nothing to send.
							currentMenuInputEventType = MenuInputEventType::kReleasedNoEvent;
						}
					}

					// Reset event tied to this bind to indicate that the button was released.
					menuControlMap[xMask].eventType = MenuInputEventType::kReleasedNoEvent;
					currentBindInfo = menuControlMap[xMask];
					// Handle menu event, if any, on release.
					HandleMenuEvent();
				}
			}
		}
	}

	void MenuInputManager::DebugPrintMenuBinds()
	{
		// Print out all binds (event name -> input ID code) per context and for each device.

		if (!controlMap || !controlMap->controlMap)
		{
			return;
		}

		auto inputContextLists = controlMap->controlMap;
		auto inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kBook];
		if (!inputContext)
		{
			return;
		}
		
		DBG("==========================================================================");
		DBG("++++++++++++++++++++++++++++++++++BOOK++++++++++++++++++++++++++++++++++++");
		DBG("---------------------------------Gamepad----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("---------------------------------Keyboard---------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("---------------------------------Mouse------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kConsole];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("++++++++++++++++++++++++++++++++CONSOLE+++++++++++++++++++++++++++++++++++");
		DBG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kCursor];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("+++++++++++++++++++++++++++++++++CURSOR+++++++++++++++++++++++++++++++++++");
		DBG("---------------------------------Gamepad----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("---------------------------------Keyboard---------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("---------------------------------Mouse------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kDebugOverlay];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("+++++++++++++++++++++++++++++++DEBUGOVERLAY+++++++++++++++++++++++++++++++");
		DBG("---------------------------------Gamepad----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kFavorites];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("+++++++++++++++++++++++++++++++FAVORITES++++++++++++++++++++++++++++++++++");
		DBG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("+++++++++++++++++++++++++++++++GAMEPLAY+++++++++++++++++++++++_+++++++++++");
		DBG("-------------------------------Gamepad------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("-------------------------------Keyboard-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("-------------------------------Mouse--------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kInventory];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("+++++++++++++++++++++++++++++++INVENTORY++++++++++++++++++++++++++++++++++");
		DBG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("+++++++++++++++++++++++++++++++ITEMMENU+++++++++++++++++++++++++++++++++++");
		DBG("-------------------------------Gamepad------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("-------------------------------Keyboard-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("-------------------------------Mouse--------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kJournal];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("++++++++++++++++++++++++++++++++JOURNAL+++++++++++++++++++++++++++++++++++");
		DBG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("---------------------------------Mouse------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kLockpicking];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("++++++++++++++++++++++++++++++LOCKPICKING+++++++++++++++++++++++++++++++++");
		DBG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("---------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("---------------------------------Mouse------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kMap];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("++++++++++++++++++++++++++++++++++MAP+++++++++++++++++++++++++++++++++++++");
		DBG("---------------------------------Gamepad----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("+++++++++++++++++++++++++++++++MENUMODE+++++++++++++++++++++++++++++++++++");
		DBG("-------------------------------Gamepad------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("-------------------------------Keyboard-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("-------------------------------Mouse--------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kStats];
		if (!inputContext)
		{
			return;
		}

		DBG("==========================================================================");
		DBG("+++++++++++++++++++++++++++++++++STATS++++++++++++++++++++++++++++++++++++");
		DBG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		DBG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			DBG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
	}

	void MenuInputManager::DrawPlayerMenuControlOverlay()
	{
		// Draw screen border to indicate which player is controlling menus.
		// Border is drawn while co-op is active or when in the Summoning Menu,
		// and has the same color as the menu-controlling player's crosshair.

		if (!glob.globalDataInit)
		{
			return;
		}

		bool attemptingToOpenSetupMenu = 
		{
			!menuNamesHashSet.empty() &&
			menuNamesHashSet.contains(Hash(GlobalCoopData::SETUP_MENU_NAME))
		};
		bool tempMenuOpenForCoop = !Util::MenusOnlyAlwaysOpen();
		// Update interpolated value and direction change flag + interpolation direction.
		// Use to set the overlay alpha value.
		auto dirChangeTPBefore = pmcFadeInterpData->directionChangeTP;
		bool valAtDirChangeBefore = pmcFadeInterpData->valueAtDirectionChange;
		bool dirBefore = pmcFadeInterpData->directionChangeFlag;
		bool interpToMaxBefore = pmcFadeInterpData->interpToMax;
		bool interpToMinBefore = pmcFadeInterpData->interpToMin;
		float valueBefore = pmcFadeInterpData->value;
		const float interpValue = pmcFadeInterpData->UpdateInterpolatedValue(tempMenuOpenForCoop);

		// REMOVE when done debugging.
		/*DBG
		(
			"Attempting to open setup menu: {}. Temp menu open: {}. "
			"Interped value: {}. Interp to min/max: {}, {}. Time since direction change: {}. "
			"Before: val at dir: {}, dir flag: {}, max: {}, min: {}, val: {}, secs: {}.", 
			attemptingToOpenSetupMenu,
			tempMenuOpenForCoop,
			interpValue,
			pmcFadeInterpData->interpToMax,
			pmcFadeInterpData->interpToMin,
			Util::GetElapsedSeconds(pmcFadeInterpData->directionChangeTP),
			valAtDirChangeBefore,
			dirBefore,
			interpToMaxBefore,
			interpToMinBefore,
			valueBefore,
			Util::GetElapsedSeconds(dirChangeTPBefore)
		);*/

		// Draw when a temporary menu is open or while still fading in/out.
		if (tempMenuOpenForCoop || 
			pmcFadeInterpData->interpToMax || 
			pmcFadeInterpData->interpToMin) 
		{
			const uint8_t alpha = static_cast<uint8_t>(static_cast<float>(0x7F) * interpValue);
			if (glob.coopSessionActive)
			{
				// Co-op session active.
				// Set to the player ID of the player who last controlled opened menus,
				// or P1's PID (0) if there is no recorded previous menu PID.
				pmcPID = 
				(
					glob.prevMenuPID != -1 ? glob.coopPlayers[glob.prevMenuPID]->playerID : 0
				);
			}
			else if (IsRunning()) 
			{
				// Co-op session not active and a player is in the co-op setup menu, 
				// so set to the player ID of the player requesting control of this menu.
				// Set to P1's PID (0) if there is no valid manager menu PID.
				pmcPID = managerMenuPID != -1 ? managerMenuPID : 0;
			}
			else
			{
				// P1 is in control.
				pmcPID = 0;
			}

			// Should never happen, but if not a valid player ID, return.
			if (pmcPID == -1)
			{
				return;
			}

			uint32_t uiRGBA = (Settings::vuOverlayRGBAValues[pmcPID] & 0xFFFFFF00) + alpha;
			const auto& thickness = Settings::fPlayerMenuControlOverlayOutlineThickness;
			const float rectWidth = DebugAPI::screenResX;
			const float rectHeight = DebugAPI::screenResY;

			// No overlap at corners by making sure only one of the edges 
			// is drawn all the way to the corner.

			// Left Edge.
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.25f * thickness, 0.5f * thickness), 
				glm::vec2(0.25f * thickness, rectHeight - 0.5f * thickness), 
				(Settings::vuOverlayRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.5f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.625f * thickness, 0.75f * thickness), 
				glm::vec2(0.625f * thickness, rectHeight - 0.75f * thickness), 
				(Settings::vuCrosshairInnerOutlineRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.875f * thickness, thickness), 
				glm::vec2(0.875f * thickness, rectHeight - thickness), 
				(Settings::vuCrosshairOuterOutlineRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);

			// Right Edge.
			DebugAPI::QueueLine2D
			(
				glm::vec2(rectWidth - 0.25f * thickness, 0.5f * thickness), 
				glm::vec2(rectWidth - 0.25f * thickness, rectHeight - 0.5f * thickness), 
				(Settings::vuOverlayRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.5f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(rectWidth - 0.625f * thickness, 0.75f * thickness), 
				glm::vec2(rectWidth - 0.625f * thickness, rectHeight - 0.75f * thickness), 
				(Settings::vuCrosshairInnerOutlineRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(rectWidth - 0.875f * thickness, thickness), 
				glm::vec2(rectWidth - 0.875f * thickness, rectHeight - thickness), 
				(Settings::vuCrosshairOuterOutlineRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);

			// Top Edge.
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.0f, 0.25f * thickness), 
				glm::vec2(rectWidth, 0.25f * thickness),
				(Settings::vuOverlayRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.5f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.5f * thickness, 0.625f * thickness), 
				glm::vec2(rectWidth - 0.5f * thickness, 0.625f * thickness),
				(Settings::vuCrosshairInnerOutlineRGBAValues[pmcPID] & 0xFFFFFF00) + alpha,
				0.25f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.75f * thickness, 0.875f * thickness), 
				glm::vec2(rectWidth - 0.75f * thickness, 0.875f * thickness),
				(Settings::vuCrosshairOuterOutlineRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);

			// Bottom Edge.
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.0f, rectHeight - 0.25f * thickness), 
				glm::vec2(rectWidth, rectHeight - 0.25f * thickness), 
				(Settings::vuOverlayRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.5f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.5f * thickness, rectHeight - 0.625f * thickness), 
				glm::vec2(rectWidth - 0.5f * thickness, rectHeight - 0.625f * thickness), 
				(Settings::vuCrosshairInnerOutlineRGBAValues[pmcPID] & 0xFFFFFF00) + alpha,
				0.25f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.75f * thickness, rectHeight - 0.875f * thickness), 
				glm::vec2(rectWidth - 0.75f * thickness, rectHeight - 0.875f * thickness), 
				(Settings::vuCrosshairOuterOutlineRGBAValues[pmcPID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);
		}
	}

	void MenuInputManager::EquipP1QSForm()
	{
		// Update equipped quick slot item/spell for P1
		// and update the Favorites Menu quick slot tag(s) as needed.
		// NOTE:
		// Run from MenuControls hook.

		auto ui = RE::UI::GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto taskInterface = SKSE::GetTaskInterface();
		if (!ui || !p1 || !taskInterface)
		{
			return;
		}

		// Global data not set or players not initialized, so return.
		if (!glob.globalDataInit || !glob.allPlayersInit)
		{
			return;
		}

		// P1 is controlling the menu.
		menuCoopActorHandle = p1->GetHandle();

		// Failsafe: Initialize index-to-entry map if it is empty. 
		if (favMenuIndexToEntryMap.empty()) 
		{
			InitP1QSFormEntries();
		}

		taskInterface->AddUITask
		(
			[this]() 
			{
				auto ui = RE::UI::GetSingleton(); 
				if (!ui)
				{
					return;
				}

				favoritesMenu = ui->GetMenu<RE::FavoritesMenu>();
				// Favorites menu must be open.
				if (!favoritesMenu)
				{
					return;
				}

				auto view = favoritesMenu->uiMovie; 
				if (!view)
				{
					return;
				}

				const auto& em = glob.coopPlayers[0]->em;
				// Index allows us to get the selected form.
				RE::GFxValue selectedIndex;
				view->GetVariable
				(
					std::addressof(selectedIndex), 
					"_root.MenuHolder.Menu_mc.itemList.selectedEntry.index"
				);
				const uint32_t index = static_cast<uint32_t>(selectedIndex.GetNumber());
				// Get mapped entry for the selected index.
				// Entry is used to update the item's text in the menu.
				const auto iter = favMenuIndexToEntryMap.find(index);
				if (iter == favMenuIndexToEntryMap.end())
				{
					return;
				}
				
				DBG
				(
					"{} at index {} is mapped to {}. Entries to indices map size: {} (empty: {}).", 
					index < favoritesMenu->favorites.size() && 
					favoritesMenu->favorites[index].item ? 
					favoritesMenu->favorites[index].item->GetName() :
					"NONE",
					index,
					iter->second,
					favMenuIndexToEntryMap.size(), 
					favMenuIndexToEntryMap.empty()
				);
				const uint32_t selectedEntryNum = iter->second;
				const auto form = favoritesMenu->favorites[index].item;
				if (!form)
				{
					return;
				}

				bool isConsumable = Util::IsConsumable(form);
				bool isSpell = form->Is(RE::FormType::Spell);
				// Must be a quick slot-supported item.
				if (!isConsumable && !isSpell)
				{
					return;
				}

				bool equipped = false;
				// Unequipped if already equipped; otherwise, equip the new item/spell.
				if (isConsumable)
				{
					em->quickSlotItem = form == em->quickSlotItem ? nullptr : form;
					equipped = em->quickSlotItem;
				}
				else
				{
					em->quickSlotSpell = 
					(
						form == em->quickSlotSpell ? nullptr : form->As<RE::SpellItem>()
					);
					equipped = em->quickSlotSpell;
				}

				RE::GFxValue entry;
				view->GetVariableArray
				(
					"_root.MenuHolder.Menu_mc.itemList.entryList", 
					selectedEntryNum, 
					std::addressof(entry), 
					1
				);
				RE::GFxValue entryText;
				entry.GetMember("text", std::addressof(entryText));
				std::string entryStr = entryText.GetString();
				// Update entry text with the quick slot tag.
				if (equipped)
				{
					entryStr = fmt::format("(*QS{}*) {}", isConsumable ? "I" : "S", entryStr);
					// Index corresponding to the previously equipped QS item/spell.
					uint32_t equippedQSIndex = 
					(
						isConsumable ? em->equippedQSItemIndex : em->equippedQSSpellIndex
					);
					// Remove item/spell tag from old quick slot item/spell entry if needed.
					if (equippedQSIndex != -1 && index != equippedQSIndex)
					{
						uint32_t oldEntryNum = favMenuIndexToEntryMap[equippedQSIndex];
						RE::GFxValue oldEntry;
						view->GetVariableArray
						(
							"_root.MenuHolder.Menu_mc.itemList.entryList", 
							oldEntryNum, 
							std::addressof(oldEntry), 
							1
						);
						RE::GFxValue oldEntryText{ };
						oldEntry.GetMember("text", std::addressof(oldEntryText));
						std::string oldEntryStr = oldEntryText.GetString();
						auto qsTagStartIndex = oldEntryStr.find("(*QS", 0);
						// Tag found and item name has a non-zero length.
						if (qsTagStartIndex != std::string::npos && 
							qsTagStartIndex + qsPrefixTagLength <= oldEntryStr.length())
						{
							// Restore old entry text with tag removed.
							oldEntryStr = oldEntryStr.substr(qsTagStartIndex + qsPrefixTagLength);
							oldEntryText.SetString(oldEntryStr);
							oldEntry.SetMember("text", oldEntryText);
							view->SetVariableArray
							(
								"_root.MenuHolder.Menu_mc.itemList.entryList",
								oldEntryNum, 
								std::addressof(oldEntry), 
								1
							);
						}
					}
				}
				else
				{
					// Remove QS tag from the current entry.
					auto qsTagStartIndex = entryStr.find("(*QS", 0);
					if (qsTagStartIndex != std::string::npos && 
						qsTagStartIndex + qsPrefixTagLength <= entryStr.length())
					{
						entryStr = entryStr.substr(qsTagStartIndex + qsPrefixTagLength);
					}
				}

				DBG("{} {}.", equipped ? "Equipped" : "Unequipped", entryStr);

				// Set entry text and then insert back into the list.
				entryText.SetString(entryStr);
				entry.SetMember("text", entryText);
				view->SetVariableArray
				(
					"_root.MenuHolder.Menu_mc.itemList.entryList",
					selectedEntryNum,
					std::addressof(entry), 
					1
				);

				// Set new equipped quick slot item/spell index.
				if (isConsumable)
				{
					em->equippedQSItemIndex = equipped ? index : -1;
				}
				else
				{
					em->equippedQSSpellIndex = equipped ? index : -1;
				}

				// Refresh equip state after equip.
				glob.coopPlayers[0]->em->RefreshEquipState(RefreshSlots::kAll);
				// Update the list to reflect our changes.
				view->InvokeNoReturn("_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0);
			}
		);
	}

	uint32_t MenuInputManager::GetMappedKey
	(
		std::string_view a_eventID,
		RE::INPUT_DEVICE a_device, 
		RE::ControlMap::InputContextID a_context
	)
	{
		// Using the MIM's previously cached input mappings list, 
		// get the ID code corresponding to the given user event name, device, and context.

		assert(a_device < RE::INPUT_DEVICE::kTotal);
		assert(a_context < RE::ControlMap::InputContextID::kTotal);

		if (!glob.globalDataInit)
		{
			return -1;
		}

		const auto& mappings = inputMappings[a_context][a_device];
		RE::BSFixedString eventID(a_eventID);
		for (auto& mapping : mappings)
		{
			if (mapping.first == eventID)
			{
				return mapping.second;
			}
		}

		return -1;
	}

	RE::TESForm* MenuInputManager::GetSelectedMagicMenuSpell()
	{
		// Get the spell/shout that the player has selected in the Magic Menu.

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			return nullptr;
		}

		// Form for spell to equip.
		RE::TESForm* formToEquip = nullptr;
		// Not actually an ItemList but likely inherits from it.
		// At the very least, the cast allows us to identify the selected item.
		auto magicItemList = reinterpret_cast<RE::ItemList*>(magicMenu->unk30);
		RE::ItemList::Item* selectedItem = GetSelectedItem(magicItemList);
		if (!selectedItem)
		{
			return nullptr;
		}

		// First, if SKYUI is installed (it should be), 
		// this will grab the selected form based on its form ID,
		// which is the most accurate way of getting the selected magic item.

		// Get the selected spell's index first.
		int32_t index = -1;
		if (!magicItemList->unk50)
		{
			RE::GFxValue selectedIndex;
			auto success = magicItemList->root.GetMember("selectedIndex", &selectedIndex);
			if (success && selectedIndex.IsNumber())
			{
				index = static_cast<std::int32_t>(selectedIndex.GetNumber());
			}
		}

		if (index >= 0 && index < magicItemList->items.size())
		{
			RE::GFxValue entry{ };
			magicItemList->entryList.GetElement(index, std::addressof(entry));
			RE::GFxValue entryFormId;
			entry.GetMember("formId", std::addressof(entryFormId));

			uint32_t formID = 0;
			if (entryFormId.GetNumber() != 0)
			{
				formID = static_cast<uint32_t>(entryFormId.GetNumber());
			}
			else
			{
				entry.GetMember("formID", std::addressof(entryFormId));
				if (entryFormId.GetNumber() != 0)
				{
					formID = static_cast<uint32_t>(entryFormId.GetNumber());
				}
			}

			if (formID != 0) 
			{
				// Got the form, so nothing more to do below.
				if (RE::TESForm* magicForm = RE::TESForm::LookupByID(formID); magicForm)
				{
					return magicForm;
				}
			}
		}

		// If the above method fails, we have to compare the selected item's name 
		// with known spells/shouts, which will sometimes fail to match 
		// with the correct spell/shout if the player knows multiple spells/shouts 
		// with the same name.

		//=========================================================================================
		// First ensure both the player in the menu and P1 have the same known spells and shouts.
		//=========================================================================================

		// Ensure placeholder spells/shout are not added to P1.
		const auto& p = glob.coopPlayers[managerMenuPID];
		auto placeholderSpell2H = p->em->placeholderMagic[!PlaceholderMagicIndex::k2H];
		auto placeholderSpellLH = p->em->placeholderMagic[!PlaceholderMagicIndex::kLH];
		auto placeholderSpellRH = p->em->placeholderMagic[!PlaceholderMagicIndex::kRH];

		// Add spells that the co-op companion player learned.
		for (auto spellItem : menuCoopActorPtr->addedSpells)
		{
			if (!glob.player1Actor->HasSpell(spellItem) && 
				!glob.placeholderSpellsSet.contains(spellItem))
			{
				glob.player1Actor->AddSpell(spellItem);
				break;
			}
		}
		
		auto companionActorBase = menuCoopActorPtr->GetActorBase();
		if (companionActorBase)
		{
			// Add spells that the co-op companion player has by virtue of their actor base.
			auto spellList = companionActorBase->actorEffects->spells;
			if (spellList)
			{
				uint32_t spellListSize = companionActorBase->actorEffects->numSpells;
				for (uint32_t i = 0; i < spellListSize; ++i)
				{
					auto spellItem = spellList[i];
					if (!glob.player1Actor->HasSpell(spellItem) && 
						!glob.placeholderSpellsSet.contains(spellItem))
					{
						glob.player1Actor->AddSpell(spellItem);
						break;
					}
				}
			}

			auto shoutList = companionActorBase->actorEffects->shouts;
			if (shoutList)
			{
				uint32_t shoutListSize = companionActorBase->actorEffects->numShouts;
				// Add shouts that the co-op companion player has by virtue of their actor base.
				for (uint32_t i = 0; i < shoutListSize; ++i)
				{
					auto shout = shoutList[i];
					if (!glob.player1Actor->HasShout(shout))
					{
						glob.player1Actor->AddShout(shout);
						break;
					}
				}
			}
		}

		auto chosenMagicItemName = selectedItem->data.GetName();
		// Match spell name with one of P1's learned spells.
		for (auto spellItem : glob.player1Actor->addedSpells)
		{
			if (strcmp(spellItem->GetName(), chosenMagicItemName) == 0)
			{
				formToEquip = spellItem;
				break;
			}
		}

		auto p1ActorBase = glob.player1Actor->GetActorBase();
		if (p1ActorBase)
		{
			auto spellList = p1ActorBase->actorEffects->spells; 
			if (spellList)
			{
				uint32_t spellListSize = p1ActorBase->actorEffects->numSpells;
				// Match spell name with one of P1's actorbase spells.
				for (uint32_t i = 0; i < spellListSize; ++i)
				{
					auto spellItem = spellList[i];
					if (strcmp(spellItem->GetName(), chosenMagicItemName) == 0)
					{
						formToEquip = spellItem;
						break;
					}
				}
			}

			auto shoutList = p1ActorBase->actorEffects->shouts; 
			if (shoutList)
			{
				uint32_t shoutListSize = p1ActorBase->actorEffects->numShouts;
				// Match with shouts that P1 has by virtue of their actor base.
				for (uint32_t i = 0; i < shoutListSize; ++i)
				{
					// Some unused shouts exist.
					// All have one-character names.
					if (shoutList[i] && 
						strlen(shoutList[i]->GetName()) > 1 && 
						strcmp(shoutList[i]->GetName(), chosenMagicItemName) == 0)
					{
						formToEquip = shoutList[i];
					}
				}
			}
		}

		// Failsafe: Ensure placeholder spell/shout is not selected.
		if ((formToEquip) &&
			(formToEquip == placeholderSpell2H ||
			formToEquip == placeholderSpellLH ||
			formToEquip == placeholderSpellRH)) 
		{
			formToEquip = nullptr;
		}

		return formToEquip;
	}

	std::string_view MenuInputManager::GetUserEventName
	(
		uint32_t a_buttonID, 
		RE::INPUT_DEVICE a_device, 
		RE::ControlMap::InputContextID a_context
	)
	{
		// Using the MIM's previously cached input mappings list,
		// get the user event name corresponding to the given button ID, device, and context.

		assert(a_device < RE::INPUT_DEVICE::kTotal);
		assert(a_context < RE::InputContextID::kTotal);

		if (!glob.globalDataInit)
		{
			return ""sv;
		}

		const auto& mappings = inputMappings[a_context][a_device];
		for (const auto& [eventName, inputKey] : inputMappings[a_context][a_device])
		{
			if (inputKey == a_buttonID)
			{
				return eventName;
			}
		}

		return ""sv;
	}

	void MenuInputManager::HotkeyFavoritedForm(bool&& a_setHotkey)
	{
		// Preview or set a hotkey to the currently selected favorited item entry.
		// 8 possible hotkey slots starting from index 0 when the RS is pointed up.

		if (!glob.globalDataInit || !glob.coopSessionActive)
		{
			return;
		}

		auto taskInterface = SKSE::GetTaskInterface();
		// Can't get selected menu entry if task interface is invalid.
		if (!taskInterface)
		{
			return;
		}

		if (a_setHotkey)
		{
			taskInterface->AddUITask
			(
				[this]() 
				{
					auto ui = RE::UI::GetSingleton(); 
					if (!ui)
					{
						return;
					}

					auto ue = RE::UserEvents::GetSingleton();
					if (!ue)
					{
						return;
					}

					favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
					if (!favoritesMenu)
					{
						return;
					}

					auto view = favoritesMenu->uiMovie; 
					if (!view)
					{
						return;
					}

					// Get entry for the item/spell and its text.
					RE::GFxValue entry{ };
					view->GetVariable
					(
						std::addressof(entry),
						"_root.MenuHolder.Menu_mc.itemList.selectedEntry"
					);

					// Need a valid selected entry.
					if (entry.IsNull() || entry.IsUndefined())
					{
						return;
					}

					RE::GFxValue selectedIndex{ };
					view->GetVariable
					(
						std::addressof(selectedIndex),
						"_root.MenuHolder.Menu_mc.itemList.selectedEntry.index"
					);
					
					// Need a valid selected index.
					if (selectedIndex.IsNull() || selectedIndex.IsUndefined())
					{
						return;
					}

					const int32_t index = static_cast<int32_t>(selectedIndex.GetNumber());
					if (index == -1)
					{
						return;
					}
					
					const auto& p = glob.coopPlayers[glob.menuPID];
					auto form = favoritesMenu->favorites[index].item;
					// Must have a valid selected form.
					if (!form)
					{
						return;
					}

					// Index must have a corresponding entry number.
					const auto iter = favMenuIndexToEntryMap.find(index);
					if (iter == favMenuIndexToEntryMap.end())
					{
						DBG
						(
							"{}'s favorited form {} "
							"does not have an entry number corresponding to an index of {}.",
							p->coopActor->GetName(), form->GetName(), index
						);
						return;
					}

					// Get entry number corresponding to index.
					const uint32_t selectedEntryNum = favMenuIndexToEntryMap.at(index);
					// Get current hotkey.
					RE::GFxValue entryHotkey{ };
					entry.GetMember("hotkey", std::addressof(entryHotkey));

					// Entry needs to have a hotkey member.
					if (entryHotkey.IsNull() || entryHotkey.IsUndefined())
					{
						return;
					}

					auto currentHotkey = static_cast<int32_t>(entryHotkey.GetSInt());
					if (currentHotkey == -1)
					{
						return;
					}
					
					int32_t hotkeySlotToChange = currentHotkey;
					RE::BSFixedString hotkeyEvent = ""sv;
					if (currentHotkey == 0)
					{
						hotkeyEvent = ue->hotkey1;
					}
					else if (currentHotkey == 1)
					{
						hotkeyEvent = ue->hotkey2;
					}
					else if (currentHotkey == 2)
					{
						hotkeyEvent = ue->hotkey3;
					}
					else if (currentHotkey == 3)
					{
						hotkeyEvent = ue->hotkey4;
					}
					else if (currentHotkey == 4)
					{
						hotkeyEvent = ue->hotkey5;
					}
					else if (currentHotkey == 5)
					{
						hotkeyEvent = ue->hotkey6;
					}
					else if (currentHotkey == 6)
					{
						hotkeyEvent = ue->hotkey7;
					}
					else
					{
						hotkeyEvent = ue->hotkey8;
					}

					// Hotkey the entry through an emulated keyboard input.
					auto hotkeyCode = GetMappedKey(hotkeyEvent, RE::INPUT_DEVICE::kKeyboard);
					if (hotkeyCode == 0xFF)
					{
						return;
					}

					// Press and release.
					Util::SendButtonEvent
					(
						RE::INPUT_DEVICE::kKeyboard, 
						hotkeyEvent,
						hotkeyCode,
						1.0f, 
						0.0f, 
						false, 
						true
					);
					Util::SendButtonEvent
					(
						RE::INPUT_DEVICE::kKeyboard, 
						hotkeyEvent, 
						hotkeyCode, 
						0.0f, 
						1.0f,
						false, 
						true
					);

					auto p1 = RE::PlayerCharacter::GetSingleton();
					auto extraDataList = 
					(
						favoritesMenu->favorites[index].entryData &&
						favoritesMenu->favorites[index].entryData->extraLists ? 
						favoritesMenu->favorites[index].entryData->extraLists->front() :
						nullptr
					);
					// If the game fails to set it via Input Event, force-apply the hotkey.
					bool isP1Hotkeyed = Util::IsHotkeyed(p1, form, extraDataList);
					if (!isP1Hotkeyed)
					{
						DBG
						(
							"ERR: {}: Failed to apply hotkey {} to P1's inventory entry, "
							"list {:p} for {}.",
							p->coopActor->GetName(),
							hotkeySlotToChange,
							fmt::ptr(extraDataList),
							form->GetName()
						);
						Util::ChangeFormHotkeyStatus(p1, form, hotkeySlotToChange, extraDataList);
						isP1Hotkeyed = Util::IsHotkeyed(p1, form, extraDataList);
					}

					DBG
					(
						"{}: Hotkeying {} (list {:p}) into slot {}, is now hotkeyed by P1: {}.",
						p->coopActor->GetName(),
						form->GetName(), 
						fmt::ptr(extraDataList),
						hotkeySlotToChange, 
						isP1Hotkeyed
					);

					if (!p->isPlayer1) 
					{
						// Ensure the companion player has the same hotkey state
						// as P1 for the form.
						auto chestExtraDataList = Util::FindMatchingExtraDataList
						(
							p->em->inventoryChest.get(),
							form->As<RE::TESBoundObject>(),
							extraDataList
						);
						Util::ChangeFormHotkeyStatus
						(
							p->em->inventoryChest.get(), 
							form, 
							isP1Hotkeyed ? hotkeySlotToChange : -1,
							chestExtraDataList
						);

						DBG
						(
							"{} (list {:p}) is now hotkeyed in inventory chest: {}.", 
							form->GetName(), 
							fmt::ptr(extraDataList),
							Util::IsHotkeyed
							(
								p->em->inventoryChest.get(), form, chestExtraDataList
							)
						);
						// Signal manager to refresh the menu.
						shouldRefreshMenu = true;
					}
					else
					{
						// Send update request to have the Favorites Menu ProcessMessage() hook 
						// re-apply the quickslot tag(s).
						auto messageQueue = RE::UIMessageQueue::GetSingleton();
						if (messageQueue)
						{
							messageQueue->AddMessage
							(
								RE::FavoritesMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kUpdate, nullptr
							);
						}
					}
				}
			);
		}
		else
		{
			taskInterface->AddUITask
			(
				[this]() 
				{
					auto ui = RE::UI::GetSingleton(); 
					if (!ui)
					{
						return;
					}
					
					auto ue = RE::UserEvents::GetSingleton();
					if (!ue)
					{
						return;
					}

					favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
					if (!favoritesMenu)
					{
						return;
					}

					auto view = favoritesMenu->uiMovie; 
					if (!view)
					{
						return;
					}
					
					int32_t menuPID = glob.menuPID;
					// Give P1 control if no player is in menus.
					if (menuPID == -1) 
					{
						menuPID = 0;
					}

					const auto& rsData = glob.cdh->GetAnalogStickState
					(
						glob.coopPlayers[menuPID]->deviceID, false
					);
					// Keeping the stick offset or moving away from center.
					if (rsData.normMag == 0.0f || rsData.normMag - rsData.prevNormMag <= -1E-2f)
					{
						return;
					}

					// Get RS angle and pick hotkey slot to apply.
					float realRSAng = atan2f(rsData.yComp, rsData.xComp);
					realRSAng = Util::ConvertAngle(Util::NormalizeAng0To2Pi(realRSAng));
					RE::BSFixedString hotkeyEvent = ""sv;
					int32_t hotkeySlotToChange = -1;
					if (realRSAng < PI / 8.0f || realRSAng > 15.0f * PI / 8.0f)
					{
						hotkeyEvent = ue->hotkey1;
						hotkeySlotToChange = 0;
					}
					else if (realRSAng < 3.0f * PI / 8.0f)
					{
						hotkeyEvent = ue->hotkey2;
						hotkeySlotToChange = 1;
					}
					else if (realRSAng < 5.0f * PI / 8.0f)
					{
						hotkeyEvent = ue->hotkey3;
						hotkeySlotToChange = 2;
					}
					else if (realRSAng < 7.0f * PI / 8.0f)
					{
						hotkeyEvent = ue->hotkey4;
						hotkeySlotToChange = 3;
					}
					else if (realRSAng < 9.0f * PI / 8.0f)
					{
						hotkeyEvent = ue->hotkey5;
						hotkeySlotToChange = 4;
					}
					else if (realRSAng < 11.0f * PI / 8.0f)
					{
						hotkeyEvent = ue->hotkey6;
						hotkeySlotToChange = 5;
					}
					else if (realRSAng < 13.0f * PI / 8.0f)
					{
						hotkeyEvent = ue->hotkey7;
						hotkeySlotToChange = 6;
					}
					else
					{
						hotkeyEvent = ue->hotkey8;
						hotkeySlotToChange = 7;
					}
					
					if (hotkeySlotToChange == -1)
					{
						return;
					}

					// Get entry for the item/spell and its text.
					RE::GFxValue entry{ };
					view->GetVariable
					(
						std::addressof(entry),
						"_root.MenuHolder.Menu_mc.itemList.selectedEntry"
					);

					// Need a valid selected entry.
					if (entry.IsNull() || entry.IsUndefined())
					{
						return;
					}

					RE::GFxValue selectedIndex{ };
					view->GetVariable
					(
						std::addressof(selectedIndex),
						"_root.MenuHolder.Menu_mc.itemList.selectedEntry.index"
					);
					
					// Need a valid selected index.
					if (selectedIndex.IsNull() || selectedIndex.IsUndefined())
					{
						return;
					}

					const int32_t index = static_cast<int32_t>(selectedIndex.GetNumber());
					if (index == -1)
					{
						return;
					}
					
					const auto& p = glob.coopPlayers[menuPID];
					auto form = favoritesMenu->favorites[index].item;
					// Must have a valid selected form.
					if (!form)
					{
						return;
					}

					// Index must have a corresponding entry number.
					const auto iter = favMenuIndexToEntryMap.find(index);
					if (iter == favMenuIndexToEntryMap.end())
					{
						DBG
						(
							"{}'s favorited form {} "
							"does not have an entry number corresponding to an index of {}.",
							p->coopActor->GetName(), form->GetName(), index
						);
						return;
					}

					// Get entry number corresponding to index.
					const uint32_t selectedEntryNum = favMenuIndexToEntryMap.at(index);
					// Get current hotkey.
					RE::GFxValue entryHotkey{ };
					entry.GetMember("hotkey", std::addressof(entryHotkey));

					// Entry needs to have a hotkey member.
					if (entryHotkey.IsNull() || entryHotkey.IsUndefined())
					{
						return;
					}

					auto currentHotkey = static_cast<int32_t>(entryHotkey.GetSInt());
					// Already hotkeyed in the requested slot.
					if (hotkeySlotToChange == currentHotkey)
					{
						return;
					}

					entryHotkey.SetNumber(hotkeySlotToChange);
					entry.SetMember("hotkey", entryHotkey);
					view->SetVariableArray
					(
						"_root.MenuHolder.Menu_mc.itemList.entryList", 
						index, 
						std::addressof(entry),
						1
					);
					view->InvokeNoReturn
					(
						"_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0
					);

					if (!p->isPlayer1) 
					{
						DBG
						(
							"{}: {} will be hotkeyed in slot {} on release.",
							p->coopActor->GetName(), form->GetName(), hotkeySlotToChange
						);
						// Signal manager to refresh the menu.
						shouldRefreshMenu = true;
					}
					else
					{
						// Send update request to have the Favorites Menu ProcessMessage() hook 
						// apply the quickslot tag(s).
						auto messageQueue = RE::UIMessageQueue::GetSingleton();
						if (messageQueue)
						{
							messageQueue->AddMessage
							(
								RE::FavoritesMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kUpdate, nullptr
							);
						}
					}
				}
			);
		}
	}

	void MenuInputManager::HandleLootRequest(bool&& a_takeAll)
	{
		// Loot all items or all of the selected item in the Container Menu.
		// Transfer to P1 and then to the requesting player.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1) 
		{
			return;
		}

		auto fromContainerPtr = Util::GetRefrPtrFromHandle(fromContainerHandle);
		// If the container to loot from is invalid 
		// or if the player has not selected an item, return here.
		if ((!fromContainerPtr) || (!selectedForm || !a_takeAll))
		{
			return;
		}

		if (a_takeAll) 
		{
			const auto inventory = fromContainerPtr->GetInventory();
			// Walk through inventory and remove all of each item.
			for (const auto& [boundObj, countEntryPair] : inventory) 
			{
				if (!boundObj || countEntryPair.first <= 0) 
				{
					continue;
				}
				
				// To P1 and then to the requesting actor 
				// through the subsequent container changed event.
				fromContainerPtr->RemoveItem
				(
					boundObj,
					countEntryPair.first,
					RE::ITEM_REMOVE_REASON::kStoreInContainer,
					nullptr, 
					p1
				);
			}
		}

		if (auto boundObj = selectedForm->As<RE::TESBoundObject>(); boundObj)
		{
			// Loot a specific item.
			int32_t count = -1;
			const auto invCounts = fromContainerPtr->GetInventoryCounts();
			const auto iter = invCounts.find(boundObj);
			if (iter != invCounts.end()) 
			{
				count = iter->second;
			}

			// Not inside inventory, so nothing to loot.
			if (count == -1) 
			{
				return;
			}

			// To P1 and then to the requesting actor 
			// through the subsequent container changed event.
			fromContainerPtr->RemoveItem
			(
				boundObj,
				count, 
				RE::ITEM_REMOVE_REASON::kStoreInContainer, 
				nullptr, 
				p1
			);
		}
	}

	void MenuInputManager::HandleMenuEvent()
	{
		// Handle resolved menu event type.

		switch (currentMenuInputEventType)
		{
		case MenuInputEventType::kEquipReq:
		{
			if (managerMenuPID > -1 && managerMenuPID < ALYSLC_MAX_PLAYER_COUNT)
			{
				const auto& p = glob.coopPlayers[managerMenuPID];
				// Do not equip if not a humanoid and not a vampire lord
				// and the item to equip is a weapon or spell, since they are not usable
				// and can cause equip looping.
				bool canEquip = 
				(
					(!p->coopActor->race) ||
					(p->coopActor->race->HasKeyword(glob.npcKeyword)) ||
					(Util::IsVampireLord(p->coopActor.get())) ||
					(
						selectedForm->As<RE::TESObjectARMO>() ||
						selectedForm->As<RE::AlchemyItem>() ||
						selectedForm->As<RE::IngredientItem>()
					)
				);
				if (canEquip)
				{
					DBG
					(
						"Equip Request Event: from container: {}, "
						"form: {}, equip index: {}, placeholder spell changed: {}.",
						(Util::HandleIsValid(fromContainerHandle)) ? 
						fromContainerHandle.get()->GetName() : 
						"NONE",
						(selectedForm) ? selectedForm->GetName() : "NONE",
						reqEquipIndex,
						placeholderMagicChanged
					);
					// Equip/unequip the selected form.
					p->em->HandleMenuEquipRequest
					(
						fromContainerHandle,
						selectedForm, 
						selectedExDataList,
						reqEquipIndex, 
						placeholderMagicChanged
					);
					// Reset placeholder magic changed flag and equip index.
					placeholderMagicChanged = false;
					reqEquipIndex = EquipIndex::kRightHand;
				}
			}

			break;
		}
		case MenuInputEventType::kEmulateInput:
		{
			// String together any queued emulated input events after checking device input.
			// Link individual input events into a chain.
			for (uint32_t i = 0; i < queuedInputEvents.size() - 1; ++i)
			{
				(*(queuedInputEvents[i].get()))->next = *(queuedInputEvents[i + 1].get());
			}

			if (magicMenu)
			{
				if (spellFavoriteStatusChanged)
				{
					const auto& p = glob.coopPlayers[managerMenuPID];
					// Have to set cyclable favorited spells
					// after a spell is favorited/unfavorited.
					p->em->UpdateFavoritedFormsLists(false);
				}
			}

			break;
		}
		default:
		{
			break;
		}
		}

		// Reset menu event type to none after handling the event.
		currentMenuInputEventType = MenuInputEventType::kReleasedNoEvent;
	}

	void MenuInputManager::InitFavoritesEntries()
	{
		DBG("InitFavoritesEntries.");

		// Update equip states in the Favorites Menu 
		// for forms equipped by the co-op companion player.

		if (managerMenuPID <= -1 || managerMenuPID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}

		// Ensure cached favorited items are up to date.
		const auto& p = glob.coopPlayers[managerMenuPID];
		p->em->RefreshEquipState(RefreshSlots::kAll);
		// Update menu equip state with the refreshed favorites data.
		RefreshFavoritesMenuEquipState();
		shouldReloadMenuEntries = false;
		shouldRefreshMenu = true;
	}

	void MenuInputManager::InitMagicMenuEquippedStates()
	{
		// Update equip states in the Magic Menu 
		// for spells/shouts equipped by the co-op companion player.

		// Ensure companio player placeholder spell/shouts are NOT learned by P1.
		if (managerMenuPID != -1) 
		{
			const auto& p = glob.coopPlayers[managerMenuPID];
			for (auto placeholderSpellForm : p->em->placeholderMagic)
			{
				if (!placeholderSpellForm)
				{
					continue;
				}

				auto spell = placeholderSpellForm->As<RE::SpellItem>();
				if (!spell || !glob.player1Actor->HasSpell(spell))
				{
					continue;
				}
				
				// P1 knows the placdholder spell, so remove it.
				glob.player1Actor->RemoveSpell(spell);
			}
		}

		// P1's favorited magic forms are still marked with a star 
		// even though the MagicFavorites singleton does not include them 
		// after we've imported the companion player's favorited magic. 
		// Reload the entries to set the proper favorites star markers.
		shouldReloadMenuEntries = true;
		shouldRefreshMenu = true;
	}
	
	void MenuInputManager::InitP1QSFormEntries()
	{
		// Set quick slot tags for any equipped quick slot items/spells 
		// and update index-to-entry map.

		DBG("InitP1QSFormEntries");
		auto ui = RE::UI::GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto taskInterface = SKSE::GetTaskInterface();
		if (!ui || !p1 || !taskInterface)
		{
			return;
		}

		if (!ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME))
		{
			return;
		}

		favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
		if (!favoritesMenu)
		{
			return;
		}

		menuCoopActorHandle = p1->GetHandle();
		const auto& em = glob.coopPlayers[0]->em;
		const auto& favoritesList = favoritesMenu->favorites;
		// Clear the menu entry to list index map before reconstructing it below.
		favMenuIndexToEntryMap.clear();
		// Update equip state.
		em->RefreshEquipState(RefreshSlots::kAll);
		// Iterate through the favorites list in order, 
		// get the list indices for any quick slot forms 
		// and check if any current quick slot forms are still in the favorites list.
		bool itemStillFavorited = false;
		bool spellStillFavorited = false;
		RE::TESForm* favForm = nullptr;
		for (auto i = 0; i < favoritesList.size(); ++i)
		{
			favForm = favoritesList[i].item;
			if (!favForm)
			{
				continue;
			}

			if (favForm->Is(RE::FormType::Spell))
			{
				// Check if quick slot spell is favorited.
				auto quickSlotSpell = em->quickSlotSpell;
				if (quickSlotSpell && quickSlotSpell == favForm)
				{
					em->equippedQSSpellIndex = i;
					spellStillFavorited = true;
				}
			}
			else if (Util::IsConsumable(favForm))
			{
				// Check if quick slot item is equipped.
				auto quickSlotItem = em->quickSlotItem;
				if (quickSlotItem && quickSlotItem == favForm)
				{
					em->equippedQSItemIndex = i;
					itemStillFavorited = true;
				}
			}
		}

		// Clear out quick slot item/spell if it isn't favorited/equipped.
		if (!itemStillFavorited)
		{
			em->quickSlotItem = nullptr;
			em->equippedQSItemIndex = -1;
		}

		if (!spellStillFavorited)
		{
			em->quickSlotSpell = nullptr;
			em->equippedQSSpellIndex = -1;
		}
		
		// Temp hacky workaround to override entry text changes without a hook:
		// Update the Favorites Menu UI entries to reflect the initial equip state 
		// of quick slot items/spells and update the index-to-entry map for use in (un)equipping
		// spells or items on player demand.
		// Delay the update a bit to make sure our entry changes stick, since, at least for P1,
		// the entries are reset to default once the initial equip state is read in
		// shortly after the menu opens.
		glob.taskRunner->AddTask
		(
			"GLOB Runner",
			__FUNCTION__,
			[this]()
			{
				// Tested in the framerate range (15-100+).
				std::this_thread::sleep_for(0.5s);
				Util::AddSyncedTask
				(
					[this]()
					{
						auto ui = RE::UI::GetSingleton(); 
						if (!ui)
						{
							return;
						}

						favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
						if (!favoritesMenu)
						{
							return;
						}

						auto view = favoritesMenu->uiMovie; 
						if (!view)
						{
							return;
						}

						// Entry positions in the menu DO NOT correspond to 
						// their indices in the favorites list. 
						// Have to map out the entries for all indices.
						RE::GFxValue entryList{ };
						view->CreateArray(std::addressof(entryList));
						view->GetVariable
						(
							std::addressof(entryList), 
							"_root.MenuHolder.Menu_mc.itemList.entryList"
						);
						double numEntries = view->GetVariableDouble
						(
							"_root.MenuHolder.Menu_mc.itemList.entryList.length"
						);
						for (uint32_t i = 0; i < numEntries; ++i)
						{
							RE::GFxValue entryIndex{ };
							RE::GFxValue entry;
							view->GetVariableArray
							(
								"_root.MenuHolder.Menu_mc.itemList.entryList", 
								i, 
								std::addressof(entry),
								1
							);
							entry.GetMember("index", std::addressof(entryIndex));

							uint32_t index = static_cast<uint32_t>(entryIndex.GetNumber());
							RE::GFxValue entryText{ };
							entry.GetMember("text", std::addressof(entryText));
							std::string entryStr = entryText.GetString();

							// Update equip state for index.
							// Normal items receive an update to the "caret" equipped icon,
							// while quick slot items have their entry text modified.
							// This tag gets wiped whenever the favorites menu is opened,
							// so it must be re-applied each time.
							const auto& em = glob.coopPlayers[0]->em;
							bool matching = 
							(
								favoritesMenu->favorites[index].item == 
								em->quickSlotItem ||
								favoritesMenu->favorites[index].item == 
								em->quickSlotSpell
							);
							if (matching)
							{
								if (entryStr.find("(*QS", 0) == std::string::npos)
								{
									bool isConsumable = index == em->equippedQSItemIndex;
									entryStr = fmt::format
									(
										"(*QS{}*) {}", isConsumable ? "I" : "S", entryStr
									);
									// Set entry text and apply modified entry.
									entryText.SetString(entryStr);
									entry.SetMember("text", entryText);
									view->SetVariableArray
									(
										"_root.MenuHolder.Menu_mc.itemList.entryList", 
										i,
										std::addressof(entry), 
										1
									);
									DBG
									(
										"Set {} entry as {}.",
										isConsumable ? "QSI" : "QSS",
										entryStr
									);
								}
							}

							// Insert pairs into the map.
							// (key = favorites list index, value = UI entry number)
							favMenuIndexToEntryMap.insert_or_assign(index, i);
						}

						// Update list to reflect changes.
						view->InvokeNoReturn
						(
							"_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0
						);
					},
					true
				);
			}	
		);
	}

	bool MenuInputManager::PerformEnderalSkillLevelUp(RE::AlchemyItem* a_skillbook)
	{
		// Level up the skill linked with the given skillbook 
		// for the player currently controlling menus.

		// Get player requesting use to level up with the book
		// (the player controlling the Container Menu).
		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr || !a_skillbook)
		{
			DBG
			(
				"FAIL: No player in menu: {}, no skillbook: {}.",
				!menuCoopActorPtr, !a_skillbook
			);
			return false;
		}

		// Get the container, which should be the player if they are in their own inventory,
		// or the container the player is attempting to loot the book from.
		auto containerRefrPtr = Util::GetRefrPtrFromHandle(menuContainerHandle); 
		if (!containerRefrPtr) 
		{
			DBG("FAIL: No container refr.");
			return false;
		}

		auto skillAV = RE::ActorValue::kNone;
		auto skillbookTier = EnderalSkillbookTier::kTotal;
		const RE::FormID& fid = a_skillbook->formID;
		// Get tier and skill to level up for this skillbook.
		const auto iter = GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.find(fid);
		if (iter != GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.end()) 
		{
			const auto& tierSkillPair = iter->second;
			skillbookTier = tierSkillPair.first;
			skillAV = tierSkillPair.second;
		}

		std::string skillName = "";
		bool canUseToLevelUp = false;
		if (skillAV != RE::ActorValue::kNone) 
		{
			// Get player's skill level and Enderal skill name.
			float avLvl = menuCoopActorPtr->GetBaseActorValue(skillAV);
			skillName = GlobalCoopData::SKYRIM_AVS_TO_ENDERAL_SKILL_NAMES_MAP.at(skillAV);
			switch (skillbookTier)
			{
			case EnderalSkillbookTier::kApprentice:
			{
				if (avLvl <= 24.0f)
				{
					canUseToLevelUp = true;
				}

				break;
			}
			case EnderalSkillbookTier::kAdept:
			{
				if (avLvl <= 49.0f)
				{
					canUseToLevelUp = true;
				}

				break;
			}
			case EnderalSkillbookTier::kExpert:
			{
				if (avLvl <= 74.0f)
				{
					canUseToLevelUp = true;
				}

				break;
			}
			case EnderalSkillbookTier::kMaster:
			{
				if (avLvl <= 99.0f)
				{
					canUseToLevelUp = true;
				}

				break;
			}
			default:
			{
				break;
			}
			}
		}
		
		// Check if the skill to level up is shared.
		bool isShared = GlobalCoopData::SHARED_SKILL_AVS_SET.contains(skillAV);
		// The skills which are leveled via crafting points 
		// are the same as the shared skills in co-op.
		// What a happy coincidence!
		bool pointsAvailable = 
		{
			(!isShared && glob.learningPointsGlob->value > 0.0f) ||
			(isShared && glob.craftingPointsGlob->value > 0.0f)
		};

		if (canUseToLevelUp && pointsAvailable)
		{
			// Index in P1's singleton skills list that corresponds to this AV.
			int32_t skillAVIndex = -1;
			// Get skill index for the skill actor value.
			for (auto i = 0; i < Skill::kTotal; ++i)
			{
				if (glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i)) == skillAV)
				{
					skillAVIndex = i;
					break;
				}
			}

			// No index for the linked skill.
			if (skillAVIndex == -1)
			{
				DBG("FAIL: No skill AV for {}.", a_skillbook->GetName());
				return false;
			}

			const float avLvl = menuCoopActorPtr->GetBaseActorValue(skillAV);
			// Update serialized skill base/increments list entry and increment skill AV level.
			// Must have serializable data.
			const auto iter = glob.serializablePlayerData.find(menuCoopActorPtr->formID);
			if (iter == glob.serializablePlayerData.end())
			{
				DBG
				(
					"FAIL: Could not get serializable data for {} (0x{:X}).",
					menuCoopActorPtr->GetName(),
					menuCoopActorPtr->formID
				);
				return false;
			}

			auto& skillList = 
			(
				isShared ? 
				iter->second->skillBaseLevelsList :
				iter->second->skillLevelIncreasesList
			);
			skillList[skillAVIndex]++;
			menuCoopActorPtr->SetBaseActorValue(skillAV, avLvl + 1);

			// Adjust crafting/learning points.
			if (isShared)
			{
				glob.craftingPointsGlob->value -= 1;
			}
			else
			{
				glob.learningPointsGlob->value -= 1;
			}

			// Notify player of how many points remain after leveling.
			RE::DebugMessageBox
			(
				fmt::format
				(
					"[ALYSLC]\n{} increased to {}! {} Points left: {}",
					skillName,
					avLvl + 1,
					isShared ? "Crafting" : "Learning",
					isShared ? glob.craftingPointsGlob->value : glob.learningPointsGlob->value
				).c_str()
			);

			// Remove consumed book.
			containerRefrPtr->RemoveItem
			(
				a_skillbook, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
			);
			// Refresh item list.
			shouldRefreshMenu = true;

			return true;
		}
		else
		{
			// Book is a +2 learning points or +1 memory point book.
			// These books can be used by P1 to gain points for the entire party 
			// and aren't handled here.
			if (a_skillbook->formID == 0xCE135 && a_skillbook->formID == 0x12E1FC)
			{
				DBG("FAIL: Is +2 learning or +1 memory point book.");
				return false;
			}

			if (!pointsAvailable)
			{
				// No points to use.
				RE::DebugMessageBox
				(
					fmt::format
					(
						"[ALYSLC]\nYou do not have enough {} Points!", 
						isShared ? "Crafting" : "Learning"
					).c_str()
				);
			}
			else
			{
				if (skillAV != RE::ActorValue::kNone)
				{
					// Not the correct tier.
					RE::DebugMessageBox
					(
						fmt::format
						(
							"[ALYSLC]\nYou already have developed this skill too well "
							"to benefit from this learning/crafting book!", 
							a_skillbook->GetName()
						).c_str()
					);
				}
				else
				{
					// Not valid for leveling.
					RE::DebugMessageBox
					(
						fmt::format
						(
							"[ALYSLC]\nCannot use {} to level up a skill.",
							a_skillbook->GetName()
						).c_str()
					);
				}
			}
			
			DBG
			(
				"FAIL: No points available: {}, not correct tier: {}, not valid to level: {}.",
				!pointsAvailable,
				pointsAvailable && skillAV != RE::ActorValue::kNone,
				pointsAvailable && skillAV == RE::ActorValue::kNone
			);
			return false;
		}

		return false;
	}

	void MenuInputManager::ProcessBarterMenuButtonInput(const RE::BSFixedString& a_userEvent)
	{
		// Handle BarterMenu input.

		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		auto ue = RE::UserEvents::GetSingleton(); 
		if (!ue || !barterMenu || !barterMenu->itemList) 
		{
			return;
		}
		
		// Signal to refresh the menu, which updates our equip state.
		shouldRefreshMenu = true;
		// Game will already reload the list of entries when processing emulated input,
		// such as pressing 'A' to sell an item.
		shouldReloadMenuEntries = false;
	}

	void MenuInputManager::ProcessBookMenuButtonInput(const RE::BSFixedString& a_userEvent)
	{
		// Handle BookMenu input.
		
		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		auto ue = RE::UserEvents::GetSingleton();
		if (!ue || !bookMenu)
		{
			return;
		}

		// Take book with the 'Accept' bind.
		// Emulate other inputs.
		if (a_userEvent != ue->accept)
		{
			return;
		}

		// Opened while the book refr is in worldspace, 
		// and not in the player's inventory.
		// Can loot and give to P1.
		auto bookRef = bookMenu->GetTargetReference(); 
		if (!bookRef || !bookRef->GetObjectReference())
		{
			return;
		}
		
		// Need to send a 'Cancel' input event to close the menu first,
		// then loot the book with P1.
		// The container changed event handler will then move the book from P1
		// to the companion player controlling menus.
		auto cancelIDCode = GetMappedKey
		(
			ue->cancel, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		);
		// Press the bind.
		std::unique_ptr<RE::InputEvent* const> buttonEvent = 
		(
			std::make_unique<RE::InputEvent* const>
			(
				RE::ButtonEvent::Create
				(
					RE::INPUT_DEVICE::kGamepad, ue->cancel, cancelIDCode, 1.0f, 0.0f
				)
			)
		);
		// Sent by a companion player.
		(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
		// Release the bind.
		std::unique_ptr<RE::InputEvent* const> buttonEvent2 = 
		(
			std::make_unique<RE::InputEvent* const>
			(
				RE::ButtonEvent::Create
				(
					RE::INPUT_DEVICE::kGamepad, ue->cancel, cancelIDCode, 0.0f, 1.0f
				)
			)
		);
		// Sent by a companion player.
		(*buttonEvent2.get())->AsIDEvent()->pad24 = 0xCA11;

		Util::SendInputEvent(buttonEvent);
		Util::SendInputEvent(buttonEvent2);

		if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
		{
			// Add book to P1 after exiting the menu.
			p1->PickUpObject(bookRef, 1);
		}

		// No event to handle.
		currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
	}

	void MenuInputManager::ProcessContainerMenuButtonInput(const RE::BSFixedString& a_userEvent)
	{
		// Handle ContainerMenu input.

		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		auto ue = RE::UserEvents::GetSingleton();
		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!ue || !containerMenu || !menuCoopActorPtr)
		{
			// Avoid equipping anything on P1 when there is an error.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			return;
		}

		auto containerRefr = RE::TESObjectREFR::LookupByHandle
		(
			RE::ContainerMenu::GetTargetRefHandle()
		);
		const auto& mode = containerMenu->GetContainerMode();
		bool isPickpocketing = mode == RE::ContainerMenu::ContainerMode::kPickpocket;
		RE::ItemList::Item* selectedItem = GetSelectedItem(containerMenu->itemList);
		// NOTE:
		// Inventory tab switch needs thorough testing for bugs.
		// TEST FOR THE FOLLOWING BUG: 
		// The container menu's data sometimes corrupts switching over to P1's inventory 
		// and then exiting the menu.
		// Upon re-entering the co-op player's inventory and switching to P1's inventory,
		// the item list is broken and attempting to retrieve 
		// the currently selected item returns nothing.
		
		// If this is the case, disable switch to P1 inventory,
		// since item transfer to/from P1 from/to a companion player 
		// is also supported through the Gift Menu.

		// Don't allow switching to P1's inventory when looting a container 
		// (not another player's inventory).
		/*
		if (!isCoopInventory && !isPickpocketing && a_userEvent == ue->wait)
		{
			RE::DebugMessageBox
			(
				"[ALYSLC]\nP1's inventory is not accessible to other players while looting."
			);
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}

		if (!selectedItem)
		{
			if (isPickpocketing)
			{
				// Signal to refresh the menu to show the companion pleyer's inventory items.
				// Will still show P1's inventory on tab switch if the list isn't force-refreshed.
				shouldRefreshMenu = true;
			}
			
			// Prevent equipping items on P1 when spamming the left/right equip + 
			// drop or accept binds.
			if (a_userEvent == ue->rightEquip ||
				a_userEvent == ue->leftEquip ||
				a_userEvent == ue->leftAttack ||
				a_userEvent == ue->rightAttack)
			{
				currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			}

			return;
		}
		*/
		
		// Ignore 'Accept' input events when the quantity menu is open.
		bool quantityMenuOpen = false;
		if (a_userEvent == ue->accept || a_userEvent == ue->xButton)
		{
			if (isShowingInventory)
			{
				auto boundObj = 
				(
					selectedItem && selectedItem->data.objDesc ? 
					selectedItem->data.objDesc->object : 
					nullptr
				);
				if (!boundObj)
				{
					currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
					return;
				}
				
				const auto& p = glob.coopPlayers[managerMenuPID];
				// Set extra data to use for dropping the object.
				selectedForm = boundObj;
				selectedExDataList = Util::GetEntryFrontExtraDataList(selectedItem->data.objDesc);
				DBG
				(
					"Transfer/drop: {}/{}. Input event type: {}. ExData list: {:p}.",
					a_userEvent == ue->accept, 
					a_userEvent == ue->xButton, 
					currentMenuInputEventType,
					fmt::ptr(selectedExDataList)
				);

				// Get total item count to check if the quantity menu will open 
				// when we try to transfer the item.
				RE::GFxValue entryCount{ };
				selectedItem->obj.GetMember("count", std::addressof(entryCount));
				DBG("Menu entry count: {}, inv entry count: {}.",
					entryCount.GetUInt(), selectedItem->data.GetCount());
				uint32_t totalItemCount = 
				(
					!entryCount.IsNull() && !entryCount.IsUndefined() && entryCount.GetUInt() > 0 ?
					entryCount.GetUInt() : 
					selectedItem->data.GetCount()
				);
				// Haven't figured out a Scaleform event for item transfers through menus,
				// so this will have to do.
				uint32_t chosenCount = 0;
				auto view = containerMenu->uiMovie;
				if (view && !containerMenu->root.IsNull() && !containerMenu->root.IsUndefined())
				{
					RE::GFxValue alpha{ };
					containerMenu->uiMovie->GetVariable
					(
						std::addressof(alpha), "_root.Menu_mc.itemCard.QuantitySlider_mc._alpha"
					);
					if (!alpha.IsNull() && !alpha.IsUndefined())
					{
						DBG("Alpha is {}.", alpha.GetUInt());
						quantityMenuOpen = alpha.GetUInt() > 0;
					}

					RE::GFxValue value{ };
					containerMenu->uiMovie->GetVariable
					(
						std::addressof(value), "_root.Menu_mc.itemCard.QuantitySlider_mc._value"
					);
					if (!value.IsNull() && !value.IsUndefined())
					{
						DBG("Value is {}.", value.GetUInt());
						chosenCount = value.GetUInt();
					}

					containerMenu->uiMovie->GetVariable
					(
						std::addressof(value), "_root.Menu_mc.itemCard.QuantitySlider_mc._maximum"
					);
					if (!value.IsNull() && !value.IsUndefined())
					{
						DBG("Max is {}.", value.GetUInt());
					}
				}
				
				// Get weight.
				double weight = selectedItem->data.objDesc->GetWeight();
				// Quantity menu only opens if there are at least 5 of an item and weight is not 0:
				// https://github.com/Mardoxx/skyrimui/blob/master/src/containermenu/ContainerMenu.as#L204
				if (quantityMenuOpen)
				{
					DBG("QUANTMEN: {}, {}.", boundObj->GetName(), chosenCount);
				}
				else
				{
					// Drops 1 at a time if there are 5 or under.
					chosenCount = totalItemCount <= 5 ? 1 : totalItemCount;
				}
				
				RE::GFxValue infoWeight{ };
				if (selectedItem->obj.HasMember("infoWeight"))
				{
					selectedItem->obj.GetMember("infoWeight", std::addressof(infoWeight));
					if (!infoWeight.IsNull() && !infoWeight.IsUndefined())
					{
						weight = infoWeight.GetNumber();
					}
				}

				// Clear the drop flag if the quantity menu is closed 
				// or if cancelling the drop request
				// while the quantity menu is open.
				if (a_userEvent == ue->xButton)
				{
					DBG("REQUESTED: SET DROP BIND FLAG.");
					dropBindPressed = true;
				}
				
				DBG
				(
					"User event name: {}, count to drop: {}, "
					"drop bind pressed: {}, quantmen open: {}. Weight: {}",
					a_userEvent, 
					chosenCount,
					dropBindPressed, 
					quantityMenuOpen,
					weight
				);
				
				// JANK AND ANGUISH AHEAD:
				// Have to send some info over to the RemoveItem/AddObjectToContainer hooks
				// to indicate that the player wants to drop the items
				// we are about to transfer by using the 'Accept' bind.
				// Have to distinguish between a normal transfer and a drop request
				// when both are triggered by the same bind.
				// 
				// Passing some data through the selected extra data list is not viable
				// since:
				// 1. There is no extra data list for unmodified items.
				// 2. The item gets moved from the inventory chest to P1
				// which can split the stack and result in multiple extra data lists passed 
				// via the PlayerCharacter::AddObjectToContainer() calls,
				// 3. Those lists can also vary based on how many items the player is dropping.
				// 
				// The only link we can establish, since we must transfer the item to P1 first
				// for the drop to succeed, is saving the object and number to drop.
				// 
				// Drop if the bind is pressed, or if already requested a drop with the bind
				// and now accepting the amount given by the quantity slider menu.
				// We CAN clear the flag if moving or dropping individual items
				// or the quantity menu has opened 
				// and we are about to confirm transfer of the item(s).
				if ((dropBindPressed) && (quantityMenuOpen || totalItemCount <= 5 || weight == 0.0))
				{
					dropReqPair = { boundObj, chosenCount };
					// Reset flag once drop request is fulfilled.
					dropBindPressed = false;
					DBG
					(
						"REQUESTED. RESET DROP BIND FLAG, "
						"count to drop: {}, quantmen open: {}, event: {}.",
						chosenCount, quantityMenuOpen, a_userEvent
					);
				}

				if (a_userEvent == ue->xButton)
				{	
					DBG("Attempt drop.");
					currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
					shouldRefreshMenu = false;
					shouldReloadMenuEntries = false;

					// Send an 'accept' input event to move the item to P1.
					// Save the add object call params to match against later
					// in the PlayerCharacter::AddObjectToContainer() hook 
					// and then drop the item there.
					// All this because removing the item directly here 
					// and then updating the menu's entry list 
					// does not always happen fast enough to prevent equipping 
					// the dropped item's invalid extra data list, 
					// which leads to a crash.
					auto acceptIDCode = GetMappedKey
					(
						ue->accept, 
						RE::INPUT_DEVICE::kGamepad,
						RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
					);
					// Press the bind.
					std::unique_ptr<RE::InputEvent* const> buttonEvent = 
					(
						std::make_unique<RE::InputEvent* const>
						(
							RE::ButtonEvent::Create
							(
								RE::INPUT_DEVICE::kGamepad,
								ue->accept, 
								acceptIDCode, 
								1.0f,
								0.0f
							)
						)
					);
					// Sent by a companion player.
					(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
					// Release the bind.
					std::unique_ptr<RE::InputEvent* const> buttonEvent2 = 
					(
						std::make_unique<RE::InputEvent* const>
						(
							RE::ButtonEvent::Create
							(
								RE::INPUT_DEVICE::kGamepad, 
								ue->accept, 
								acceptIDCode, 
								0.0f,
								1.0f
							)
						)
					);
					// Sent by a companion player.
					(*buttonEvent2.get())->AsIDEvent()->pad24 = 0xCA11;
					Util::SendInputEvent(buttonEvent);
					Util::SendInputEvent(buttonEvent2);

					// This may open a quantity menu, 
					// so we cannot clear the drop bind pressed flag yet.
				}
				else if (quantityMenuOpen)
				{
					currentMenuInputEventType = MenuInputEventType::kEmulateInput;
					// Refreshing the menu blocks the emulated input event from being processed.
					// Huh.
					shouldRefreshMenu = true;
					shouldReloadMenuEntries = false;
				}
				else
				{
					if (inventoryChestOpen)
					{
						// Do not emulate the 'Accept' input because we do not want to
						// transfer items to P1.
						currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
					}
					else
					{
						// Transfer through P1 with the player's inventory imported over.
						currentMenuInputEventType = MenuInputEventType::kEmulateInput;
					}
				}
			}
			else
			{
				// Take or drop through P1.
				currentMenuInputEventType = MenuInputEventType::kEmulateInput;
			}
		}
		else if (a_userEvent == ue->cancel)
		{
			// Reset flag once drop request is cancelled.
			DBG("CANCELLED: RESET DROP BIND FLAG.");
			dropBindPressed = false;
		}
		// Favorite the selected item.
		else if (a_userEvent == ue->yButton)
		{
			// Handled here; no event to send.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;

			// Companion player's inventory must be open.
			if (!isShowingInventory)
			{
				return;
			}

			// Need inventory changes to (un)favorite any selected form.
			auto inventoryChanges = containerRefr->GetInventoryChanges();
			if (!inventoryChanges)
			{
				return;
			}

			selectedForm = selectedItem ? selectedItem->data.objDesc->object : nullptr;
			// Must have a selected form to (un)favorite.
			if (!selectedForm)
			{
				return;
			}
			
			const auto& p = glob.coopPlayers[managerMenuPID];
			// Favorite the item when in the player's inventory.
			// Credit to po3 for the code to check if the item has been favorited.
			// From an older version of:
			// https://github.com/powerof3/PapyrusExtenderSSE/
			bool shouldFavorite = true;
			RE::InventoryEntryData* entryData = selectedItem->data.objDesc;
			selectedExDataList = Util::GetEntryFrontExtraDataList(entryData);
			if (entryData)
			{
				if (entryData->extraLists && !entryData->extraLists->empty())
				{
					RE::ExtraDataList* exDataList = nullptr;
					for (auto& exData : *entryData->extraLists)
					{
						// Already has favorited data, so unfavorite instead.
						auto exHotkey = exData->GetByType<RE::ExtraHotkey>();
						if (exHotkey)
						{
							// Remove hotkey to prevent lingering assignment after unfavoriting.
							exHotkey->hotkey = 
							(
								RE::ExtraHotkey::Hotkey::kUnbound
							);
							shouldFavorite = false;
							exDataList = exData;
							break;
						}
					}

					if (shouldFavorite)
					{
						exDataList = selectedExDataList;
						Util::NativeFunctions::Favorite
						(
							inventoryChanges, entryData, exDataList
						);
					}
					else
					{
						Util::NativeFunctions::Unfavorite
						(
							inventoryChanges, entryData, exDataList
						);
					}

					// Since the player's favorited physical forms have changed, 
					// update the co-op player's corresponding list of cyclable forms.
					switch (*selectedForm->formType)
					{
					case RE::FormType::Ammo:
					{
						glob.coopPlayers[managerMenuPID]->em->SetCyclableFavForms
						(
							CyclableForms::kAmmo
						);
						break;
					}
					case RE::FormType::Weapon:
					{
						glob.coopPlayers[managerMenuPID]->em->SetCyclableFavForms
						(
							CyclableForms::kWeapon
						);
						break;
					}
					default:
					{
						break;
					}
					}
				}
				else
				{
					// Entry data may not have a list of extra data, 
					// but we can still favorite the item.
					Util::NativeFunctions::Favorite
					(
						inventoryChanges, entryData, selectedExDataList
					);
				}
			}

			// Refresh menu to display the changed favorites status indicator.
			shouldRefreshMenu = true;
			shouldReloadMenuEntries = true;
		}
		else if (a_userEvent == ue->back || a_userEvent == ue->wait)
		{
			if (inventoryChestOpen)
			{
				// Switching to the Magic Menu.
				currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;

				const auto& reqP = glob.coopPlayers[glob.menuPID];
				// Companion player requesting to open their inventory.
				bool succ = glob.moarm->InsertRequest
				(
					reqP->playerID,
					InputAction::kMagicMenu, 
					SteadyClock::now(), 
					RE::MagicMenu::MENU_NAME
				);
				if (succ)
				{
					DBG
					(
						"Opening the Magic Menu for {}.", 
						reqP->coopActor->GetName()
					);
					if (auto msgQ = RE::UIMessageQueue::GetSingleton(); msgQ)
					{
						msgQ->AddMessage
						(
							RE::ContainerMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr
						);
						msgQ->AddMessage
						(
							RE::MagicMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr
						);
					}
				}
			}
			else
			{
				// Switch to container/P1's inventory.
				// Will import/export this player's inventory.
				currentMenuInputEventType = MenuInputEventType::kEmulateInput;
				DBG("Switching tabs.");
			}
		}

		bool isLeftEquip = a_userEvent == ue->leftAttack || a_userEvent == ue->leftEquip;
		bool isRightEquip = 
		(
			(a_userEvent == ue->rightAttack || a_userEvent == ue->rightEquip) ||
			(!quantityMenuOpen && a_userEvent == ue->accept && inventoryChestOpen)
		);
		if (isLeftEquip || isRightEquip)
		{
			// No event to send by default. 
			// Do not want to equip selected items onto P1 through trigger presses.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			auto obj = selectedItem ? selectedItem->data.objDesc->object : nullptr; 
			if (!obj)
			{
				return;
			}
				
			const auto& p = glob.coopPlayers[managerMenuPID];
			const auto p1 = RE::PlayerCharacter::GetSingleton(); 
			bool isEquipable = Util::IsEquipableInventoryObject(obj);
			bool isConsumable = Util::IsConsumable(obj);
			auto asBook = obj->As<RE::TESObjectBOOK>(); 
			if (!isPickpocketing && asBook && p1)
			{
				// If this is a skillbook, co-op companions will level up 
				// the accompanying skill in the activation hook after P1 uses the book.
				currentMenuInputEventType = MenuInputEventType::kEmulateInput;
			}
			else if (!isPickpocketing && !isEquipable && !isConsumable)
			{
				// You get NOTHING! You LOSE! Good DAY, sir.
				// No reason to try to equip it onto the companion player.
				currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
				DBG("Nah: {}", obj->GetName());
			}
			else if (!isPickpocketing &&
					 ALYSLC::EnderalCompat::g_installed && 
					 obj->As<RE::AlchemyItem>() && 
					 obj->As<RE::AlchemyItem>()->HasKeywordString("Lehrbuch"))
			{
				auto item = obj->As<RE::AlchemyItem>();
				// Level up with Enderal skillbook.
				bool succ = PerformEnderalSkillLevelUp(item);
				if (!succ)
				{
					// Notify the player to open their inventory and try again.
					DBG
					(
						"Failed to use Enderal skillbook '{}'.",
						item->GetName()
					);
				}
				
				// No event and we do not want P1 to use the book.
				currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			}
			else
			{
				// Only transfer if the player is not pickpocketing the item,
				// or if the pickpocket attempt was successful.
				// Can't simply emulate a trigger press for P1 
				// because even though this will attempt the pickpocket for us,
				// it will equip the item on P1, instead of the companion player, 
				// if successful.
				bool canTransfer = true;
				if (isPickpocketing)
				{
					auto refrPtr = menuContainerHandle.get();
					if (refrPtr)
					{
						canTransfer = p1->AttemptPickpocket
						(
							menuContainerHandle.get().get(), selectedItem->data.objDesc, 1
						);

						// Pickpocketing was a success! 
						// Add skill XP for P1,
						// since the above function does not do it automatically.
						if (canTransfer)
						{
							DBG("{}'s gold value: {}.", 
								obj->GetName(), 
								obj->GetGoldValue());
							p1->UseSkill
							(
								RE::ActorValue::kPickpocket, 
								obj->GetGoldValue(),
								obj
							);
						}
					}
				}

				if (canTransfer)
				{
					// Setup equip request.
					currentMenuInputEventType = MenuInputEventType::kEquipReq;
					fromContainerHandle = menuContainerHandle;
					reqEquipIndex = 
					(
						isLeftEquip ? EquipIndex::kLeftHand : EquipIndex::kRightHand
					);
					selectedForm = obj;
					placeholderMagicChanged = false;
					selectedExDataList = Util::GetEntryFrontExtraDataList
					(
						selectedItem->data.objDesc
					);

					DBG
					(
						"Selected form {}, selected inv entry: {:p}, selected exData: {:p}.",
						selectedForm ? selectedForm->GetName() : "NONE",
						fmt::ptr(selectedItem->data.objDesc),
						fmt::ptr(selectedExDataList)
					);
					RE::ExtraDataList* selectedList{ nullptr };
					if (selectedItem->data.objDesc->extraLists &&
						!selectedItem->data.objDesc->extraLists->empty())
					{
						auto listSize = std::distance
						(
							selectedItem->data.objDesc->extraLists->begin(), 
							selectedItem->data.objDesc->extraLists->end()
						);
						for (const auto list : *selectedItem->data.objDesc->extraLists)
						{
							if (!list)
							{
								continue;
							}
								
							selectedList = list;
							for (auto type = RE::ExtraDataType::kNone; 
									type <= RE::ExtraDataType::kUnkBF; 
									type = static_cast<RE::ExtraDataType>(!type + 1))
							{
								if (auto data = list->GetByType(type); data)
								{
									DBG
									(
										"Selected form {} has exData list {:p} ({}) "
										"with data {:p} of type 0x{:X}.",
										selectedForm->GetName(),
										fmt::ptr(list),
										listSize,
										fmt::ptr(data),
										type
									);
									if (type == RE::ExtraDataType::kOwnership)
									{
										auto exOwner = static_cast<RE::ExtraOwnership*>(data);
										DBG
										(
											"Owner: {} (0x{:X}).",
											exOwner && exOwner->owner ? 
											exOwner->owner->GetName() :
											"NONE",
											exOwner && exOwner->owner ? 
											exOwner->owner->formID :
											0xDEAD
										);
									}
								}
							}
						}
					}
					else
					{
						DBG("{} has no extra data lists.", selectedForm->GetName());
					}
						
					// Only need to reload menu entries 
					// if not equipping from the player's inventory and the item 
					// is not a weapon, armor piece, or ammo.
					// Refresh the menu either way.
					shouldRefreshMenu = true;
					if (isShowingInventory || 
						!glob.coopPlayers[managerMenuPID]->em->IsEquipped
						(
							selectedForm, nullptr
						))
					{
						// Refresh equip state later once the item is (un)equipped.
						lastEquipStateRefreshReqTP = SteadyClock::now();
						// Entry list size will change when equipping from an external container
						// or when using a consumable, so make sure the list is updated.
						shouldReloadMenuEntries = 
						(
							(!isShowingInventory) ||
							(
								selectedForm->IsNot
								(
									RE::FormType::Weapon, 
									RE::FormType::Armor,
									RE::FormType::Ammo
								)
							)
						);
						DBG
						(
							"Should reload player inventory lists: {}", 
							shouldReloadMenuEntries
						);
					}
					else
					{
						// Refresh right away after item removal otherwise.
						shouldReloadMenuEntries = true;
					}

					// If container reference is the player,
					// or the item is not valid, do not remove it from its container.
					if (!obj)
					{
						return;
					}

					auto containerRefrPtr = Util::GetRefrPtrFromHandle(menuContainerHandle); 
					if (!containerRefrPtr) 
					{
						return;
					}

					auto droppedInventory = containerRefrPtr->GetDroppedInventory();
					const auto iter = droppedInventory.find(obj);
					// Loot dropped inventory items from the overworld,
					// since they cannot be removed from the container directly.
					if (iter != droppedInventory.end())
					{
						const auto& countHandlePair = iter->second;
						if (countHandlePair.first > 0)
						{
							for (const auto& handle : countHandlePair.second)
							{
								if (!Util::HandleIsValid(handle))
								{
									continue;
								}
									
								DBG
								(
									"Picking up {} from {}'s dropped inventory "
									"and transferring from P1 to {}.",
									obj ? obj->GetName() : "NONE", 
									containerRefrPtr->GetName(), 
									glob.coopPlayers[managerMenuPID]->coopActor->GetName()
								);
								p1->PickUpObject(handle.get().get(), 1);
							}
						}
					}
					else if (!isShowingInventory)
					{
						auto counts = containerRefrPtr->GetInventoryCounts();
						const auto iter2 = counts.find(obj);
						if (iter2 != counts.end())
						{
							auto count = iter2->second;
							if (count > 0)
							{
								// NOTE: 
								// Move ALL directly to the menu-controlling companion player's
								// inventory chest to be equipped immediately. 
								// Delaying the transfer may cause the equip call to execute
								// before the item is transfered and the equip will fail.
								const auto& menuP = glob.coopPlayers[managerMenuPID]; 
								DBG
								(
									"Moving {} {} from {} to {}'s inventory.",
									count, 
									obj ? obj->GetName() : "NONE", 
									containerRefrPtr->GetName(), 
									menuP->coopActor->GetName()
								);
								// Move to inventory chest to add ownership exData
								// which will then move it straight to P1 
								// from the AddObjectToContainer hook.
								containerRefrPtr->RemoveItem
								(	
									obj,
									count, 
									RE::ITEM_REMOVE_REASON::kStoreInContainer,
									nullptr,
									menuP->em->inventoryChest.get()
								);
								// Extra data list changes after moving to P1/chest,
								// so grab the new front list to use for the equip.
								selectedExDataList = Util::GetEntryFrontExtraDataList
								(
									Util::GetInventoryEntryDataForObject
									(
										menuP->em->inventoryChest.get(),
										obj,
										nullptr
									)
								);
							}
						}
					}
				}
				else
				{
					// Exit the menu if caught pickpocketing.
					// No event to handle.
					currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
					auto msgQ = RE::UIMessageQueue::GetSingleton(); 
					if (msgQ)
					{
						msgQ->AddMessage
						(
							RE::ContainerMenu::MENU_NAME,
							RE::UI_MESSAGE_TYPE::kForceHide, 
							nullptr
						);
						return;
					}
				}
			}
		}
		else
		{
			// Emulate all other inputs.
			currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		}
	}

	void MenuInputManager::ProcessDialogueMenuButtonInput(const RE::BSFixedString& a_userEvent)
	{
		// Handle DialogueMenu input.
		
		// Can only use the DPad to navigate and either close the menu or choose dialogue options.
		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		auto ue = RE::UserEvents::GetSingleton();
		if (!ue || !dialogueMenu)
		{
			return;
		}

		// Block all other controls from being emulated.
		if (a_userEvent != ue->up &&
			a_userEvent != ue->down &&
			a_userEvent != ue->left &&
			a_userEvent != ue->right &&
			a_userEvent != ue->cancel && 
			a_userEvent != ue->accept)
		{
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
	}

	void MenuInputManager::ProcessFavoritesMenuButtonInput
	(
		const uint32_t a_xMask, const RE::BSFixedString& a_userEvent
	)
	{
		// Handle FavoritesMenu input.
		
		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		auto ue = RE::UserEvents::GetSingleton();
		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!ue || !favoritesMenu || !menuCoopActorPtr)
		{
			// Avoid equipping anything on P1 when there is an error.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			return;
		}

		// Attempt to equip item in the quick slot.
		if (a_userEvent == ue->pause || a_userEvent == ue->journal)
		{
			// No event to handle.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			auto taskInterface = SKSE::GetTaskInterface();
			// Can't update QS tag if task interface is invalid.
			if (!taskInterface)
			{
				return;
			}

			taskInterface->AddUITask
			(
				[this]() 
				{
					auto ui = RE::UI::GetSingleton(); 
					if (!ui)
					{
						return;
					}

					favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
					if (!favoritesMenu)
					{
						return;
					}

					auto view = favoritesMenu->uiMovie; 
					if (!view)
					{
						return;
					}

					RE::GFxValue selectedIndex;
					view->GetVariable
					(
						std::addressof(selectedIndex),
						"_root.MenuHolder.Menu_mc.itemList.selectedEntry.index"
					);
					// Index in favorites list.
					uint32_t index = static_cast<uint32_t>(selectedIndex.GetNumber());
					if (index >= favoritesMenu->favorites.size())
					{
						return;
					}

					const auto iter = favMenuIndexToEntryMap.find(index);
					if (iter == favMenuIndexToEntryMap.end())
					{
						return;
					}

					uint32_t selectedEntryNum = iter->second;
					auto form = favoritesMenu->favorites[index].item;
					if (!form)
					{
						return;
					}
					
					// Ignore non-quick slot items.
					bool isConsumable = Util::IsConsumable(form);
					bool isSpell = form->Is(RE::FormType::Spell);
					if (!isConsumable && !isSpell)
					{
						return;
					}
					
					const auto& em = glob.coopPlayers[managerMenuPID]->em;
					bool shouldEquip = false;
					// Check if the selected item is the player's current quick slot item/spell,
					// and if it is, remove the equipped tag. 
					// Otherwise, add the equipped tag.
					if (isConsumable)
					{
						em->quickSlotItem = form == em->quickSlotItem ? nullptr : form;
						shouldEquip = em->quickSlotItem;
					}
					else
					{
						em->quickSlotSpell = 
						(
							form == em->quickSlotSpell ?
							nullptr : 
							form->As<RE::SpellItem>()
						);
						shouldEquip = em->quickSlotSpell;
					}

					// Get entry for the item/spell and its text.
					RE::GFxValue entry;
					view->GetVariableArray
					(
						"_root.MenuHolder.Menu_mc.itemList.entryList", 
						selectedEntryNum, 
						std::addressof(entry), 
						1
					);
					RE::GFxValue entryText;
					entry.GetMember("text", std::addressof(entryText));
					std::string entryStr = entryText.GetString();

					if (shouldEquip)
					{
						entryStr = fmt::format("(*QS{}*) {}", isConsumable ? "I" : "S", entryStr);
						// Previously equipped item/spell's index.
						uint32_t equippedQSIndex = 
						(
							isConsumable ? em->equippedQSItemIndex : em->equippedQSSpellIndex
						);
						// Remove item/spell tag from old quick slot item/spell entry if needed.
						if (equippedQSIndex != -1 && index != equippedQSIndex)
						{
							uint32_t oldEntryNum = favMenuIndexToEntryMap[equippedQSIndex];
							RE::GFxValue oldEntry;
							view->GetVariableArray
							(
								"_root.MenuHolder.Menu_mc.itemList.entryList", 
								oldEntryNum, 
								std::addressof(oldEntry),
								1
							);
							RE::GFxValue oldEntryText;
							oldEntry.GetMember("text", std::addressof(oldEntryText));
							std::string oldEntryStr = oldEntryText.GetString();

							auto qsTagStartIndex = oldEntryStr.find("(*QS", 0);
							// If there is a QS tag applied,
							// restore the old entry text.
							if (qsTagStartIndex != std::string::npos &&
								qsTagStartIndex + qsPrefixTagLength <= oldEntryStr.length())
							{
								oldEntryStr = oldEntryStr.substr
								(
									qsTagStartIndex + qsPrefixTagLength
								);
								oldEntryText.SetString(oldEntryStr);
								oldEntry.SetMember("text", oldEntryText);
								view->SetVariableArray
								(
									"_root.MenuHolder.Menu_mc.itemList.entryList",
									oldEntryNum, 
									std::addressof(oldEntry), 
									1
								);
							}
						}
					}
					else
					{
						// 'Unequipping': remove the equipped tag from the current entry.
						auto qsTagStartIndex = entryStr.find("(*QS", 0);
						if (qsTagStartIndex != std::string::npos && 
							qsTagStartIndex + qsPrefixTagLength <= entryStr.length())
						{
							entryStr = entryStr.substr(qsTagStartIndex + qsPrefixTagLength);
						}
					}

					// Update text and entry list.
					entryText.SetString(entryStr);
					entry.SetMember("text", entryText);
					view->SetVariableArray
					(
						"_root.MenuHolder.Menu_mc.itemList.entryList", 
						selectedEntryNum, 
						std::addressof(entry),
						1
					);
					view->InvokeNoReturn
					(
						"_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0
					);

					// Set new equipped quick slot item/spell index.
					if (isConsumable)
					{
						em->equippedQSItemIndex = shouldEquip ? index : -1;
					}
					else
					{
						em->equippedQSSpellIndex = shouldEquip ? index : -1;
					}

					// Refresh equip state after (un)equipping quick slot item/spell.
					glob.coopPlayers[managerMenuPID]->em->RefreshEquipState(RefreshSlots::kAll);
				}
			);
		}
		else if (a_userEvent == ue->left || a_userEvent == ue->right)
		{
			// Changing categories clears our changes to the equip "carets", 
			// (the empty/LH/RH arrow to the left of each equipped menu entry),
			// and imports P1's item equip state. 
			// Have to reimport the companion player's equip state, 
			// but no need to refresh the cached equipped data, which has not changed.
			shouldRefreshMenu = true;
		}
		else if (a_userEvent == ue->accept)
		{
			// Ignore equip attempts with the "A" button, 
			// as emulating input here will equip this player's favorited item on P1.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
		else if (a_userEvent == ue->xButton || a_userEvent == ue->readyWeapon)
		{
			// No event to handle.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			auto taskInterface = SKSE::GetTaskInterface();
			if (!taskInterface)
			{
				return;
			}

			taskInterface->AddUITask
			(
				[this]() 
				{
					auto ui = RE::UI::GetSingleton(); 
					if (!ui)
					{
						return;
					}

					favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
					if (!favoritesMenu)
					{
						return;
					}

					auto view = favoritesMenu->uiMovie; 
					if (!view)
					{
						return;
					}

					RE::GFxValue selectedIndex;
					view->GetVariable
					(
						std::addressof(selectedIndex),
						"_root.MenuHolder.Menu_mc.itemList.selectedEntry.index"
					);
					// Index in favorites list.
					uint32_t index = static_cast<uint32_t>(selectedIndex.GetNumber());
					if (index >= favoritesMenu->favorites.size())
					{
						return;
					}

					auto form = favoritesMenu->favorites[index].item;
					if (!form)
					{
						return;
					}

					if (form->formID == 0x64B33)
					{
						if (glob.mim->managerMenuPID != -1)
						{
							const auto& p = glob.coopPlayers[glob.mim->managerMenuPID];
							p->tm->canSMORF = !p->tm->canSMORF;
							if (p->tm->canSMORF)
							{
								RE::DebugMessageBox
								(
									"A latent power suddenly compels you. Propels you?"
								);
							}
							else
							{
								RE::DebugMessageBox
								(
									"The power ebbs away and you feel grounded again."
								);
							}
						
						}
					}
				}
			);
		}
		else if (a_xMask == XINPUT_GAMEPAD_RIGHT_THUMB)
		{
			// Hotkey the selected form, if any.
			// No emulated input to send.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
		else 
		{
			bool isLeftEquip = a_userEvent == ue->leftAttack || a_userEvent == ue->leftEquip;
			bool isRightEquip = a_userEvent == ue->rightAttack || a_userEvent == ue->rightEquip;
			const auto& p = glob.coopPlayers[glob.menuPID];
			if (isLeftEquip || isRightEquip)
			{
				// Default to no event to handle because we don't want to equip anything onto P1
				// if there are any early returns below.
				currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
				auto view = favoritesMenu->uiMovie; 
				if (!view)
				{
					return;
				}

				// Get the selected form from the selected entry's index.
				RE::GFxValue selectedIndex{ };
				RE::GFxValue selectedFID{ };
				view->GetVariable
				(
					std::addressof(selectedIndex), 
					"_root.MenuHolder.Menu_mc.itemList.selectedEntry.index"
				);
				uint32_t index = static_cast<uint32_t>(selectedIndex.GetNumber());
				selectedForm = favoritesMenu->favorites[index].item;
				if (!selectedForm)
				{
					return;
				}
				
				selectedExDataList = Util::GetEntryFrontExtraDataList
				(
					favoritesMenu->favorites[index].entryData
				);
				auto boundObj = selectedForm->As<RE::TESBoundObject>();
				// If the favorited item is a spell/shout or a physical item that exists
				// in the co-op player's inventory, then attempt to equip and update the menu.
				bool equipable = 
				{
					(selectedForm->Is(RE::FormType::Shout, RE::FormType::Spell))
				};
				if (!equipable && boundObj)
				{
					auto inventory = p->em->inventoryChest->GetInventory();
					const auto iter = inventory.find(boundObj);
					equipable = 
					(
						iter != inventory.end() && 
						iter->second.first > 0
					);
				}

				if (equipable)
				{
					currentMenuInputEventType = MenuInputEventType::kEquipReq;
					fromContainerHandle = menuCoopActorHandle;
					reqEquipIndex = EquipIndex::kRightHand;
					EntryEquipState newEquipState = EntryEquipState::kNone;
					if (selectedForm->Is(RE::FormType::Spell, RE::FormType::Weapon))
					{
						auto asSpell = selectedForm->As<RE::SpellItem>();
						auto asWeapon = selectedForm->As<RE::TESObjectWEAP>();
						// Weapon or hand spell.
						if ((asWeapon) || 
							(
								asSpell && 
								asSpell->GetSpellType() == RE::MagicSystem::SpellType::kSpell
							))
						{
							auto equipType = selectedForm->As<RE::BGSEquipType>();
							if (equipType && 
								equipType->equipSlot->flags.any
								(
									RE::BGSEquipSlot::Flag::kUseAllParents
								))
							{
								// Two handed spell/weapon equip request.
								newEquipState = EntryEquipState::kBothHands;
								// Use RH equip index for 2H equip requests.
								reqEquipIndex = EquipIndex::kRightHand;
							}
							else
							{
								// One handed spell/weapon equip request.
								// Vampire Lords can only swap out their LH spell.
								// The RH life drain spell is pre-determined 
								// by player level and stays equipped.
								bool isVampireLord = 
								(
									Util::IsVampireLord
									(
										glob.coopPlayers[managerMenuPID]->coopActor.get()
									)
								);
								newEquipState = 
								(
									isLeftEquip || isVampireLord ? 
									EntryEquipState::kLH : 
									EntryEquipState::kRH
								);
								reqEquipIndex = 
								(
									isLeftEquip || isVampireLord ? 
									EquipIndex::kLeftHand : 
									EquipIndex::kRightHand
								);
							}
						}
						// Voice/power/abilities etc.
						else
						{
							// Non-hand slot equipable item.
							newEquipState = EntryEquipState::kDefault;
							reqEquipIndex = EquipIndex::kVoice;
						}
					}
					else if (selectedForm->Is
								(
								RE::FormType::Ammo, 
								RE::FormType::Armor,
								RE::FormType::Armature,
								RE::FormType::Shout
								))
					{
						// NOTE: 
						// Shield is equipped to left hand slot 
						// but uses the default entry equip caret.
						newEquipState = EntryEquipState::kDefault;
						if (selectedForm->Is(RE::FormType::Ammo)) 
						{
							reqEquipIndex = EquipIndex::kAmmo;
						}
						else if (selectedForm->Is(RE::FormType::Shout))
						{
							reqEquipIndex = EquipIndex::kVoice;
						}
						else
						{
							// Default to RH equip index (won't be used for armor equips anyways).
							reqEquipIndex = EquipIndex::kRightHand;
						}
					}

					// Signal to update equip state if not a consumable;
					// otherwise update consumable count right away.
					if (newEquipState != EntryEquipState::kNone)
					{
						lastEquipStateRefreshReqTP = SteadyClock::now();
					}
					else if (Util::IsConsumable(selectedForm))
					{
						shouldRefreshMenu = true;
					}
				
					auto spellToEquip = selectedForm->As<RE::SpellItem>();
					bool isHandSlotSpell = 
					(
						spellToEquip && 
						spellToEquip->GetSpellType() == RE::MagicSystem::SpellType::kSpell
					);
					const auto& em = glob.coopPlayers[managerMenuPID]->em;
					if (isHandSlotSpell)
					{
						// Check if a hand placeholder magic form is about to be changed.
						if (newEquipState == EntryEquipState::kRH)
						{
							placeholderMagicChanged = 
							(
								selectedForm->formID !=
								em->copiedMagicFormIDs[!PlaceholderMagicIndex::kRH]
							);
						}
						else if (newEquipState == EntryEquipState::kLH)
						{
							placeholderMagicChanged = 
							(
								selectedForm->formID !=
								em->copiedMagicFormIDs[!PlaceholderMagicIndex::kLH]
							);
						}
						else if (newEquipState == EntryEquipState::kBothHands)
						{
							placeholderMagicChanged = 
							(
								selectedForm->formID != 
								em->copiedMagicFormIDs[!PlaceholderMagicIndex::k2H]
							);
						}
					}
					else if (spellToEquip)
					{
						// Is a voice spell, no copying to perform.
						placeholderMagicChanged = false;
					}

					if (selectedForm && Util::IsConsumable(selectedForm))
					{
						auto p1 = RE::PlayerCharacter::GetSingleton();
						if (p1)
						{
							auto invCounts = p1->GetInventoryCounts();
							auto iter = invCounts.find
							(
								selectedForm->As<RE::TESBoundObject>()
							);
							uint32_t p1Count = 0;
							if (iter != invCounts.end())
							{
								DBG
								(
									"P1 has {} of {}.", iter->second, selectedForm->GetName()
								);
								p1Count = iter->second;
							}

							invCounts = p->em->inventoryChest->GetInventoryCounts();
							iter = invCounts.find
							(
								selectedForm->As<RE::TESBoundObject>()
							);
							if (iter != invCounts.end())
							{
								DBG
								(
									"Chest has {} of {}.", iter->second, selectedForm->GetName()
								);
								if (iter->second == 1 && p1Count > 0)
								{
									DBG("TIME TO REMOVE {} MUHAHAHHAHAHHAHAHAH", 
										selectedForm->GetName());
								}
							}
						}
					}

					DBG
					(
						"Selected form {} (type 0x{:X}) is equipable: {}. "
						"Placeholder magic changed: {}.",
						selectedForm->GetName(), 
						*selectedForm->formType,
						equipable,
						placeholderMagicChanged
					);
				}
				else
				{
					// Not equipable.
					// Do not attempt to equip on P1.
					currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
					selectedExDataList = nullptr;
					selectedForm = nullptr;
				}
			}
			else
			{
				// Emulate all other inputs.
				currentMenuInputEventType = MenuInputEventType::kEmulateInput;
			}
		}
	}

	void MenuInputManager::ProcessGiftMenuButtonInput(const RE::BSFixedString& a_userEvent)
	{
		// Handle GiftMenu input.
		
		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		auto ue = RE::UserEvents::GetSingleton();
		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!ue || !giftMenu || !menuCoopActorPtr)
		{
			return;
		}

		if (a_userEvent == ue->accept)
		{
			// Update equip state after the game reloads entries.
			shouldReloadMenuEntries = false;
			shouldRefreshMenu = true;
		}
	}

	void MenuInputManager::ProcessInventoryMenuButtonInput(const RE::BSFixedString& a_userEvent)
	{
		// Handle InventoryMenu input.
		
		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		auto ue = RE::UserEvents::GetSingleton();
		if (!ue || !inventoryMenu)
		{
			// Avoid equipping anything on P1 when there is an error.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			return;
		}

		// Companion players can only move through P1's inventory or exit.
		// Ignore all other input events.
		if (a_userEvent != ue->up &&
			a_userEvent != ue->down &&
			a_userEvent != ue->left &&
			a_userEvent != ue->right &&
			a_userEvent != ue->cancel)
		{
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
	}

	void MenuInputManager::ProcessLootMenuButtonInput
	(
		const uint32_t a_xMask, const RE::BSFixedString& a_userEvent
	)
	{
		// Handle LootMenu input.
		
		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		auto ue = RE::UserEvents::GetSingleton();
		if (!ue)
		{
			// Avoid equipping anything on P1 when there is an error.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			return;
		}
		
		// Button codes for QuickLootMenu binds.
		std::set<uint32_t> allowedButtonIDCodes{ };
		// Add all default QuickLootEE button codes.
		auto acceptIDCode = GetMappedKey
		(
			ue->accept, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		);
		auto cancelIDCode = GetMappedKey
		(
			ue->cancel, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		);
		auto readyWeaponIDCode = GetMappedKey
		(
			ue->readyWeapon,
			RE::INPUT_DEVICE::kGamepad, 
			RE::UserEvents::INPUT_CONTEXT_ID::kGameplay
		);

		// For QuickLootIE, check for binds with the 'QUICKLOOT_EVENT_GROUP_FLAG'
		// user event group flag set and add to the allowed codes set.
		// https://github.com/MissCorruption/QuickLootIE/blob/main/src/Input/InputManager.cpp#L106
		if (ALYSLC::QuickLootCompat::g_isQuickLootIE)
		{
			auto context = controlMap->controlMap[RE::ControlMap::InputContextID::kGameplay];
			if (context)
			{
				const auto& mappings = context->deviceMappings[RE::INPUT_DEVICE::kGamepad];
				for (const auto& mapping : mappings)
				{
					auto qlieUserEventGroupFlag = static_cast<RE::ControlMap::UEFlag>(1 << 12);
					if (mapping.userEventGroupFlag.all(qlieUserEventGroupFlag))
					{
						allowedButtonIDCodes.insert(mapping.inputKey);
					}
				}
			}
		}
		else
		{
			allowedButtonIDCodes.insert(acceptIDCode);
			allowedButtonIDCodes.insert(cancelIDCode);
			allowedButtonIDCodes.insert(readyWeaponIDCode);
		}

		// Close the LootMenu with the 'Cancel' bind.
		if (a_userEvent == ue->cancel)
		{
			// Exit menu and relinquish control when cancel bind is pressed.
			if (auto crosshairPickData = RE::CrosshairPickData::GetSingleton(); crosshairPickData)
			{
				DBG
				(
					"{} is closing LootMenu.",
					Util::HandleIsValid(menuCoopActorHandle) ?
					menuCoopActorHandle.get()->GetName() :
					"NONE"
				);
				Util::SendCrosshairEvent(nullptr);
			}

			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
		else if (glob.paInfoHolder->XIMASKS_TO_INPUT_GROUPS.at(a_xMask) != InputGroup::kDPad && 
				 !allowedButtonIDCodes.contains(glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask)))
		{
			// Ignore all button presses (do not send emulated input event)
			// that are not the DPad or the 'Cancel', 'Accept', or 'Ready Weapon' binds.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
	}

	void MenuInputManager::ProcessMagicMenuButtonInput(const RE::BSFixedString& a_userEvent)
	{
		// Handle MagicMenu input.
		
		auto magicFavorites = RE::MagicFavorites::GetSingleton();
		if (magicFavorites)
		{
			DBG("============================================");
			for (const auto magForm : magicFavorites->spells)
			{
				if (!magForm)
				{
					continue;
				}

				DBG("{}", magForm->GetName());
			}
		
			uint32_t i = 0;
			for (auto magForm : magicFavorites->hotkeys)
			{ 
				++i;
				if (!magForm)
				{
					continue;
				}

				DBG("#{}: {}", i, magForm->GetName());
			}
		}

		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		auto ue = RE::UserEvents::GetSingleton();
		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!ue || !magicMenu || !menuCoopActorPtr)
		{
			// Avoid equipping anything on P1 when there is an error.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			return;
		}

		if (a_userEvent == ue->accept)
		{
			shouldReloadMenuEntries = false;
			shouldRefreshMenu = true;
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
		else if (a_userEvent == ue->yButton)
		{
			// Save selected form to add to cyclable spells list later 
			// after the spell is favorited.
			selectedForm = GetSelectedMagicMenuSpell();
			selectedExDataList = nullptr;
			if (selectedForm) 
			{
				// Remove any assigned hotkey when (un)favoriting 
				// to prevent lingering hotkey assignments.
				auto p1 = RE::PlayerCharacter::GetSingleton();
				bool wasFavorited = Util::IsFavorited(p1, selectedForm);
				if (wasFavorited)
				{
					Util::ChangeFormHotkeyStatus
					(
						RE::PlayerCharacter::GetSingleton(), selectedForm, -1
					);
				}

				//Util::ChangeFormFavoritesStatus(p1, selectedForm, !wasFavorited);
				spellFavoriteStatusChanged = true;
				//shouldReloadMenuEntries = true;
				shouldRefreshMenu = true;
				//currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			}
		}
		else if (a_userEvent == ue->left || a_userEvent == ue->right)
		{
			// Changing categories clears our changes to the equip "carets", 
			// (the empty/LH/RH arrow to the left of each equipped menu entry),
			// and imports P1's spell equip state. 
			// Have to reimport the co-op companion player's equip state,
			// but no need to refresh the cached equipped data, which has not changed.
			shouldReloadMenuEntries = false;
			shouldRefreshMenu = true;
		}
		else if (a_userEvent == ue->back || a_userEvent == ue->wait)
		{
			// Do not switch to P1's inventory. Open this player's inventory instead.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;

			const auto& reqP = glob.coopPlayers[glob.menuPID];
			// Companion player requesting to open their inventory.
			auto intfcStr = RE::InterfaceStrings::GetSingleton();
			auto msgQ = RE::UIMessageQueue::GetSingleton(); 
			if (msgQ && intfcStr)
			{
				// Construct a custom message from the player who wants to open their inventory.
				// This message will be processed by the ContainerMenu::ProcessMessage() hook
				// and will open the menu there.
				RE::HUDData* data = static_cast<RE::HUDData*>
				(
					msgQ->CreateUIMessageData(intfcStr->hudData)
				);
				if (data)
				{
					DBG
					(
						"Opening {}'s inventory instead of P1's.", 
						reqP->coopActor->GetName()
					);
					data->crosshairRef = reqP->coopActor->GetHandle();
					data->show = false;
					data->quest = nullptr;
					data->text = "Open ALYSLC Inventory";
					data->type.set(RE::HUD_MESSAGE_TYPE::kNone);
					data->wordOfPower = nullptr;
				}

				msgQ->AddMessage
				(
					RE::MagicMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, data
				);
			}
		}
		else
		{
			bool isLeftEquip = a_userEvent == ue->leftAttack || a_userEvent == ue->leftEquip;
			bool isRightEquip = a_userEvent == ue->rightAttack || a_userEvent == ue->rightEquip;
			if (isLeftEquip || isRightEquip)
			{
				// No event to send by default. 
				// Do not want to equip selected spells onto P1 through trigger presses.
				currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
				selectedForm = GetSelectedMagicMenuSpell(); 
				selectedExDataList = nullptr;
				if (selectedForm)
				{
					auto magicItemList = reinterpret_cast<RE::ItemList*>(magicMenu->unk30); 
					if (!magicItemList)
					{
						return;
					}
					
					shouldReloadMenuEntries = false;
					shouldRefreshMenu = true;
					currentMenuInputEventType = MenuInputEventType::kEquipReq;
					fromContainerHandle = menuCoopActorHandle;
					reqEquipIndex = EquipIndex::kRightHand;
					EntryEquipState newEquipState = EntryEquipState::kNone;

					auto spellToEquip = selectedForm->As<RE::SpellItem>();
					bool isHandSlotSpell = 
					(
						spellToEquip && 
						spellToEquip->GetSpellType() == RE::MagicSystem::SpellType::kSpell
					);
					if (isHandSlotSpell)
					{
						if (spellToEquip->equipSlot->flags.any
							(
								RE::BGSEquipSlot::Flag::kUseAllParents
							))
						{
							newEquipState = EntryEquipState::kBothHands;
							// Right hand index for 2H equip req.
							reqEquipIndex = EquipIndex::kRightHand;
						}
						else
						{
							newEquipState = 
							(
								isLeftEquip ? EntryEquipState::kLH : EntryEquipState::kRH
							);
							reqEquipIndex = 
							(
								isLeftEquip ? EquipIndex::kLeftHand : EquipIndex::kRightHand
							);
						}

						// Check if the placeholder spell is about to be changed.
						const auto& em = glob.coopPlayers[managerMenuPID]->em;
						if (newEquipState == EntryEquipState::kRH)
						{
							placeholderMagicChanged = 
							(
								selectedForm->formID != 
								em->copiedMagicFormIDs[!PlaceholderMagicIndex::kRH]
							);
						}
						else if (newEquipState == EntryEquipState::kLH)
						{
							placeholderMagicChanged = 
							(
								selectedForm->formID != 
								em->copiedMagicFormIDs[!PlaceholderMagicIndex::kLH]
							);
						}
						else if (newEquipState == EntryEquipState::kBothHands)
						{
							placeholderMagicChanged = 
							(
								selectedForm->formID != 
								em->copiedMagicFormIDs[!PlaceholderMagicIndex::k2H]
							);
						}
					}
					else
					{
						newEquipState = EntryEquipState::kDefault;
						// Voice slot for power/shout/any other magic.
						reqEquipIndex = EquipIndex::kVoice;
					}
					
					// Signal to update equip states for spells.
					lastEquipStateRefreshReqTP = SteadyClock::now();
				}
				else
				{
					RE::DebugNotification
					(
						fmt::format
						(
							"Cannot equip this spell for {}", 
							menuCoopActorPtr->GetName()
						).c_str()
					);
					currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
				}
			}
			else
			{
				// Emulate all other inputs.
				currentMenuInputEventType = MenuInputEventType::kEmulateInput;
			}
		}
	}

	void MenuInputManager::SetOpenedMenu
	(
		const RE::BSFixedString& a_menuName, const bool& a_isOpened
	)
	{
		// Update this manager's menu stack, accounted-for menu names, and handled menu count.
		// Then update the opened menu type and menu pointers.

		auto newHash = Hash(a_menuName);
		auto oldHash = Hash(menuName);
		if (a_isOpened)
		{
			// Only push onto stack if the menu is not already accounted for.
			if (!menuNamesHashSet.contains(newHash))
			{
				menuNamesStack.emplace_front(a_menuName);
				// Add new menu name.
				menuNamesHashSet.insert(newHash);
			}
		}
		else
		{
			// Closing menu may not always be the most recently opened menu.
			// Example: A previously opened LootMenu closes 
			// while the ContainerMenu is atop the stack.
			if (!menuNamesStack.empty())
			{
				menuNamesStack.remove(a_menuName);
			}

			// Remove menu name.
			const auto iter = menuNamesHashSet.find(newHash);
			if (iter != menuNamesHashSet.end()) 
			{
				menuNamesHashSet.erase(iter);
			}
		}

		// Update number of managed menus.
		if (glob.coopSessionActive) 
		{
			managedCoopMenusCount = menuNamesStack.size();
		}
		else
		{
			managedCoopMenusCount = 0;
		}

		if (menuNamesStack.size() > 0)
		{
			menuName = menuNamesStack.front();
			DBG("New menu on top of the stack: {}", menuName);
		}
		else
		{
			menuName = "";
			DBG("Menu stack is now empty.");
		}

		newHash = Hash(menuName);
		// Only update menu type if a new menu is atop the stack.
		if (newHash != oldHash) 
		{
			DBG
			(
				"Getting lock. (0x{:X})", 
				std::hash<std::jthread::id>()(std::this_thread::get_id())
			);
			{
				std::unique_lock<std::mutex> lock(openedMenuMutex);
				DBG
				(
					"Lock obtained. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
				newMenuAtopStack = true;
			}

			// Have to update menu type as soon as possible after the menu opens to avoid
			// setting the container menu's container to an invalid reference. 
			// The container menu target ref handle seems to change to 0 
			// not long after the menu opens (???).
			UpdateMenuType();
		}
	}

	void MenuInputManager::RefreshCyclableSpells() const
	{
		// Based on the selected form, update cyclable favorited forms of the same type.

		if (!selectedForm)
		{
			return;
		}

		const auto& em = glob.coopPlayers[managerMenuPID]->em;
		if (selectedForm->Is(RE::FormType::Shout))
		{
			em->SetCyclableFavForms(CyclableForms::kVoice);
		}
		else if (selectedForm->Is(RE::FormType::Spell))
		{
			auto spell = selectedForm->As<RE::SpellItem>();
			auto spellType = spell->GetSpellType();
			if (spellType == RE::MagicSystem::SpellType::kVoicePower ||
				spellType == RE::MagicSystem::SpellType::kPower ||
				spellType == RE::MagicSystem::SpellType::kLesserPower)
			{
				em->SetCyclableFavForms(CyclableForms::kVoice);
			}
			else if (spellType == RE::MagicSystem::SpellType::kSpell)
			{
				em->SetCyclableFavForms(CyclableForms::kSpell);
			}
		}
	}

	void MenuInputManager::RefreshFavoritesMenuEquipState()
	{
		// Refresh displayed and/or cached equip state while in the FavoritesMenu.

		if (!favoritesMenu) 
		{
			if (auto ui = RE::UI::GetSingleton(); ui)
			{
				favoritesMenu = ui->GetMenu<RE::FavoritesMenu>();
			}

			return;
		}

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			return;
		}

		// Set favorites list and set favorites' equip states 
		// for the player controlling the menu.
		const auto& favoritesList = favoritesMenu->favorites;
		const auto& em = glob.coopPlayers[managerMenuPID]->em;
		auto favoritedFormIDsSet = em->favoritedFormIDs;
		// Maintain P1's favorites list if transformed into a Vampire Lord.
		bool isVampireLord = Util::IsVampireLord
		(
			glob.coopPlayers[managerMenuPID]->coopActor.get()
		);
		// Update cached equip states for all favorited items.

		const auto& equippedForms = em->equippedForms;
		// Clear before reconstructing below.
		favMenuIndexToEntryMap.clear();
		// Clear both because the favorites list and/or equip states of favorites 
		// may have changed significantly since the Favorites Menu was last opened.
		favEntryEquipStates.clear();
		favEntryEquipStates = std::vector<EntryEquipState>
		(
			favoritesList.size(), EntryEquipState::kNone
		);

		// Iterate through the favorites list in order, 
		// set the equipped state for each form equipped by the co-op player,
		// and get the indices of all equipped favorited forms.
		// Equip state ids:
		// 0 - unequipped
		// 1 - equipped in non-hand slot
		// 2 - equipped in LH slot
		// 3 - equipped in RH slot
		// 4 - equipped in 2H slot
		bool itemStillEquipped = false;
		bool spellStillEquipped = false;
		bool isFavoritedByCoopPlayer = false;
		for (auto i = 0; i < favoritesList.size(); ++i)
		{
			const auto favForm = favoritesList[i].item;
			const auto favInvEntry = favoritesList[i].entryData;
			// Companion player shares favorites with P1 when they are transformed
			// into a vampire lord.
			isFavoritedByCoopPlayer = 
			(
				isVampireLord || favoritedFormIDsSet.contains(favForm->formID)
			);
			if (favForm && isFavoritedByCoopPlayer)
			{
				// Weapons
				if (auto asWeap = favForm->As<RE::TESObjectWEAP>(); asWeap)
				{
					// Chest must contain the worn extra data on a list 
					// that matches the favorited list.
					auto wornList = Util::GetWornRankExtraDataList
					(
						em->inventoryChest.get(), favForm->As<RE::TESBoundObject>(), true
					);
					bool equipped = 
					(
						(
							equippedForms[!EquipIndex::kLeftHand] && 
							equippedForms[!EquipIndex::kLeftHand] == favForm
						) &&
						(
							wornList && 
							wornList ==
							Util::FindMatchingExtraDataList
							(
								em->inventoryChest.get(),
								favForm->As<RE::TESBoundObject>(), 
								favInvEntry->extraLists ? 
								favInvEntry->extraLists->front():
								nullptr
							)
						)
					);
					// Check LH and RH for the same weapon.
					// LH
					if (equipped)
					{
						// Is two-handed weapon.
						if (asWeap && 
							asWeap->equipSlot->flags.any
							(
								RE::BGSEquipSlot::Flag::kUseAllParents
							))
						{
							DBG
							(
								"{} equipped in both hands.", favForm->GetName()
							);
							favEntryEquipStates[i] = EntryEquipState::kBothHands;
						}
						// One-handed.
						else
						{
							DBG
							(
								"{} equipped in left hand.", favForm->GetName()
							);
							favEntryEquipStates[i] = 
							(
								favEntryEquipStates[i] == EntryEquipState::kRH ? 
								EntryEquipState::kBothHands : 
								EntryEquipState::kLH
							);
						}
					}
						
					wornList = Util::GetWornRankExtraDataList
					(
						em->inventoryChest.get(), favForm->As<RE::TESBoundObject>(), false
					);
					equipped = 
					(
						(
							equippedForms[!EquipIndex::kRightHand] && 
							equippedForms[!EquipIndex::kRightHand] == favForm
						) &&
						(
							wornList && 
							wornList ==
							Util::FindMatchingExtraDataList
							(
								em->inventoryChest.get(),
								favForm->As<RE::TESBoundObject>(), 
								favInvEntry->extraLists ? 
								favInvEntry->extraLists->front():
								nullptr
							)
						)
					);
					// RH
					if (equipped)
					{
						// Is two-handed weapon.
						if (asWeap && 
							asWeap->equipSlot->flags.any
							(
								RE::BGSEquipSlot::Flag::kUseAllParents
							))
						{
							DBG
							(
								"{} equipped in both hands.", favForm->GetName()
							);
							favEntryEquipStates[i] = EntryEquipState::kBothHands;
						}
						// One-handed.
						else
						{
							DBG
							(
								"{} equipped in right hand.", favForm->GetName()
							);
							favEntryEquipStates[i]= 
							(
								favEntryEquipStates[i] == EntryEquipState::kLH ? 
								EntryEquipState::kBothHands : 
								EntryEquipState::kRH
							);
						}
					}
				}
				// Spells
				else if (auto asSpell = favForm->As<RE::SpellItem>(); asSpell)
				{
					auto spellType = asSpell->GetSpellType();
					// Check left, right, and voice equip slots for matching spells.
					if (spellType == RE::MagicSystem::SpellType::kSpell)
					{
						auto lhObj = em->equippedForms[!EquipIndex::kLeftHand];
						auto rhObj = em->equippedForms[!EquipIndex::kRightHand];
						bool is2HSpell = 
						(
							lhObj && 
							lhObj->As<RE::SpellItem>() && 
							lhObj->As<RE::SpellItem>()->equipSlot == glob.bothHandsEquipSlot
						);
						// Set hand spells to copied spells 
						// if they are currently placeholder spells.
						if (is2HSpell)
						{
							auto copied2HSpell = 
							(
								em->GetCopiedMagic(PlaceholderMagicIndex::k2H)
							);
							lhObj = 
							(
								lhObj == em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? 
								copied2HSpell : 
								lhObj
							);
							rhObj = 
							(
								rhObj == em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? 
								copied2HSpell : 
								rhObj
							);
						}
						else
						{
							if (lhObj) 
							{
								lhObj = 
								(
									lhObj == 
									em->placeholderMagic[!PlaceholderMagicIndex::kLH] ?
									em->GetCopiedMagic(PlaceholderMagicIndex::kLH) : 
									lhObj
								);
							}

							if (rhObj) 
							{
								rhObj = 
								(
									rhObj == 
									em->placeholderMagic[!PlaceholderMagicIndex::kRH] ? 
									em->GetCopiedMagic(PlaceholderMagicIndex::kRH) : 
									rhObj
								);
							}
						}

						bool favEquippedLH = favForm == lhObj;
						bool favEquippedRH = favForm == rhObj;
						bool favEquippedBothH = 
						{
							(favEquippedLH && favEquippedRH) ||
							(
								favEquippedLH && 
								lhObj && 
								lhObj->As<RE::BGSEquipType>()->equipSlot == 
								glob.bothHandsEquipSlot
							) ||
							(
								favEquippedRH &&
								rhObj &&
								rhObj->As<RE::BGSEquipType>()->equipSlot == 
								glob.bothHandsEquipSlot
							)
						};

						// Both hands spell.
						if (favEquippedBothH)
						{
							DBG
							(
								"{} equipped in both hands.", favForm->GetName()
							);
							favEntryEquipStates[i] = EntryEquipState::kBothHands;
						}
						// LH spell only
						else if (favEquippedLH)
						{
							DBG
							(
								"{} equipped in left hand.", favForm->GetName()
							);
							favEntryEquipStates[i] = EntryEquipState::kLH;
						}
						// RH spell only
						else if (favEquippedRH)
						{
							DBG
							(
								"{} equipped in right hand.", favForm->GetName()
							);
							favEntryEquipStates[i] = EntryEquipState::kRH;
						}
					}
					// Voice/power spells.
					else if (spellType == RE::MagicSystem::SpellType::kAbility ||
								spellType == RE::MagicSystem::SpellType::kLesserPower ||
								spellType == RE::MagicSystem::SpellType::kPower ||
								spellType == RE::MagicSystem::SpellType::kVoicePower)
					{
						auto voiceForm = em->equippedForms[!EquipIndex::kVoice]; 
						if (favForm == voiceForm)
						{
							DBG
							(
								"{} equipped in voice slot.", favForm->GetName()
							);
							favEntryEquipStates[i] = EntryEquipState::kDefault;
						}
					}

					// Check if quick slot spell is equipped.
					auto quickSlotSpell = em->quickSlotSpell; 
					if (quickSlotSpell && quickSlotSpell == favForm)
					{
						DBG("{} equipped in quick slot.", favForm->GetName());
						em->equippedQSSpellIndex = i;
						spellStillEquipped = true;
					}
				}
				// Shouts
				else if (favForm->Is(RE::FormType::Shout))
				{
					if (em->highestShoutVarIndex != -1)
					{
						auto asShout = favForm->As<RE::TESShout>();
						auto shoutVariation = asShout->variations[em->highestShoutVarIndex];
						if (shoutVariation.spell && shoutVariation.spell == em->voiceSpell)
						{
							DBG
							(
								"{} with highest var index {} equipped in voice slot",
								asShout->GetName(), em->highestShoutVarIndex
							);
							favEntryEquipStates[i] = EntryEquipState::kDefault;
						}
					}
				}
				// Armor
				else if (favForm->Is(RE::FormType::Armature, RE::FormType::Armor))
				{
					// Chest must contain the worn extra data on a list 
					// that matches the favorited list.
					auto wornList = Util::GetWornRankExtraDataList
					(
						em->inventoryChest.get(), favForm->As<RE::TESBoundObject>(), false
					);
					bool equipped = 
					(
						wornList && 
						wornList ==
						Util::FindMatchingExtraDataList
						(
							em->inventoryChest.get(),
							favForm->As<RE::TESBoundObject>(), 
							favInvEntry->extraLists ? 
							favInvEntry->extraLists->front():
							nullptr
						)
					);
					if (equipped)
					{
						DBG
						(
							"{} (0x{:X}) armor equipped.", favForm->GetName(), favForm->formID
						);
						favEntryEquipStates[i] = EntryEquipState::kDefault;
					}
				}
				// Ammo
				else if (favForm->Is(RE::FormType::Ammo))
				{
					auto currentAmmo = equippedForms[!EquipIndex::kAmmo]; 
					if (currentAmmo && currentAmmo == favForm)
					{
						auto wornList = Util::GetWornRankExtraDataList
						(
							em->inventoryChest.get(), favForm->As<RE::TESBoundObject>(), false
						);
						bool equipped = 
						(
							wornList && 
							wornList ==
							Util::FindMatchingExtraDataList
							(
								em->inventoryChest.get(),
								favForm->As<RE::TESBoundObject>(), 
								favInvEntry->extraLists ? 
								favInvEntry->extraLists->front():
								nullptr
							)
						);
						if (equipped)
						{
							DBG
							(
								"{} (0x{:X}) ammo equipped.",
								currentAmmo->GetName(), currentAmmo->formID
							);
							favEntryEquipStates[i] = EntryEquipState::kDefault;
						}
					}
				}
				// Quick slot items: consumables.
				else if (Util::IsConsumable(favForm))
				{
					// Check if quick slot item is equipped.
					auto quickSlotItem = em->quickSlotItem;
					if (quickSlotItem && quickSlotItem == favForm)
					{
						DBG("{} equipped in quick slot.", favForm->GetName());
						em->equippedQSItemIndex = i;
						itemStillEquipped = true;
					}
				}
				else
				{
					// Everything else -- do not treat as an equipable form.
					favEntryEquipStates[i] = EntryEquipState::kNone;
				}
			}
			else
			{
				// Favorited form invalid or was not favorited by the co-op player.
				// Set equip state to 0.
				favEntryEquipStates[i] = EntryEquipState::kNone;
			}
		}

		// Cached quick slot item or spell is no longer favorited
		// and therefore not equipped, so clear as needed.
		if (!itemStillEquipped)
		{
			em->quickSlotItem = nullptr;
			em->equippedQSItemIndex = -1;
		}

		if (!spellStillEquipped)
		{
			em->quickSlotSpell = nullptr;
			em->equippedQSSpellIndex = -1;
		}
	}

	void MenuInputManager::RefreshMagicMenuEquipState()
	{
		// UNUSED FOR NOW.
		// Refresh displayed and/or cached equip state while in the MagicMenu.

		if (!magicMenu) 
		{
			if (auto ui = RE::UI::GetSingleton(); ui)
			{
				magicMenu = ui->GetMenu<RE::MagicMenu>();
			}

			return;
		}

		// Set list containing all magic forms if it is empty.
		if (magFormsList.empty())
		{
			SetMagicMenuFormsList();
		}

		auto numMagItems = magFormsList.size();
		// If still empty, return early.
		if (numMagItems == 0)
		{
			return;
		}

		// Clear out cached data before repopulating.
		magEntryEquipStates.clear();
		magEntryEquipStates = 
		(
			std::vector<EntryEquipState>(numMagItems, EntryEquipState::kNone)
		);

		const auto& em = glob.coopPlayers[managerMenuPID]->em;
		// Get copied spells in place of equipped placeholder spells.
		auto lhObj = em->equippedForms[!EquipIndex::kLeftHand];
		auto rhObj = em->equippedForms[!EquipIndex::kRightHand];
		bool is2HSpell = 
		(
			lhObj && 
			lhObj->As<RE::SpellItem>() && 
			lhObj->As<RE::SpellItem>()->equipSlot == glob.bothHandsEquipSlot
		);
		// Set hand spells to copied spells if they are currently placeholder spells.
		if (is2HSpell)
		{
			auto copied2HSpell = em->GetCopiedMagic(PlaceholderMagicIndex::k2H);
			lhObj = 
			(
				lhObj == em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? 
				copied2HSpell : 
				lhObj
			);
			rhObj = 
			(
				rhObj == em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? 
				copied2HSpell : 
				rhObj
			);
		}
		else
		{
			if (lhObj)
			{
				lhObj = 
				(
					lhObj == em->placeholderMagic[!PlaceholderMagicIndex::kLH] ?
					em->GetCopiedMagic(PlaceholderMagicIndex::kLH) :
					lhObj
				);
			}

			if (rhObj)
			{
				rhObj = 
				(
					rhObj == em->placeholderMagic[!PlaceholderMagicIndex::kRH] ?
					em->GetCopiedMagic(PlaceholderMagicIndex::kRH) : 
					rhObj
				);
			}
		}

		auto voiceForm = em->equippedForms[!EquipIndex::kVoice];
		// Set voice spell to copied spell if it is currently a placeholder spell.
		if (voiceForm && voiceForm->Is(RE::FormType::Spell))
		{
			voiceForm = 
			(
				voiceForm == em->GetPlaceholderMagic(PlaceholderMagicIndex::kVoice) ?
				em->GetCopiedMagic(PlaceholderMagicIndex::kVoice) : 
				voiceForm
			);
		}

		for (uint32_t i = 0; i < magFormsList.size(); ++i)
		{
			if (auto magForm = magFormsList[i]; magForm)
			{
				bool magEquippedLH = magForm == lhObj;
				bool magEquippedRH = magForm == rhObj;
				bool magEquippedBothH = 
				(
					(magEquippedLH && magEquippedRH) ||
					(
						magEquippedLH && 
						lhObj && 
						lhObj->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot
					) ||
					(
						magEquippedRH &&
						rhObj && 
						rhObj->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot
					)
				);
				bool magEquippedVoice = magForm == voiceForm;

				// Both hands.
				if (magEquippedBothH)
				{
					DBG("{} equipped in both hands.",  magForm->GetName());
					magEntryEquipStates[i] = EntryEquipState::kBothHands;
				}
				// LH
				else if (magEquippedLH)
				{
					DBG("{} equipped in left hand.",magForm->GetName());
					magEntryEquipStates[i] = EntryEquipState::kLH;
				}
				// RH
				else if (magEquippedRH)
				{
					DBG("{} equipped in right hand.", magForm->GetName());
					magEntryEquipStates[i] = EntryEquipState::kRH;
				}
				// Voice
				else if (magEquippedVoice)
				{
					DBG("{} equipped in voice slot.",magForm->GetName());
					magEntryEquipStates[i] = EntryEquipState::kDefault;
				}
				// No match or invalid
				else
				{
					magEntryEquipStates[i] = EntryEquipState::kNone;
				}
			}
		}
	}

	void MenuInputManager::RefreshMenu() 
	{
		// Refresh the currently opened menu.
		
		// NOTE:
		// Update the menu's item list since its size will change
		// on the addition/removal of an item.
		// Does cause lag spikes with larger item lists.
		// TBD: Figure out a more efficient way to update item lists.

		auto taskInterface = SKSE::GetTaskInterface(); 
		if ((magicMenu) || ((isShowingInventory) && (containerMenu || barterMenu || giftMenu)))
		{
			// Special case: Update equip states and can force-reload entry list
			// if the entries count changes.
			UpdateMenuEntryEquipStates(shouldReloadMenuEntries, false);
		}
		else if ((taskInterface) && (containerMenu || barterMenu || giftMenu))
		{
			// Reload entries without any additional processing afterward.
			taskInterface->AddUITask
			(
				[this]() 
				{
					auto ui = RE::UI::GetSingleton();
					if (!ui)
					{
						return;
					}
						
					if (glob.mim->openedMenuType == SupportedMenu::kContainer)
					{
						const auto& containerMenu = ui->GetMenu<RE::ContainerMenu>();
						if (!containerMenu)
						{
							return;
						}

						containerMenu->itemList->Update();
					}
					else if (glob.mim->openedMenuType == SupportedMenu::kGift)
					{
						const auto& giftMenu = ui->GetMenu<RE::GiftMenu>();
						if (!giftMenu)
						{
							return;
						}

						giftMenu->itemList->Update();
					}
				}
			);
		}

		// Send update request to have the corresponding menu's ProcessMessage() hook 
		// refresh the menu.
		if (const auto ui = RE::UI::GetSingleton(); ui && !menuNamesStack.empty())
		{
			if (auto currentMenu = ui->GetMenu(menuNamesStack.front()); currentMenu)
			{
				auto messageQueue = RE::UIMessageQueue::GetSingleton();
				if (messageQueue)
				{
					messageQueue->AddMessage
					(
						menuNamesStack.front(), RE::UI_MESSAGE_TYPE::kUpdate, nullptr
					);
				}
			}
		}
		
		// Clear flags after refresh.
		shouldRefreshMenu = false;
		shouldReloadMenuEntries = false;
	}

	void MenuInputManager::SendQueuedInputEvents()
	{
		// Chain and send queued P1 input events.

		if (queuedInputEvents.size() > 0)
		{
			// Link individual input events into a chain.
			for (uint32_t i = 0; i < queuedInputEvents.size() - 1; ++i)
			{
				(*(queuedInputEvents[i].get()))->next = *(queuedInputEvents[i + 1].get());
			}

			// Send the head.
			Util::SendInputEvent(queuedInputEvents[0]);

			// Handled now, so free input event pointers.
			for (auto& ptr : queuedInputEvents)
			{
				ptr.release();
			}

			// Clear queue when done.
			queuedInputEvents.clear();
		}
	}

	bool MenuInputManager::SetEmulatedInputEventInfo
	(
		const uint32_t& a_xMask, MenuBindInfo& a_bindInfoOut
	)
	{
		// Set menu bind info based on the XInput mask 
		// and event name derived from the given XInputMask
		// in the context of one of the open menus.
		// Return true if a valid event name and context were found.

		auto ui = RE::UI::GetSingleton();
		if (!ui)
		{
			return false;
		}

		// Send gamepad input event.
		a_bindInfoOut.device = RE::INPUT_DEVICE::kGamepad;
		// Set to invalid ID code at first.
		a_bindInfoOut.idCode = 0xFF;
		const auto iter = glob.cdh->XIMASK_TO_GAMEMASK.find(a_xMask);
		if (iter != glob.cdh->XIMASK_TO_GAMEMASK.end())
		{
			a_bindInfoOut.idCode = iter->second;
		}
		else
		{
			// Invalid XInput mask.
			return false;
		}
		
		// NOTE:
		// AE added the kMarketplace context, 
		// which incremented kFavor, kTotal, and kNone by 1.
		// Was causing a crash with the AE versions of TrueHUD and QuickLootIE, 
		// which have their menu contexts set to kNone (19), 
		// which is not a valid context defined by the SE versions of CommonLib. 
		// Clamp here to ensure the context index is valid.

		// Search for a context with a valid event name for the gamepad ID code.
		auto context = RE::UserEvents::INPUT_CONTEXT_ID::kNone;
		bool validEventNameFound = false;
		// Check current menu's context first.
		const auto& currentMenu = ui->GetMenu(menuName); 
		if (currentMenu && *currentMenu->inputContext != RE::UserEvents::INPUT_CONTEXT_ID::kNone) 
		{
			context = static_cast<RE::UserEvents::INPUT_CONTEXT_ID>
			(
				min(!RE::UserEvents::INPUT_CONTEXT_ID::kNone, !(*currentMenu->inputContext))
			);
			//DBG("Current menu {} has context {}.", menuName, context);
			if (context == RE::UserEvents::INPUT_CONTEXT_ID::kTotal)
			{
				context = RE::UserEvents::INPUT_CONTEXT_ID::kNone;
			}

			if (context != RE::UserEvents::INPUT_CONTEXT_ID::kNone)
			{
				a_bindInfoOut.eventName = GetUserEventName
				(
					a_bindInfoOut.idCode, RE::INPUT_DEVICE::kGamepad, context
				);
				validEventNameFound = Hash(a_bindInfoOut.eventName) != Hash(""sv);
			}
		}

		// Fall back to menu mode or item menu mode context.
		if (!validEventNameFound) 
		{
			// 'X' is not mapped by default in the menu context 
			// and 'Y' maps to 'DownloadAll' which does not trigger the correct menu response
			// unless in the Creations Menu (won't ever happen in co-op).
			if (a_bindInfoOut.idCode == GAME_INPUT_CODE_X ||
				a_bindInfoOut.idCode == GAME_INPUT_CODE_Y)
			{
				context = RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu;
			}
			else
			{
				context = RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode;
			}
			
			a_bindInfoOut.eventName = GetUserEventName
			(
				a_bindInfoOut.idCode, RE::INPUT_DEVICE::kGamepad, context
			);
			validEventNameFound = Hash(a_bindInfoOut.eventName) != Hash(""sv);
		}

		// Fall back to checking the menu stack for a valid bind from each menu's context.
		if (!validEventNameFound)
		{
			for (const auto& menu : ui->menuStack)
			{
				context = static_cast<RE::UserEvents::INPUT_CONTEXT_ID>
				(
					min(!RE::UserEvents::INPUT_CONTEXT_ID::kNone, !(*menu->inputContext))
				);
				if (context == RE::UserEvents::INPUT_CONTEXT_ID::kTotal)
				{
					context = RE::UserEvents::INPUT_CONTEXT_ID::kNone;
				}

				if (context != RE::UserEvents::INPUT_CONTEXT_ID::kNone)
				{
					a_bindInfoOut.eventName = GetUserEventName
					(
						a_bindInfoOut.idCode, RE::INPUT_DEVICE::kGamepad, context
					);
					if (Hash(a_bindInfoOut.eventName) != Hash(""sv))
					{
						validEventNameFound = true;
						break;
					}
				}
			}
		}

		// Fall-back to gameplay, which is a catch-all context 
		// with valid event names for all device inputs.
		if (!validEventNameFound) 
		{
			context = RE::UserEvents::INPUT_CONTEXT_ID::kGameplay;
			a_bindInfoOut.eventName = GetUserEventName
			(
				a_bindInfoOut.idCode, RE::INPUT_DEVICE::kGamepad, context
			);
			/*DBG("Gameplay context has event name {} from id code 0x{:X}.",
				a_bindInfoOut.eventName, a_bindInfoOut.idCode);*/
			validEventNameFound = Hash(a_bindInfoOut.eventName) != Hash(""sv);
		}

		// Should not happen, but bail here if there is still no valid event name.
		if (!validEventNameFound) 
		{
			DBG
			(
				"Could not get event name for XInput button mask 0x{:X} "
				"(id code: 0x{:X}) and current menu '{}'.", 
				a_xMask, a_bindInfoOut.idCode, menuName
			);
			return false;
		}

		// Set valid context here.
		/*DBG
		(
			"Chose context {}, event name {} for xMask 0x{:X}. "
			"Value, held time: {}, {}. Current event type: {}.",
			context, a_bindInfoOut.eventName, a_xMask,
			a_bindInfoOut.value,
			a_bindInfoOut.heldTimeSecs,
			currentMenuInputEventType
		);*/
		a_bindInfoOut.context = context;

		return true;
	}

	void MenuInputManager::SetMagicMenuFormsList()
	{
		// UNUSED FOR NOW.
		// Set all selectable MagicMenu forms
		// from the menu's item list.

		auto magicItemList = reinterpret_cast<RE::ItemList*>(magicMenu->unk30); 
		if (!magicItemList)
		{
			return;
		}

		// Clear old data.
		magFormsList.clear();

		// Ensure there are no duplicate FIDs.
		std::set<RE::FormID> insertedFIDs{ };
		auto numMagicItems = magicItemList->items.size();
		RE::TESForm* formToAdd = nullptr;
		for (uint32_t i = 0; i < numMagicItems; ++i)
		{
			// Clear out our found form.
			formToAdd = nullptr;
			// Initial, more-accurate attempt.
			// SKYUI ONLY: 
			// Get the form ID of the entry and then use the data handler to get the form directly.
			RE::GFxValue entry;
			magicItemList->entryList.GetElement(i, std::addressof(entry));

			RE::GFxValue entryFormId;
			entry.GetMember("formId", std::addressof(entryFormId));
			uint32_t formID = 0;
			if (entryFormId.GetNumber() != 0)
			{
				formID = static_cast<uint32_t>(entryFormId.GetNumber());
			}
			else
			{
				entry.GetMember("formID", std::addressof(entryFormId));
				if (entryFormId.GetNumber() != 0)
				{
					formID = static_cast<uint32_t>(entryFormId.GetNumber());
				}
			}

			if (formID != 0)
			{
				// Valid form found from the given FID, so insert it.
				if (formToAdd = RE::TESForm::LookupByID(formID); formToAdd)
				{
					magFormsList.push_back(formToAdd);
					continue;
				}
			}

			auto magicItem = magicItemList->items[i]; 
			if (!magicItem)
			{
				continue;
			}
			
			// Second attempt. Should be unnecessary unless SKYUI is not installed.
			// Match magic item name with known spells/shouts.
			// Will fail when multiple known spells/shouts have the same name.
			auto chosenMagicItemName = magicItem->data.GetName();
			// Match spell name with one of P1's learned spells.
			for (auto spellItem : glob.player1Actor->addedSpells)
			{
				if (strcmp(spellItem->GetName(), chosenMagicItemName) == 0 && 
					!insertedFIDs.contains(spellItem->formID))
				{
					formToAdd = spellItem;
					insertedFIDs.insert(spellItem->formID);
					break;
				}
			}

			auto p1ActorBase = glob.player1Actor->GetActorBase();
			if (!p1ActorBase)
			{
				continue;
			}

			// Match with spells that P1 has by virtue of their actor base
			if (auto spellList = p1ActorBase->actorEffects->spells; spellList)
			{
				uint32_t spellListSize = p1ActorBase->actorEffects->numSpells;
				for (uint32_t i = 0; i < spellListSize; ++i)
				{
					auto spellItem = spellList[i];
					if (strcmp(spellItem->GetName(), chosenMagicItemName) == 0 && 
						!insertedFIDs.contains(spellItem->formID))
					{
						formToAdd = spellItem;
						insertedFIDs.insert(spellItem->formID);
						break;
					}
				}
			}

			// Match with shouts that P1 has by virtue of their actor base.
			if (auto shoutList = p1ActorBase->actorEffects->shouts; shoutList)
			{
				uint32_t shoutListSize = p1ActorBase->actorEffects->numShouts;
				for (uint32_t i = 0; i < shoutListSize; ++i)
				{
					// Some unused shouts exist in P1's actor base shouts list.
					// All have one character length names.
					if (shoutList[i] && strlen(shoutList[i]->GetName()) > 1 && 
						strcmp(shoutList[i]->GetName(), chosenMagicItemName) == 0 && 
						!insertedFIDs.contains(shoutList[i]->formID))
					{
						formToAdd = shoutList[i];
						insertedFIDs.insert(shoutList[i]->formID);
						break;
					}
				}
			}

			magFormsList.push_back(formToAdd);
		}
	}

	void MenuInputManager::SetMenuControlMap()
	{
		// Initialize the co-op player's base control map for the most recently opened menu.

		// Used for ID code and event name lookups with P1's default binds.
		controlMap = RE::ControlMap::GetSingleton();
		// Save previous control map to copy the tap/hold/release states 
		// of all buttons over to the new control map.
		auto oldMenuControlMap = std::unordered_map<uint32_t, MenuBindInfo>(menuControlMap);
		// clang-format off
		menuControlMap = std::unordered_map<uint32_t, MenuBindInfo>
		(
			{
				{ XINPUT_GAMEPAD_DPAD_UP,			MenuBindInfo() },
				{ XINPUT_GAMEPAD_DPAD_DOWN,			MenuBindInfo() },
				{ XINPUT_GAMEPAD_DPAD_LEFT,			MenuBindInfo() },
				{ XINPUT_GAMEPAD_DPAD_RIGHT,		MenuBindInfo() },
				{ XINPUT_GAMEPAD_A,					MenuBindInfo() },
				{ XINPUT_GAMEPAD_B,					MenuBindInfo() },
				{ XINPUT_GAMEPAD_X,					MenuBindInfo() },
				{ XINPUT_GAMEPAD_Y,					MenuBindInfo() },
				{ XINPUT_GAMEPAD_BACK,				MenuBindInfo() },
				{ XINPUT_GAMEPAD_START,				MenuBindInfo() },
				{ XINPUT_GAMEPAD_LEFT_SHOULDER,		MenuBindInfo() },
				{ XINPUT_GAMEPAD_RIGHT_SHOULDER,	MenuBindInfo() },
				{ XINPUT_GAMEPAD_LEFT_THUMB,		MenuBindInfo() },
				{ XINPUT_GAMEPAD_RIGHT_THUMB,		MenuBindInfo() },
				{ XMASK_LT,							MenuBindInfo() },
				{ XMASK_RT,							MenuBindInfo() },
				{ XMASK_LS,							MenuBindInfo() },
				{ XMASK_RS,							MenuBindInfo() },
			}
		);
		// clang-format on

		// Copy over old in-common linked menu events 
		// so that held buttons from the previously opened
		// menu do register as presses in the new menu when it opens.
		for (const auto& [xMask, menuBindInfo] : oldMenuControlMap) 
		{
			const auto iter = menuControlMap.find(xMask);
			if (iter == menuControlMap.end()) 
			{
				continue;
			}

			iter->second = menuBindInfo;
		}
	}

	void MenuInputManager::SetMenuInputMappings()
	{
		// Initialize the menu input mappings to check for user event and input ID code pairings.
		if (controlMap)
		{
			for (auto i = 0; i < RE::UserEvents::INPUT_CONTEXT_ID::kTotal; ++i)
			{
				const auto map = controlMap->controlMap[i];
				if (!map)
				{
					continue;
				}

				for (auto j = 0; j < RE::INPUT_DEVICE::kTotal; ++j)
				{
					inputMappings[i][j].clear();
					for (auto k = 0; k < map->deviceMappings[j].size(); ++k)
					{
						const auto& ueMapping = map->deviceMappings[j][k];
						inputMappings[i][j].emplace_back
						(
							std::pair<RE::BSFixedString, uint32_t>
							(
								ueMapping.eventID, ueMapping.inputKey
							)
						);
					}
				}
			}
		}
		else
		{
			DBG("ERR: No control map. Cannot set input mappings.");
		}
	}

	void MenuInputManager::ToggleCoopPlayerMenuMode
	(
		const int32_t& a_reqDeviceID, const int32_t& a_reqPlayerID
	)
	{
		// Toggle menu mode for the given player DID and PID.

		bool shouldEnter = a_reqDeviceID != -1 && a_reqPlayerID != -1;
		if (shouldEnter) 
		{
			// Ensure that menu controller is available before starting/resuming the manager.
			XINPUT_STATE buttonState{ };
			ZeroMemory(&buttonState, sizeof(buttonState));
			if (XInputGetState(a_reqDeviceID, &buttonState) != ERROR_SUCCESS)
			{
				DBG("Got invalid menu device ID ({}). Exiting.", a_reqDeviceID);
				return;
			}

			// Set menu controlling player DID and PID.
			managerMenuDID = a_reqDeviceID;
			managerMenuPID = a_reqPlayerID;
			// Signal to run.
			RequestStateChange(ManagerState::kRunning);
		}
		else
		{
			// Signal to pause and clear out IDs.
			RequestStateChange(ManagerState::kPaused);
			managerMenuDID = -1;
			managerMenuPID = -1;
		}

		DBG
		(
			"Performed state change request. MIM is now set to {}. PID/DID: {}, {}.", 
			shouldEnter ? "running" : "paused",
			managerMenuPID,
			managerMenuDID
		);
	}

	void MenuInputManager::UpdateMenuEntryEquipStates
	(
		bool a_reloadEntries, bool a_forPlayer1
	)
	{
		// Update the player's inventory item list (Barter/Container Menu entries)
		// to reflect the items that the player has equipped.
		// Also, if requested, manually restore P1's equip state for each entry when tab-switched 
		// over to view P1's inventory (ContainerMenu only).
		// Eww, gross. If I find a more efficient way than calling ItemList::Update(),
		// I can do away with this function call.

		// Either the container menu with the controlling player's inventory showing
		// or the barter menu with the companion player's inventory imported onto P1.
		bool canUpdateEntryEquipStates = 
		(
			(barterMenu || giftMenu || magicMenu) || 
			(
				(containerMenu) && 
				(
					(a_forPlayer1 && !isShowingInventory) || 
					(!a_forPlayer1 && isShowingInventory)
				)
			)
		);
		if (!canUpdateEntryEquipStates)
		{
			return;
		}

		auto taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface)
		{
			return;
		}

		taskInterface->AddUITask
		(
			[this, a_reloadEntries, a_forPlayer1]()
			{
				auto ui = RE::UI::GetSingleton();
				if (!ui)
				{
					return;
				}

				auto containerMenu = 
				(
					glob.mim->openedMenuType == SupportedMenu::kContainer ? 
					ui->GetMenu<RE::ContainerMenu>() :
					nullptr
				);
				auto barterMenu = 
				(
					glob.mim->openedMenuType == SupportedMenu::kBarter ? 
					ui->GetMenu<RE::BarterMenu>() :
					nullptr
				);
				auto giftMenu = 
				(
					glob.mim->openedMenuType == SupportedMenu::kGift ? 
					ui->GetMenu<RE::GiftMenu>() :
					nullptr	
				);
				auto magicMenu = 
				(
					glob.mim->openedMenuType == SupportedMenu::kMagic ? 
					ui->GetMenu<RE::MagicMenu>() :
					nullptr	
				);
				if (!containerMenu && !barterMenu && !giftMenu && !magicMenu)
				{
					return;
				}

				auto menuEntryList = 
				(
					containerMenu ? 
					containerMenu->itemList :
					barterMenu ? 
					barterMenu->itemList :
					giftMenu ?
					giftMenu->itemList :
					magicMenu ?
					reinterpret_cast<RE::ItemList*>(magicMenu->unk30) :
					nullptr
				);
				if (!menuEntryList)
				{
					return;
				}

				auto view = 
				(
					containerMenu ? 
					containerMenu->uiMovie :
					barterMenu ?
					barterMenu->uiMovie :
					giftMenu ?
					giftMenu->uiMovie :
					magicMenu ? 
					magicMenu->uiMovie :
					nullptr
				);
				if (!view)
				{
					return;
				}

				const auto& p = a_forPlayer1 ? glob.coopPlayers[0] : glob.coopPlayers[glob.menuPID];
				if (menuEntryList->entryList.IsArray())
				{
					DBG
					(
						"Entry list size: {}. Item list size: {}. "
						"Reload entries: {}, for P1 : {}.",
						menuEntryList->entryList.GetArraySize(), 
						menuEntryList->items.size(),
						a_reloadEntries,
						a_forPlayer1
					);
				}
				else
				{
					return;
				}
				
				if (!a_forPlayer1 && a_reloadEntries)
				{
					DBG("Reloading list entries.");
					menuEntryList->Update();
				}
				
				if (magicMenu)
				{
					auto& magicEntryList = menuEntryList->entryList;
					RE::GFxValue numItemsGFx;
					magicEntryList.GetMember("length", std::addressof(numItemsGFx));
					double numItems = numItemsGFx.GetNumber();
					for (auto i = 0; i < numItems; ++i)
					{
						RE::GFxValue entry;
						magicEntryList.GetElement(i, std::addressof(entry));
						RE::GFxValue newEquipState;
						entry.GetMember("equipState", std::addressof(newEquipState));

						// Get copied spells in place of equipped placeholder spells.
						auto lhObj = p->coopActor->GetEquippedObject(true);
						auto rhObj = p->coopActor->GetEquippedObject(false);
						bool is2HSpell = 
						(
							lhObj && 
							lhObj->As<RE::SpellItem>() && 
							lhObj->As<RE::SpellItem>()->equipSlot == glob.bothHandsEquipSlot
						);
						// Set hand spells to copied spells if they are currently placeholder spells.
						if (is2HSpell)
						{
							auto copied2HSpell = p->em->GetCopiedMagic(PlaceholderMagicIndex::k2H);
							lhObj = 
							(
								lhObj == p->em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? 
								copied2HSpell : 
								lhObj
							);
							rhObj = 
							(
								rhObj == p->em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? 
								copied2HSpell : 
								rhObj
							);
						}
						else
						{
							if (lhObj)
							{
								lhObj = 
								(
									lhObj == p->em->placeholderMagic[!PlaceholderMagicIndex::kLH] ?
									p->em->GetCopiedMagic(PlaceholderMagicIndex::kLH) :
									lhObj
								);
							}

							if (rhObj)
							{
								rhObj = 
								(
									rhObj == p->em->placeholderMagic[!PlaceholderMagicIndex::kRH] ?
									p->em->GetCopiedMagic(PlaceholderMagicIndex::kRH) : 
									rhObj
								);
							}
						}

						auto voiceForm = p->em->equippedForms[!EquipIndex::kVoice];
						// Set voice spell to copied spell if it is currently a placeholder spell.
						if (voiceForm && voiceForm->Is(RE::FormType::Spell))
						{
							voiceForm = 
							(
								voiceForm == 
								p->em->GetPlaceholderMagic(PlaceholderMagicIndex::kVoice) ?
								p->em->GetCopiedMagic(PlaceholderMagicIndex::kVoice) : 
								voiceForm
							);
						}

						RE::TESForm* magForm = nullptr;
						RE::GFxValue entryFormId{ };
						entry.GetMember("formId", std::addressof(entryFormId));
						uint32_t formID = 0;
						if (entryFormId.GetNumber() != 0)
						{
							formID = static_cast<uint32_t>(entryFormId.GetNumber());
						}
						else
						{
							entry.GetMember("formID", std::addressof(entryFormId));
							if (entryFormId.GetNumber() != 0)
							{
								formID = static_cast<uint32_t>(entryFormId.GetNumber());
							}
						}

						if (formID != 0)
						{
							// Valid form found from the given FID, so insert it.
							magForm = RE::TESForm::LookupByID(formID); 
						}

						if (!magForm)
						{
							auto magicItem = menuEntryList->items[i]; 
							if (!magicItem)
							{
								continue;
							}

							// Second attempt. Should be unnecessary unless SKYUI is not installed.
							// Match magic item name with known spells/shouts.
							// Will fail when multiple known spells/shouts have the same name.
							auto chosenMagicItemName = magicItem->data.GetName();
							// Match spell name with one of P1's learned spells.
							for (auto spellItem : glob.player1Actor->addedSpells)
							{
								if (strcmp(spellItem->GetName(), chosenMagicItemName) == 0)
								{
									magForm = spellItem;
									break;
								}
							}

							auto p1ActorBase = glob.player1Actor->GetActorBase();
							if (!p1ActorBase)
							{
								continue;
							}

							// Match with spells that P1 has by virtue of their actor base
							if (auto spellList = p1ActorBase->actorEffects->spells; spellList)
							{
								uint32_t spellListSize = p1ActorBase->actorEffects->numSpells;
								for (uint32_t i = 0; i < spellListSize; ++i)
								{
									auto spellItem = spellList[i];
									if (strcmp(spellItem->GetName(), chosenMagicItemName) == 0)
									{
										magForm = spellItem;
										break;
									}
								}
							}

							// Match with shouts that P1 has by virtue of their actor base.
							if (auto shoutList = p1ActorBase->actorEffects->shouts; shoutList)
							{
								uint32_t shoutListSize = p1ActorBase->actorEffects->numShouts;
								for (uint32_t i = 0; i < shoutListSize; ++i)
								{
									// Some unused shouts exist in P1's actor base shouts list.
									// All have one character length names.
									if (shoutList[i] && strlen(shoutList[i]->GetName()) > 1 && 
										strcmp(shoutList[i]->GetName(), chosenMagicItemName) == 0)
									{
										magForm = shoutList[i];
										break;
									}
								}
							}
						}
						
						if (!magForm)
						{
							DBG("ERR: Could not find magic item at index {}.", i);
							continue;
						}

						bool magEquippedLH = magForm == lhObj;
						bool magEquippedRH = magForm == rhObj;
						bool magEquippedBothH = 
						(
							(magEquippedLH && magEquippedRH) ||
							(
								magEquippedLH && 
								lhObj && 
								lhObj->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot
							) ||
							(
								magEquippedRH &&
								rhObj && 
								rhObj->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot
							)
						);
						bool magEquippedVoice = magForm == voiceForm;

						EntryEquipState equipState = EntryEquipState::kNone;
						// Both hands.
						if (magEquippedBothH)
						{
							DBG("{} equipped in both hands.",  magForm->GetName());
							equipState = EntryEquipState::kBothHands;
						}
						// LH
						else if (magEquippedLH)
						{
							DBG("{} equipped in left hand.",magForm->GetName());
							equipState = EntryEquipState::kLH;
						}
						// RH
						else if (magEquippedRH)
						{
							DBG("{} equipped in right hand.", magForm->GetName());
							equipState = EntryEquipState::kRH;
						}
						// Voice
						else if (magEquippedVoice)
						{
							DBG("{} equipped in voice slot.",magForm->GetName());
							equipState = EntryEquipState::kDefault;
						}
						// No match or invalid
						else
						{
							equipState = EntryEquipState::kNone;
						}

						// Set new equip state.
						newEquipState.SetNumber(static_cast<double>(equipState));
						// Apply updated entry and list.
						entry.SetMember("equipState", newEquipState);
						magicEntryList.SetElement(i, entry);
						menuEntryList->view->SetVariable("entryList", magicEntryList);
					}

					// Update the magic entry list.
					/*view->InvokeNoReturn
					(
						"_root.Menu_mc.inventoryLists.itemList.UpdateList", nullptr, 0
					);
					DBG("Refreshed magic menu equip state.");*/
				}
				else
				{
					// Inventory on display in the menu.
					// When the player chest's inventory is copied over to P1, 
					// we use P1 as the inventory refr.
					const auto menuInvRefr = 
					(
						glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory) ? 
						RE::PlayerCharacter::GetSingleton() :
						p->em->inventoryChest.get()
					);
					const auto playerInventory = p->coopActor->GetInventory();
					// Maps chest extra data lists to the equip state we should set for them.
					std::unordered_map<RE::ExtraDataList*, EntryEquipState> 
					exListToEquipStateMap{ };
					// Since unmodified items have no extra data lists, 
					// we cannot map entry equip state to their extra data lists 
					// and will instead use the corresponding chest bound object.
					std::unordered_map<RE::TESBoundObject*, EntryEquipState> 
					unmodifiedObjToEquipStateMap{ };
					for (const auto& [boundObj, countInvEntryPair] : playerInventory)
					{
						if (!boundObj ||
							countInvEntryPair.first <= 0 || 
							!countInvEntryPair.second ||
							!countInvEntryPair.second->extraLists)
						{
							continue;
						}

						// GAH! below.
						const auto equipType = boundObj->As<RE::BGSEquipType>();
						for (auto exDataList : *countInvEntryPair.second->extraLists)
						{
							if (!exDataList)
							{
								continue;
							}

							auto worn = exDataList->HasType<RE::ExtraWorn>();
							auto wornLeft = exDataList->HasType<RE::ExtraWornLeft>();
							if (!worn && !wornLeft)
							{
								continue;
							}

							// Functionally equivalent to a nullptr exDataList (0 intrinsic types),
							// indicating an unmodified item.
							bool equivToUnmodifiedList = Util::AreIntrinsicallyEquivalentExDataLists
							(
								exDataList, nullptr
							);
							RE::ExtraDataList* matchingChestList = nullptr;
							if (worn && wornLeft)
							{
								if (a_forPlayer1)
								{
									DBG
									(
										"{}: P1 list {:p} equipped in LH/RH.",
										boundObj->GetName(),
										equivToUnmodifiedList ? "unmodified" : "modified",
										fmt::ptr(exDataList)
									);
									exListToEquipStateMap.insert
									(
										{ exDataList, EntryEquipState::kBothHands }
									);
								}
								else
								{
									// Prioritize matching with a list that has the corresponding 
									// worn exRank data.
									auto matchingChestList = Util::FindMatchingExtraDataList
									(
										menuInvRefr, boundObj, exDataList
									);
									if (!matchingChestList)
									{
										DBG
										(
											"{}: No worn exRank matching chest list for {} "
											"player list {:p} equipped in LH/RH. "
											"Checking for a matching list "
											"without worn exRank data.",
											boundObj->GetName(),
											equivToUnmodifiedList ? "unmodified" : "modified",
											fmt::ptr(exDataList)
										);
										matchingChestList = Util::FindMatchingExtraDataList
										(
											menuInvRefr, boundObj, exDataList
										);
									}

									// Matching data, cache straight away.
									if (matchingChestList)
									{
										DBG
										(
											"{}: Matching chest list {:p} for {} player list {:p} "
											"equipped in LH/RH.",
											boundObj->GetName(),
											fmt::ptr(matchingChestList),
											equivToUnmodifiedList ? "unmodified" : "modified",
											fmt::ptr(exDataList)
										);
										exListToEquipStateMap.insert
										(
											{ matchingChestList, EntryEquipState::kBothHands }
										);
									}
									else
									{
										// Save the bound object if the same as an unmodified list.
										if (equivToUnmodifiedList)
										{
											DBG
											(
												"{}: No chest list for unmodified player list {:p} "
												"equipped in LH/RH.",
												boundObj->GetName(),
												fmt::ptr(exDataList)
											);
											unmodifiedObjToEquipStateMap.insert
											(
												{ boundObj, EntryEquipState::kBothHands }
											);
										}
										else
										{
											// Uh-oh, my cruddy matching game has failed.
											// Will not show as equipped.
											DBG
											(
												"{}: MATCH FAILURE: "
												"No chest list for modified player list {:p} "
												"equipped in LH/RH.",
												boundObj->GetName(),
												fmt::ptr(exDataList)
											);
										}
									}
								}
							}
							else if (worn)
							{
								if (a_forPlayer1)
								{
									DBG
									(
										"{}: P1 list {:p} equipped in RH/default slot.",
										boundObj->GetName(),
										equivToUnmodifiedList ? "unmodified" : "modified",
										fmt::ptr(exDataList)
									);
									if (!equipType)
									{
										exListToEquipStateMap.insert
										(
											{ exDataList, EntryEquipState::kDefault }
										);
									} 
									else if (equipType->equipSlot == glob.bothHandsEquipSlot)
									{
										exListToEquipStateMap.insert
										(
											{ exDataList, EntryEquipState::kBothHands }
										);
									}
									else if (equipType->equipSlot == glob.shieldEquipSlot)
									{
										exListToEquipStateMap.insert
										(
											{ exDataList, EntryEquipState::kDefault }
										);
									}
									else if (boundObj->As<RE::TESObjectARMO>() ||
											 boundObj->As<RE::TESObjectARMA>())
									{
										exListToEquipStateMap.insert
										(
											{ exDataList, EntryEquipState::kDefault }
										);
									}
									else
									{
										auto iter = exListToEquipStateMap.find(exDataList);
										// Already present in another hand, so change to both hands.
										if (iter != exListToEquipStateMap.end() &&
											iter->second == EntryEquipState::kLH)
										{
											iter->second = EntryEquipState::kBothHands;
										}
										else
										{
											exListToEquipStateMap.insert
											(
												{ exDataList, EntryEquipState::kRH }
											);
										}
									}
								}
								else
								{
									// Prioritize matching with a list that has the corresponding 
									// worn exRank data.
									auto matchingChestList = Util::GetWornRankExtraDataList
									(
										menuInvRefr, boundObj, false
									);
									if (!matchingChestList)
									{
										DBG
										(
											"{}: No worn exRank matching chest list for {} "
											"player list {:p} equipped in RH/default slot. "
											"Checking for a matching list "
											"without worn exRank data.",
											boundObj->GetName(),
											equivToUnmodifiedList ? "unmodified" : "modified",
											fmt::ptr(exDataList)
										);
										matchingChestList = Util::FindMatchingExtraDataList
										(
											menuInvRefr, boundObj, exDataList
										);
									}
								
									// Matching data, cache straight away.
									if (matchingChestList)
									{
										DBG
										(
											"{}: Matching chest list {:p} for {} player list {:p} "
											"equipped in RH/default slot.",
											boundObj->GetName(),
											fmt::ptr(matchingChestList),
											equivToUnmodifiedList ? "unmodified" : "modified",
											fmt::ptr(exDataList)
										);
										if (!equipType)
										{
											exListToEquipStateMap.insert
											(
												{ matchingChestList, EntryEquipState::kDefault }
											);
										} 
										else if (equipType->equipSlot == glob.bothHandsEquipSlot)
										{
											exListToEquipStateMap.insert
											(
												{ matchingChestList, EntryEquipState::kBothHands }
											);
										}
										else if (equipType->equipSlot == glob.shieldEquipSlot)
										{
											exListToEquipStateMap.insert
											(
												{ matchingChestList, EntryEquipState::kDefault }
											);
										}
										else if (boundObj->As<RE::TESObjectARMO>() ||
												 boundObj->As<RE::TESObjectARMA>())
										{
											exListToEquipStateMap.insert
											(
												{ matchingChestList, EntryEquipState::kDefault }
											);
										}
										else
										{
											auto iter = exListToEquipStateMap.find
											(
												matchingChestList
											);
											// Already present in another hand, 
											// so change to both hands.
											if (iter != exListToEquipStateMap.end() &&
												iter->second == EntryEquipState::kLH)
											{
												iter->second = EntryEquipState::kBothHands;
											}
											else
											{
												exListToEquipStateMap.insert
												(
													{ matchingChestList, EntryEquipState::kRH }
												);
											}
										}
									}
									else
									{
										// Save the bound object if the same as an unmodified list.
										if (equivToUnmodifiedList)
										{
											DBG
											(
												"{}: No chest list for unmodified player list {:p} "
												"equipped in RH/default slot.",
												boundObj->GetName(),
												fmt::ptr(exDataList)
											);
											if (!equipType)
											{
												unmodifiedObjToEquipStateMap.insert
												(
													{ boundObj, EntryEquipState::kDefault }
												);
											} 
											else if (equipType->equipSlot == 
													 glob.bothHandsEquipSlot)
											{
												unmodifiedObjToEquipStateMap.insert
												(
													{ boundObj, EntryEquipState::kBothHands }
												);
											}
											else if (equipType->equipSlot == glob.shieldEquipSlot)
											{
												unmodifiedObjToEquipStateMap.insert
												(
													{ boundObj, EntryEquipState::kDefault }
												);
											}
											else if (boundObj->As<RE::TESObjectARMO>() ||
													 boundObj->As<RE::TESObjectARMA>())
											{
												unmodifiedObjToEquipStateMap.insert
												(
													{ boundObj, EntryEquipState::kDefault }
												);
											}
											else
											{
												auto iter = unmodifiedObjToEquipStateMap.find
												(
													boundObj
												);
												// Already present in another hand, 
												// so change to both hands.
												if (iter != unmodifiedObjToEquipStateMap.end() &&
													iter->second == EntryEquipState::kLH)
												{
													iter->second = EntryEquipState::kBothHands;
												}
												else
												{
													unmodifiedObjToEquipStateMap.insert
													(
														{ boundObj, EntryEquipState::kRH }
													);
												}
											}
										}
										else
										{
											// Uh-oh, my cruddy matching game has failed.
											// Will not show as equipped.
											DBG
											(
												"{}: MATCH FAILURE: "
												"No chest list for modified player list {:p} "
												"equipped in RH/default slot.",
												boundObj->GetName(),
												fmt::ptr(exDataList)
											);
										}
									}
								}
							}
							else
							{
								if (a_forPlayer1)
								{
									DBG
									(
										"{}: P1 list {:p} equipped in LH slot.",
										boundObj->GetName(),
										equivToUnmodifiedList ? "unmodified" : "modified",
										fmt::ptr(exDataList)
									);
									if (!equipType)
									{
										exListToEquipStateMap.insert
										(
											{ exDataList, EntryEquipState::kDefault }
										);
									} 
									else if (equipType->equipSlot == glob.bothHandsEquipSlot)
									{
										exListToEquipStateMap.insert
										(
											{ exDataList, EntryEquipState::kBothHands }
										);
									}
									else if (equipType->equipSlot == glob.shieldEquipSlot)
									{
										exListToEquipStateMap.insert
										(
											{ exDataList, EntryEquipState::kDefault }
										);
									}
									else if (boundObj->As<RE::TESObjectLIGH>())
									{
										exListToEquipStateMap.insert
										(
											{ exDataList, EntryEquipState::kDefault }
										);
									}
									else
									{
										auto iter = exListToEquipStateMap.find(exDataList);
										// Already present in another hand, so change to both hands.
										if (iter != exListToEquipStateMap.end() &&
											iter->second == EntryEquipState::kRH)
										{
											iter->second = EntryEquipState::kBothHands;
										}
										else
										{
											exListToEquipStateMap.insert
											(
												{ exDataList, EntryEquipState::kLH }
											);
										}
									}
								}
								else
								{
									// Prioritize matching with a list that has the corresponding 
									// worn exRank data.
									auto matchingChestList = Util::GetWornRankExtraDataList
									(
										menuInvRefr, boundObj, true
									);
									if (!matchingChestList)
									{
										DBG
										(
											"{}: No worn exRank matching chest list for {} "
											"player list {:p} equipped in LH. "
											"Checking for a matching list "
											"without worn exRank data.",
											boundObj->GetName(),
											equivToUnmodifiedList ? "unmodified" : "modified",
											fmt::ptr(exDataList)
										);
										matchingChestList = Util::FindMatchingExtraDataList
										(
											menuInvRefr, boundObj, exDataList
										);
									}
								
									// Matching data, cache straight away.
									if (matchingChestList)
									{
										DBG
										(
											"{}: Matching chest list {:p} for {} player list {:p} "
											"equipped in LH.",
											boundObj->GetName(),
											fmt::ptr(matchingChestList),
											equivToUnmodifiedList ? "unmodified" : "modified",
											fmt::ptr(exDataList)
										);
										if (!equipType)
										{
											exListToEquipStateMap.insert
											(
												{ matchingChestList, EntryEquipState::kDefault }
											);
										} 
										else if (equipType->equipSlot == glob.bothHandsEquipSlot)
										{
											exListToEquipStateMap.insert
											(
												{ matchingChestList, EntryEquipState::kBothHands }
											);
										}
										else if (equipType->equipSlot == glob.shieldEquipSlot)
										{
											exListToEquipStateMap.insert
											(
												{ matchingChestList, EntryEquipState::kDefault }
											);
										}
										else if (boundObj->As<RE::TESObjectLIGH>())
										{
											exListToEquipStateMap.insert
											(
												{ matchingChestList, EntryEquipState::kDefault }
											);
										}
										else
										{
											auto iter = exListToEquipStateMap.find
											(
												matchingChestList
											);
											// Already present in another hand, 
											// so change to both hands.
											if (iter != exListToEquipStateMap.end() &&
												iter->second == EntryEquipState::kRH)
											{
												iter->second = EntryEquipState::kBothHands;
											}
											else
											{
												exListToEquipStateMap.insert
												(
													{ matchingChestList, EntryEquipState::kLH }
												);
											}
										}
									}
									else
									{
										// Save the bound object if the same as an unmodified list.
										if (equivToUnmodifiedList)
										{
											DBG
											(
												"{}: No chest list for unmodified player list {:p} "
												"equipped in LH.",
												boundObj->GetName(),
												fmt::ptr(exDataList)
											);
											if (!equipType)
											{
												unmodifiedObjToEquipStateMap.insert
												(
													{ boundObj, EntryEquipState::kDefault }
												);
											} 
											else if (equipType->equipSlot == 
													 glob.bothHandsEquipSlot)
											{
												unmodifiedObjToEquipStateMap.insert
												(
													{ boundObj, EntryEquipState::kBothHands }
												);
											}
											else if (equipType->equipSlot == glob.shieldEquipSlot)
											{
												unmodifiedObjToEquipStateMap.insert
												(
													{ boundObj, EntryEquipState::kDefault }
												);
											}
											else if (boundObj->As<RE::TESObjectLIGH>())
											{
												unmodifiedObjToEquipStateMap.insert
												(
													{ boundObj, EntryEquipState::kDefault }
												);
											}
											else
											{
												auto iter = unmodifiedObjToEquipStateMap.find
												(
													boundObj
												);
												// Already present in another hand, 
												// so change to both hands.
												if (iter != unmodifiedObjToEquipStateMap.end() &&
													iter->second == EntryEquipState::kRH)
												{
													iter->second = EntryEquipState::kBothHands;
												}
												else
												{
													unmodifiedObjToEquipStateMap.insert
													(
														{ boundObj, EntryEquipState::kLH }
													);
												}
											}
										}
										else
										{
											// Uh-oh, my cruddy matching game has failed.
											// Will not show as equipped.
											DBG
											(
												"{}: MATCH FAILURE: "
												"No chest list for modified player list {:p} "
												"equipped in RH/default slot.",
												boundObj->GetName(),
												fmt::ptr(exDataList)
											);
										}
									}
								}
							}
						}
					}

					for (auto i = 0; i < menuEntryList->items.size(); ++i)
					{
						const auto item = menuEntryList->items[i];
						if (!item || !item->data.objDesc || !item->data.objDesc->object)
						{
							continue;
						}
					
						const auto boundObj = item->data.objDesc->object;
						// Not equipable (no caret to left of entry), skiiip.
						if (!Util::IsEquipableInventoryObject(boundObj))
						{
							continue;
						}
	
						// Continue early if we can't get the entry for some reason.
						RE::GFxValue entry{ };
						menuEntryList->entryList.GetElement(i, std::addressof(entry));
						if (entry.IsNull() || entry.IsUndefined())
						{
							continue;
						}

						EntryEquipState equipState = EntryEquipState::kNone;
						if (item->data.objDesc->extraLists && 
							!item->data.objDesc->extraLists->empty())
						{
							for (auto exDataList : *item->data.objDesc->extraLists)
							{
								if (!exDataList)
								{
									continue;
								}

								const auto iter = exListToEquipStateMap.find(exDataList);
								if (iter != exListToEquipStateMap.end())
								{
									equipState = iter->second;
									DBG
									(
										"{} (#{}): MODIFIED version {:p} equipped. "
										"Equip state: {}.",
										boundObj->GetName(), i, fmt::ptr(exDataList), !equipState
									);
								}
							}
						}
						else
						{
							const auto iter = unmodifiedObjToEquipStateMap.find(boundObj);
							if (iter != unmodifiedObjToEquipStateMap.end())
							{
								equipState = iter->second;
								DBG
								(
									"{} (#{}): UNMODIFIED version equipped. Equip state: {}.",
									boundObj->GetName(), i, !equipState
								);
							}
						}
					
						if (equipState != EntryEquipState::kNone)
						{
							RE::GFxValue index{ };
							entry.GetMember("itemIndex", std::addressof(index));
							DBG
							(
								"{} (#{}), (list index {}) in item list. "
								"Set equip state to {}.",
								boundObj->GetName(),
								i,
								index.GetUInt(),
								!equipState
							);
						}
								
						entry.SetMember("equipState", equipState);
						// Apply updated entry to the list.
						menuEntryList->entryList.SetElement(i, entry);

						// Diagnostics below, not for you, P1.
						if (a_forPlayer1 || !p->em->IsEquipped(boundObj, nullptr, false, true))
						{
							continue;
						}
			
						if (item->data.objDesc->extraLists)
						{
							for (auto exDataList : *item->data.objDesc->extraLists)
							{
								if (!exDataList)
								{
									continue;
								}
					
								DBG
								(
									"{} has exData list {:p}.",
									boundObj->GetName(),
									fmt::ptr(exDataList)
								);
								for (auto type = RE::ExtraDataType::kNone; 
									type <= RE::ExtraDataType::kUnkBF; 
									type = static_cast<RE::ExtraDataType>(!type + 1))
								{
									if (auto data = exDataList->GetByType(type); data)
									{
										DBG
										(
											"{} has exData list {:p} "
											"with data {:p} of type 0x{:X}.",
											boundObj->GetName(),
											fmt::ptr(exDataList),
											fmt::ptr(data),
											type
										);
									}
								}
							}
						}
					}
				}

				// Update the container entry list.
				// Applies our equip state changes but does NOT change the entries
				// or item counts.
				menuEntryList->root.Invoke("UpdateList");
			}
		);
	}

	void MenuInputManager::UpdateFavoritedConsumableCount
	(
		RE::TESForm* a_selectedForm, uint32_t a_selectedIndex
	)
	{
		// Update the shown amount of the given consumable at the given menu index.
		
		// Can't update entry count if task interface is invalid.
		auto taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface)
		{
			return;
		}

		taskInterface->AddUITask
		(
			[this, a_selectedForm, a_selectedIndex]() 
			{

				RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
				if (!menuCoopActorPtr || !a_selectedForm)
				{
					return;
				}

				auto ui = RE::UI::GetSingleton(); 
				if (!ui)
				{
					return;
				}
				
				favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
				if (!favoritesMenu)
				{
					return;
				}

				auto view = favoritesMenu->uiMovie; 
				if (!view)
				{
					return;
				}

				
				const auto iter = favMenuIndexToEntryMap.find(a_selectedIndex);
				if (iter == favMenuIndexToEntryMap.end())
				{
					return;
				}

				uint32_t selectedEntry = iter->second;
				const auto& p = glob.coopPlayers
				[
					GlobalCoopData::GetCoopPlayerIndex(menuCoopActorHandle)
				];
				auto invCounts = p->em->inventoryChest->GetInventoryCounts();
				int32_t newCount = -1;
				auto boundObj = a_selectedForm->As<RE::TESBoundObject>();
				if (boundObj)
				{
					const auto iter = invCounts.find(boundObj);
					if (iter != invCounts.end())
					{
						newCount = iter->second;
					}
				}

				if (newCount < 0)
				{
					return;
				}

				// Get entry and its text to search for the count bounded by parens.
				RE::GFxValue entry{ };
				view->GetVariableArray
				(
					"_root.MenuHolder.Menu_mc.itemList.entryList", 
					selectedEntry, 
					std::addressof(entry), 
					1
				);

				RE::GFxValue entryText{ };
				entry.GetMember("text", std::addressof(entryText));
				std::string entryStr = entryText.GetString();
				auto parenPos1 = entryStr.find("(");
				auto parenPos2 = entryStr.find(")");
				// Found both parens, set the new count between them.
				if (parenPos1 != std::string::npos && parenPos2 != std::string::npos)
				{
					entryStr = 
					(
						entryStr.substr(0, parenPos1) + 
						"(" + 
						std::to_string(newCount) + 
						entryStr.substr(parenPos2)
					);
				}

				// Set the new entry text, 
				// place the new entry in the list, 
				// and update the entries list.
				entryText.SetString(entryStr);
				entry.SetMember("text", entryText);
				view->SetVariableArray
				(
					"_root.MenuHolder.Menu_mc.itemList.entryList",
					selectedEntry, 
					std::addressof(entry),
					1
				);
				view->InvokeNoReturn("_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0);
			}
		);
	}

	bool MenuInputManager::UpdateMenuType()
	{
		// Set the supported opened menu type to handle based on the menu name set earlier.
		// Also get and save a pointer to the menu.
		// Return true if the new opened menu type differs from the previous one.

		DBG("Menu name: {}", menuName);
		auto oldMenuType = openedMenuType;
		auto menuNameHash = Hash(menuName);

		// Clear all menus that have special control binds handled by this manager.
		// Only one supported menu (the topmost one) handled at a time.
		barterMenu = nullptr;
		bookMenu = nullptr;
		containerMenu = nullptr;
		dialogueMenu = nullptr;
		favoritesMenu = nullptr;
		giftMenu = nullptr;
		inventoryMenu = nullptr;
		journalMenu = nullptr;
		lockpickingMenu = nullptr;
		magicMenu = nullptr;
		mapMenu = nullptr;
		sleepWaitMenu = nullptr;
		auto ui = RE::UI::GetSingleton();
		if (ui)
		{
			// Will only be one of the following.
			if (menuNameHash == Hash(RE::BarterMenu::MENU_NAME))
			{
				barterMenu = ui->GetMenu<RE::BarterMenu>();
				openedMenuType = SupportedMenu::kBarter;
			}
			else if (menuNameHash == Hash(RE::BookMenu::MENU_NAME))
			{
				bookMenu = ui->GetMenu<RE::BookMenu>();
				openedMenuType = SupportedMenu::kBook;
			}
			else if (menuNameHash == Hash(RE::ContainerMenu::MENU_NAME))
			{
				containerMenu = ui->GetMenu<RE::ContainerMenu>();
				openedMenuType = SupportedMenu::kContainer;
			}
			else if (menuNameHash == Hash(RE::DialogueMenu::MENU_NAME))
			{
				dialogueMenu = ui->GetMenu<RE::DialogueMenu>();
				openedMenuType = SupportedMenu::kDialogue;
			}
			else if (menuNameHash == Hash(RE::FavoritesMenu::MENU_NAME))
			{
				favoritesMenu = ui->GetMenu<RE::FavoritesMenu>();
				openedMenuType = SupportedMenu::kFavorites;
			}
			else if (menuNameHash == Hash(RE::GiftMenu::MENU_NAME))
			{
				giftMenu = ui->GetMenu<RE::GiftMenu>();
				openedMenuType = SupportedMenu::kGift;
			}
			else if (menuNameHash == Hash(RE::InventoryMenu::MENU_NAME))
			{
				inventoryMenu = ui->GetMenu<RE::InventoryMenu>();
				openedMenuType = SupportedMenu::kInventory;
			}
			else if (menuNameHash == Hash(RE::JournalMenu::MENU_NAME))
			{
				journalMenu = ui->GetMenu<RE::JournalMenu>();
				openedMenuType = SupportedMenu::kJournal;
			}
			else if (menuNameHash == Hash(RE::LockpickingMenu::MENU_NAME))
			{
				lockpickingMenu = ui->GetMenu<RE::LockpickingMenu>();
				openedMenuType = SupportedMenu::kLockpicking;
			}
			else if (menuNameHash == Hash(RE::MagicMenu::MENU_NAME))
			{
				magicMenu = ui->GetMenu<RE::MagicMenu>();
				openedMenuType = SupportedMenu::kMagic;
			}
			else if (menuNameHash == Hash(RE::MapMenu::MENU_NAME))
			{
				mapMenu = ui->GetMenu<RE::MapMenu>();
				openedMenuType = SupportedMenu::kMap;
			}
			else if (menuNameHash == Hash(RE::SleepWaitMenu::MENU_NAME))
			{
				sleepWaitMenu = ui->GetMenu<RE::SleepWaitMenu>();
				openedMenuType = SupportedMenu::kSleepWaitMenu;
			}
			else if (menuNameHash == Hash(GlobalCoopData::LOOT_MENU) && 
					 ui->GetMenu(GlobalCoopData::LOOT_MENU) )
			{
				openedMenuType = SupportedMenu::kLoot;
			}
			else
			{
				// Use default menu control binds in all other cases.
				openedMenuType = SupportedMenu::kDefault;
			}
		}
		else
		{
			// Default if UI was unobtainable.
			openedMenuType = SupportedMenu::kDefault;
		}

		return oldMenuType != openedMenuType;
	}

	void MenuInputManager::UpdateShowingInventoryFlags()
	{
		// Update the flag indicating that the player is viewing their inventory
		// while the container menu is open.

		if ((!glob.globalDataInit || !glob.coopSessionActive || managerMenuPID == -1) ||
			(
				openedMenuType != SupportedMenu::kContainer && 
				openedMenuType != SupportedMenu::kBarter &&
				openedMenuType != SupportedMenu::kGift
			))
		{
			inventoryChestOpen = false;
			isShowingInventory = false;
			return;
		}

		const auto& p = glob.coopPlayers[managerMenuPID];
		RE::GFxValue result{ };
		RE::TESObjectREFRPtr refrPtr{ nullptr };
		if (containerMenu && containerMenu->uiMovie)
		{
			// Viewing the open container, not P1's inventory.
			containerMenu->uiMovie->Invoke
			(
				"_root.Menu_mc.isViewingContainer", std::addressof(result), nullptr, 0
			);
			RE::TESObjectREFR::LookupByHandle(containerMenu->GetTargetRefHandle(), refrPtr);
			if (!refrPtr)
			{
				DBG("No refr associated with open container. Flags were set: {}, {}.",
					inventoryChestOpen, isShowingInventory);
				return;
			}
		}
		else if (barterMenu && barterMenu->uiMovie)
		{
			// Viewing the vendor's items, not P1's inventory.
			barterMenu->root.Invoke
			(
				"_root.Menu_mc.isViewingVendorItems", std::addressof(result), nullptr, 0
			);
		}
		else if (giftMenu)
		{
			// Always showing companion player's inventory when in the Gift Menu
			// as there is no tab switch.
			inventoryChestOpen = false;
			isShowingInventory = true;
			return;
		}
		else
		{
			return;
		}
		
		bool isNotViewingP1Inventory = result.GetBool(); 
		bool wasShowingInventory = isShowingInventory;
		inventoryChestOpen = refrPtr && refrPtr == p->em->inventoryChest;
		isShowingInventory = 
		(
			(inventoryChestOpen && isNotViewingP1Inventory) ||
			(
				!inventoryChestOpen && 
				!isNotViewingP1Inventory && 
				glob.copiedPlayerDataTypes.any(CopyablePlayerDataTypes::kInventory)
			)
		);
		if (isShowingInventory != wasShowingInventory)
		{
			DBG("Shwoing inventory flag changed to {}.", isShowingInventory);
			shouldRefreshMenu = true;
			shouldReloadMenuEntries = isShowingInventory;
		}
	}

	MenuBindInfo::MenuBindInfo() :
		idCode(0), 
		device(RE::INPUT_DEVICE::kGamepad), 
		eventName(""sv),
		context(RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode),
		eventType(MenuInputEventType::kReleasedNoEvent),
		value(0.0f),
		heldTimeSecs(0.0f),
		firstPressTP(SteadyClock::now())
	{ }
	
	MenuBindInfo::MenuBindInfo
	(
		RE::INPUT_DEVICE a_device, 
		RE::BSFixedString a_eventName, 
		RE::UserEvents::INPUT_CONTEXT_ID a_context
	) :	
		device(a_device), 
		eventName(a_eventName), 
		context(a_context),
		eventType(MenuInputEventType::kReleasedNoEvent),
		idCode
		(
			RE::ControlMap::GetSingleton() ? 
			RE::ControlMap::GetSingleton()->GetMappedKey(eventName, device, context) :
			0xFF
		),
		value(0.0f), 
		heldTimeSecs(0.0f), 
		firstPressTP(SteadyClock::now())
	{ }

	void MenuOpeningActionRequestsManager::ClearAllRequests()
	{
		// Clear out all requests for all active players.
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive) 
			{
				continue;
			}

			ClearRequests(p->playerID);
		}
	}

	void MenuOpeningActionRequestsManager::ClearRequests(const int32_t& a_playerID)
	{
		// Clear all menu opening action requests for the given player PID.

		if (a_playerID <= -1 || a_playerID >= menuOpeningActionRequests.size())
		{
			return;
		}

		menuOpeningActionRequests[a_playerID].clear();
	}

	bool MenuOpeningActionRequestsManager::InsertRequest
	(
		const int32_t& a_playerID, 
		InputAction a_fromAction, 
		SteadyClock::time_point a_timestamp,
		RE::BSFixedString a_reqMenuName,
		RE::ObjectRefHandle a_assocRefrHandle, 
		bool a_isExtRequest
	)
	{
		// Insert a menu opening action request for the given PID, 
		// associated action, menu, and form. 
		// Tag with timestamp.
		// Return true if successfullly inserted.

		if (a_playerID <= -1 || a_playerID >= menuOpeningActionRequests.size())
		{
			return false;
		}

		{
			std::unique_lock<std::mutex> lock(reqQueueMutexList[a_playerID], std::try_to_lock);
			if (lock) 
			{
				auto& reqQueue = menuOpeningActionRequests[a_playerID];
				// Remove oldest request if currently full.
				if (reqQueue.size() == maxCachedRequests)
				{
					reqQueue.pop_back();
				}

				DBG
				(
					"Adding menu opening action request for PID {}: input action: {}, "
					"menu name: {}, associated form: {}",
					a_playerID,
					a_fromAction,
					Hash(a_reqMenuName) == Hash("") ? "NONE" : a_reqMenuName,
					Util::HandleIsValid(a_assocRefrHandle) ?
					a_assocRefrHandle.get()->GetName() : 
					"NONE"
				);
				reqQueue.emplace_front
				(
					a_fromAction, a_timestamp, a_reqMenuName, a_assocRefrHandle, a_isExtRequest
				);
				return true;
			}
			else
			{
				DBG
				(
					"Failed to obtain lock. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
				return false;
			}
		}

		return false;
	}

	int32_t MenuOpeningActionRequestsManager::ResolveMenuPlayerID
	(
		const RE::BSFixedString& a_menuName, bool&& a_modifyReqQueue
	)
	{
		// Get the player ID for player that should control the given menu when it opens/closes.
		// Optionally, modify the menu requests queue for this player to re-order 
		// or clear out requests during the resolution process.

		DBG("Menu: {}, modify requests queue: {}.", a_menuName, a_modifyReqQueue);

		// Default to none.
		int32_t resolvedPID = -1;
		auto ui = RE::UI::GetSingleton();
		if (!ui) 
		{
			return resolvedPID;
		}

		// Must be a supported menu or the Lockpicking Menu, 
		// which is not handled by the MIM, but supports input from companion players 
		// via a special lockpicking task (multiple player inputs simultaneously).
		if (glob.SUPPORTED_MENU_NAMES.contains(a_menuName) || 
			a_menuName == RE::LockpickingMenu::MENU_NAME)
		{
			// First conditional check for dialogue menu control.
			// If the dialogue menu is open and no player is already controlling menus,
			// the player closest to the speaker is given control of the menu.
			// Overriden by valid requests below.
			if (glob.cam->IsRunning() && 
				Settings::bUninitializedDialogueWithClosestPlayer && 
				a_menuName == RE::DialogueMenu::MENU_NAME)
			{
				if (glob.menuPID != -1)
				{
					resolvedPID = glob.menuPID;
				}
				else if (auto menuTopicManager = RE::MenuTopicManager::GetSingleton(); 
						 menuTopicManager)
				{
					auto speakerPtr =
					(
						Util::HandleIsValid(menuTopicManager->speaker) ? 
						Util::GetRefrPtrFromHandle(menuTopicManager->speaker) : 
						Util::GetRefrPtrFromHandle(menuTopicManager->lastSpeaker)
					);
					if (speakerPtr) 
					{
						float closestDistToSpeaker = FLT_MAX;
						for (const auto& p : glob.coopPlayers)
						{
							if (!p->isActive)
							{
								continue;
							}

							auto dist = 
							(
								speakerPtr->data.location.GetDistance(p->coopActor->data.location)
							); 
							if (dist < closestDistToSpeaker) 
							{
								resolvedPID = p->playerID;
								closestDistToSpeaker = dist;
							}
						}
					}
				}
			}
			
			// Hash of the opening/closing menu's name.
			auto menuNameHash = Hash(a_menuName);
			// Message box menus consider the newest requests first.
			bool isMessageBoxMenu = a_menuName == RE::MessageBoxMenu::MENU_NAME;
			// For the 'CustomMenu', if there is a direct request or an external request,
			// consider the oldest request first.
			// Then, if there are any menu requests triggered by activation,
			// consider the newest of these requests as well.
			bool isCustomMenu = a_menuName == GlobalCoopData::CUSTOM_MENU;
			// Ignore max 5 second request lifetime for the LootMenu.
			bool ignoreReqExpiration = a_menuName == GlobalCoopData::LOOT_MENU;
			// Type for the currently chosen request determining 
			// which player should obtain control of the given menu.
			enum
			{
				kNone,
				kMaintainControl,
				kExternal,
				kOldest,
				kNewestDirect,
				kNewestIndirect
			};
			auto chosenReqType = kNone;
			// Seconds since the currently chosen req was made.
			std::optional<float> secsSinceChosenReq = std::nullopt;
			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive)
				{
					continue;
				}

				// No need to check requests for other players once one is guaranteed control.
				if (chosenReqType == kMaintainControl)
				{
					break;
				}

				const auto& pid = p->playerID;
				// Iterate through copy, even if not modifying the request queue.
				std::list<MenuOpeningActionRequests> reqQueue{ menuOpeningActionRequests[pid] };
				// Clear before adding back unfulfilled requests later.
				if (a_modifyReqQueue && !menuOpeningActionRequests[pid].empty()) 
				{
					ClearRequests(pid);
				}

				while (!reqQueue.empty()) 
				{
					// Oldest to newest.
					auto currentReq = reqQueue.back();
					// Insert for now. Will remove later after handling.
					if (a_modifyReqQueue) 
					{
						menuOpeningActionRequests[pid].emplace_front(currentReq);
					}

					float secsSinceReq = Util::GetElapsedSeconds(currentReq.timestamp);
					DBG
					(
						"Got request for {}. Menu: {}, from action: {}, assoc refr: {} (0x{:X}), "
						"ext req: {}. Seconds since request inserted: {}. "
						"Chosen req type: {}, seconds since chosen request inserted: {}. "
						"Queue size: {}.", 
						p->coopActor->GetName(), 
						currentReq.reqMenuName, 
						currentReq.fromAction,
						Util::HandleIsValid(currentReq.assocRefrHandle) ?
						currentReq.assocRefrHandle.get()->GetName() :
						"NONE",
						Util::HandleIsValid(currentReq.assocRefrHandle) ?
						currentReq.assocRefrHandle.get()->formID :
						0xDEAD,
						currentReq.isExtRequest,
						secsSinceReq, 
						chosenReqType,
						secsSinceChosenReq.has_value() ? secsSinceChosenReq.value() : -1.0f, 
						reqQueue.size()
					);

					// Hash of the current request's menu's name.
					auto reqMenuNameHash = Hash(currentReq.reqMenuName);
					// True if this request's associated menu is the same
					// as the opening/closing one.
					bool isRequestedMenu = reqMenuNameHash == menuNameHash;

					// Prioritize validity of the most recent external requests first
					// (ex. from a script).
					if (currentReq.isExtRequest)
					{
						bool isMaintainControlRequest = 
						(
							currentReq.reqMenuName == GlobalCoopData::RETAIN_MENU_CONTROL
						);
						bool isValidExtRequest = 
						(
							(isRequestedMenu) &&
							(ignoreReqExpiration || secsSinceReq < 5.0f) && 
							(
								chosenReqType != kExternal ||
								!secsSinceChosenReq.has_value() || 
								secsSinceReq > secsSinceChosenReq.value()
							)
						);
						if (isMaintainControlRequest || isValidExtRequest)
						{
							secsSinceChosenReq = secsSinceReq;
							resolvedPID = p->playerID;
							DBG
							(
								"External request: "
								"{} is in control of {}. Should maintain control: {}.",
								p->coopActor->GetName(), a_menuName, isMaintainControlRequest
							);

							// Skip the rest of the requests if maintaining control.
							if (isMaintainControlRequest)
							{
								chosenReqType = kMaintainControl;
								if (a_modifyReqQueue) 
								{
									menuOpeningActionRequests[pid].pop_front();
								}

								reqQueue.pop_back();
								break; 
							}
							else
							{
								chosenReqType = kExternal;
							}
						}
					}
					
					// Next, we want to choose the oldest valid request because, for example, 
					// if two players activate the same NPC and a menu opens 
					// to interact with that NPC, the player that activated the NPC first 
					// (oldest request) should control the menu.
					// Ignore requests that are older than 5 seconds.
					// Also look for a direct request if an indirect one is currently chosen.
					bool checkForOldestReq = 
					(
						(
							chosenReqType == kNone || 
							chosenReqType == kOldest || 
							chosenReqType == kNewestIndirect
						) &&
						(!isMessageBoxMenu) &&
						(ignoreReqExpiration || secsSinceReq < 5.0f) && 
						(
							chosenReqType == kNewestIndirect ||
							!secsSinceChosenReq.has_value() || 
							secsSinceReq > secsSinceChosenReq.value()
						)
					);
					if (checkForOldestReq) 
					{
						// NOTE: 
						// If an activator was activated and a menu opens, 
						// compare the menu's associated refr's name 
						// with the activated refr's name.
						// Can't figure out another way to check an activator's associated refr 
						// and compare to the menu's refr, and this will lead to instances
						// where the wrong player gains control of the menu 
						// if two players or more submit requests to activate objects 
						// of the same name at almost the same time.
						// Will continue looking for a direct way to link activators to references.
						
						// If the menu-associated refr the same as the request's one?
						bool isSameRefr = false;
						// Set the chosen request to the current one.
						bool setAsChosen = false;
						switch (menuNameHash)
						{
						case Hash
						(
							RE::BarterMenu::MENU_NAME.data(), RE::BarterMenu::MENU_NAME.size()
						):
						{
							// BarterMenu must be open, and request queued by activation.
							auto barterMenu = ui->GetMenu<RE::BarterMenu>();
							if (!barterMenu || currentReq.fromAction != InputAction::kActivate)
							{
								break;
							}

							auto assocRefrPtr = Util::GetRefrPtrFromHandle
							(
								currentReq.assocRefrHandle
							); 
							if (!assocRefrPtr || !assocRefrPtr->GetObjectReference())
							{
								break;
							}

							RE::TESObjectREFRPtr refrPtr{ };
							RE::TESObjectREFR::LookupByHandle
							(
								barterMenu->GetTargetRefHandle(), refrPtr
							);
							if (!refrPtr)
							{
								break;
							}

							auto baseObj = assocRefrPtr->GetObjectReference();
							isSameRefr = 
							{
								(assocRefrPtr == refrPtr) ||
								(
									baseObj->Is
									(
										RE::FormType::Activator, RE::FormType::TalkingActivator
									) &&
									Hash(assocRefrPtr->GetName()) == Hash(refrPtr->GetName())
								)
							};
							if (isSameRefr)
							{
								setAsChosen = true;
								DBG
								(
									"BarterMenu: {} is in control of bartering with {}.",
									p->coopActor->GetName(), assocRefrPtr->GetName()
								);
							}

							break;
						}
						case Hash(RE::BookMenu::MENU_NAME.data(), RE::BookMenu::MENU_NAME.size()):
						{
							// BookMenu must be open, and request queued by activation.
							auto bookMenu = ui->GetMenu<RE::BookMenu>();
							if (!bookMenu || currentReq.fromAction != InputAction::kActivate)
							{
								break;
							}

							auto assocRefrPtr = Util::GetRefrPtrFromHandle
							(
								currentReq.assocRefrHandle
							); 
							if (!assocRefrPtr || !assocRefrPtr->GetObjectReference())
							{
								break;
							}

							RE::TESObjectREFR* refrPtr = bookMenu->GetTargetReference(); 
							if (!refrPtr)
							{
								break;
							}
							
							auto baseObj = assocRefrPtr->GetObjectReference();
							isSameRefr = 
							{
								(assocRefrPtr.get() == refrPtr) ||
								(
									baseObj->Is
									(
										RE::FormType::Activator, RE::FormType::TalkingActivator
									) &&
									Hash(assocRefrPtr->GetName()) == Hash(refrPtr->GetName())
								)
							};
							if (isSameRefr)
							{
								setAsChosen = true;
								DBG
								(
									"BookMenu: {} is in control of reading {}.",
									p->coopActor->GetName(), assocRefrPtr->GetName()
								);
							}

							break;
						}
						case Hash
						(
							RE::ContainerMenu::MENU_NAME.data(), 
							RE::ContainerMenu::MENU_NAME.size()
						):
						{
							// ContainerMenu must be open.
							auto containerMenu = ui->GetMenu<RE::ContainerMenu>();
							if (!containerMenu)
							{
								break;
							}

							// Get refr associated with the ContainerMenu.
							auto refrHandle = RE::ContainerMenu::GetTargetRefHandle();
							RE::TESObjectREFRPtr refrPtr{ nullptr };
							bool succ = RE::LookupReferenceByHandle
							(
								RE::ContainerMenu::GetTargetRefHandle(), refrPtr
							);
							// Is a player inventory if the associated refr 
							// is the requesting player.
							bool isReqPlayersInventory = 
							(
								succ && 
								containerMenu->GetContainerMode() == 
								RE::ContainerMenu::ContainerMode::kNPCMode && 
								refrPtr == p->em->inventoryChest
							);
							// Container is this player's inventory 
							// and the player pressed their inventory bind 
							// or are requesting to open a container.
							if ((isReqPlayersInventory) && 
								(
									currentReq.fromAction == InputAction::kInventory || 
									isRequestedMenu
								))
							{
								setAsChosen = true;
								DBG
								(
									"ContainerMenu: {} is in control of their inventory.", 
									p->coopActor->GetName()
								);
							}
							else if (currentReq.fromAction == InputAction::kActivate)
							{
								if (!refrPtr)
								{
									break;
								}

								auto assocRefrPtr = Util::GetRefrPtrFromHandle
								(
									currentReq.assocRefrHandle
								); 
								if (!assocRefrPtr || !assocRefrPtr->GetObjectReference())
								{
									break;
								}

								// Check if the associated refrs match 
								// or if the extra ash pile refr matches 
								// the request's associated refr.
								auto baseObj = assocRefrPtr->GetObjectReference();
								auto extraAshPile = assocRefrPtr->extraList.GetAshPileRef();
								isSameRefr = 
								{
									(assocRefrPtr == refrPtr) ||
									(Util::GetRefrPtrFromHandle(extraAshPile) == refrPtr) ||
									(
										baseObj->Is
										(
											RE::FormType::Activator, RE::FormType::TalkingActivator
										) &&
										Hash(assocRefrPtr->GetName()) == Hash(refrPtr->GetName())
									)
								};
								if (isSameRefr)
								{
									setAsChosen = true;
									DBG
									(
										"ContainerMenu: {} is in control of {}'s container menu.",
										p->coopActor->GetName(), assocRefrPtr->GetName()
									);
								}
							}

							break;
						}
						case Hash
						(
							RE::CraftingMenu::MENU_NAME.data(), RE::CraftingMenu::MENU_NAME.size()
						):
						{
							// Request must be queued by activation.
							if (currentReq.fromAction != InputAction::kActivate)
							{
								break;
							}

							auto assocRefrPtr = Util::GetRefrPtrFromHandle
							(
								currentReq.assocRefrHandle
							); 
							if (!assocRefrPtr || !assocRefrPtr->GetObjectReference())
							{
								break;
							}

							// TODO:
							// Ugh. Would like to match the type of submenu furniture 
							// to the associated furniture refr's type,
							// but can't figure out a way to get the crafting menu's 
							// submenu furniture without already having P1's 
							// inventory data loaded into the menu.
							// Need the menu PID to figure out which player's inventory 
							// to copy to P1 and need the sub menu furniture to get the menu PID, 
							// but the copied inventory does not display
							// if done after the submenu data is set, 
							// and sending "Reshow" messages fail.
							//
							// So, for now, just check if any furniture with workbench data 
							// was activated and choose the oldest of these requests
							// to determine which player gets control over the crafting menu.
							// Will cause issues if multiple players activate 
							// menu-triggering furniture at almost the exact same time,
							// but the tradeoff is worth it, in my opinion, 
							// since the inventory copying occurs without a hitch afterward, 
							// and co-op companion players can use the crafting menu 
							// to adjust their own gear.
							auto asFurniture = 
							(
								assocRefrPtr->GetObjectReference()->As<RE::TESFurniture>()
							); 
							if (asFurniture && 
								*asFurniture->workBenchData.benchType != 
								RE::TESFurniture::WorkBenchData::BenchType::kNone)
							{
								setAsChosen = true;
								DBG
								(
									"CraftingMenu: "
									"{} is in control of crafting menu by activating {} "
									"with workbench data type {}.",
									p->coopActor->GetName(), 
									assocRefrPtr->GetName(), 
									*asFurniture->workBenchData.benchType
								);
							}

							break;
						}
						case Hash
						(
							RE::DialogueMenu::MENU_NAME.data(), RE::DialogueMenu::MENU_NAME.size()
						):
						{
							// Available menu topic manager 
							// and request must be queued by activation.
							auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
							if (!menuTopicManager || 
								currentReq.fromAction != InputAction::kActivate)
							{
								break;
							}

							auto assocRefrPtr = Util::GetRefrPtrFromHandle
							(
								currentReq.assocRefrHandle
							); 
							if (!assocRefrPtr || !assocRefrPtr->GetObjectReference())
							{
								break;
							}

							auto baseObj = assocRefrPtr->GetObjectReference();
							auto speakerPtr = Util::GetRefrPtrFromHandle
							(
								menuTopicManager->speaker
							);
							auto lastSpeakerPtr = Util::GetRefrPtrFromHandle
							(
								menuTopicManager->lastSpeaker
							);
							isSameRefr = 
							{
								(speakerPtr && speakerPtr == assocRefrPtr) ||
								(lastSpeakerPtr && lastSpeakerPtr == assocRefrPtr) ||
								(
									baseObj->Is
									(
										RE::FormType::Activator, RE::FormType::TalkingActivator
									) &&
									speakerPtr && 
									Hash(assocRefrPtr->GetName()) == Hash(speakerPtr->GetName())
								) ||
								(
									baseObj->Is
									(
										RE::FormType::Activator, RE::FormType::TalkingActivator
									) &&
									lastSpeakerPtr && 
									Hash(assocRefrPtr->GetName()) ==
									Hash(lastSpeakerPtr->GetName())
								)
							};
							if (isSameRefr)
							{
								setAsChosen = true;
								DBG
								(
									"DialogueMenu: {} is in control of dialogue with {}.",
									p->coopActor->GetName(), 
									speakerPtr ? 
									speakerPtr->GetName() : 
									lastSpeakerPtr ? 
									lastSpeakerPtr->GetName() :
									"NONE"
								);
							}

							break;
						}
						case Hash
						(
							RE::FavoritesMenu::MENU_NAME.data(),
							RE::FavoritesMenu::MENU_NAME.size()
						):
						{
							// Wants to access the FavoritesMenu.
							if (isRequestedMenu || 
								currentReq.fromAction == InputAction::kFavorites)
							{
								setAsChosen = true;
								DBG
								(
									"FavoritesMenu: {} is in control of menu.",
									p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash(RE::GiftMenu::MENU_NAME.data(), RE::GiftMenu::MENU_NAME.size()):
						{
							// Wants to trade with another player.
							if (isRequestedMenu || 
								currentReq.fromAction == InputAction::kTradeWithPlayer || 
								currentReq.fromAction == InputAction::kActivate)
							{
								setAsChosen = true;
								DBG
								(
									"GiftMenu: {} is in control of menu.", p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash
						(
							RE::JournalMenu::MENU_NAME.data(), RE::JournalMenu::MENU_NAME.size()
						):
						{
							// Wants to pause the game (open JournalMenu).
							if (isRequestedMenu || currentReq.fromAction == InputAction::kPause)
							{
								setAsChosen = true;
								DBG
								(
									"JournalMenu: {} is in control of menu.", 
									p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash
						(
							RE::InventoryMenu::MENU_NAME.data(),
							RE::InventoryMenu::MENU_NAME.size()
						):
						{
							// Wants to open P1's inventory directly or from the TweenMenu.
							if (isRequestedMenu ||
								currentReq.fromAction == InputAction::kInventory || 
								currentReq.fromAction == InputAction::kTweenMenu)
							{
								setAsChosen = true;
								DBG
								(
									"InventoryMenu: {} is in control of menu.",
									p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash
						(
							RE::LevelUpMenu::MENU_NAME.data(), RE::LevelUpMenu::MENU_NAME.size()
						):
						{
							// Wants to open the LevelUpMenu through the StatsMenu or TweenMenu.
							if (isRequestedMenu || 
								currentReq.fromAction == InputAction::kStatsMenu || 
								currentReq.fromAction == InputAction::kTweenMenu)
							{
								setAsChosen = true;
								DBG
								(
									"LevelUpMenu: {} is in control of menu.", 
									p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash
						(
							RE::LockpickingMenu::MENU_NAME.data(),
							RE::LockpickingMenu::MENU_NAME.size()
						):
						{
							// LockpickingMenu must be open and request queued by activation.
							auto lockpickingMenu = ui->GetMenu<RE::LockpickingMenu>(); 
							if (!lockpickingMenu || 
								currentReq.fromAction != InputAction::kActivate)
							{
								break;
							}

							auto assocRefrPtr = Util::GetRefrPtrFromHandle
							(
								currentReq.assocRefrHandle
							); 
							if (!assocRefrPtr || !assocRefrPtr->GetObjectReference())
							{
								break;
							}

							auto refrPtr = lockpickingMenu->GetTargetReference(); 
							if (!refrPtr)
							{
								break;
							}

							auto baseObj = assocRefrPtr->GetObjectReference();
							isSameRefr = 
							{
								(assocRefrPtr.get() == refrPtr) || 
								(
									baseObj->Is
									(
										RE::FormType::Activator, RE::FormType::TalkingActivator
									) && 
									Hash(assocRefrPtr->GetName()) == Hash(refrPtr->GetName())
								) 
							};
							if (isSameRefr)
							{
								setAsChosen = true;
								DBG
								(
									"LockpickingMenu: {} is in control of unlocking {}.",
									p->coopActor->GetName(), assocRefrPtr->GetName()
								);
							}

							break;
						}
						case Hash
						(
							RE::MagicMenu::MENU_NAME.data(), RE::MagicMenu::MENU_NAME.size()
						):
						{
							// Wants to open the MagicMenu.
							if (isRequestedMenu || 
								currentReq.fromAction == InputAction::kMagicMenu)
							{
								setAsChosen = true;
								DBG
								(
									"MagicMenu: {} is in control of menu.", p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash(RE::MapMenu::MENU_NAME.data(), RE::MapMenu::MENU_NAME.size()):
						{
							// Wants to open the MapMenu.
							if (isRequestedMenu || currentReq.fromAction == InputAction::kMapMenu)
							{
								setAsChosen = true;
								DBG
								(
									"MapMenu: {} is in control of menu.", p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash
						(
							RE::SleepWaitMenu::MENU_NAME.data(), 
							RE::SleepWaitMenu::MENU_NAME.size()
						):
						{
							// Wants to open the WaitMenu.
							if (isRequestedMenu || currentReq.fromAction == InputAction::kWaitMenu)
							{
								setAsChosen = true;
								DBG
								(
									"WaitMenu: {} is in control of menu.", p->coopActor->GetName()
								);
							}
							else if (currentReq.fromAction == InputAction::kActivate)
							{
								// Can also open the Wait Menu via activation of furniture
								// with the 'can sleep' flag.
								auto assocRefrPtr = Util::GetRefrPtrFromHandle
								(
									currentReq.assocRefrHandle
								); 
								if (!assocRefrPtr || !assocRefrPtr->GetObjectReference())
								{
									break;
								}

								auto asFurniture = 
								(
									assocRefrPtr->GetObjectReference()->As<RE::TESFurniture>()
								); 
								if (asFurniture && 
									asFurniture->furnFlags.all
									(
										RE::TESFurniture::ActiveMarker::kCanSleep
									))
								{
									setAsChosen = true;
									DBG
									(
										"WaitMenu: "
										"{} is in control of menu by activating {} "
										"with furniture flags 0x{:X}.",
										p->coopActor->GetName(), 
										assocRefrPtr->GetName(), 
										*asFurniture->furnFlags
									);
								}
							}

							break;
						}
						case Hash
						(
							RE::StatsMenu::MENU_NAME.data(), RE::StatsMenu::MENU_NAME.size()
						):
						{
							// Wants to open the StatsMenu directly or through the TweenMenu.
							if (isRequestedMenu || 
								currentReq.fromAction == InputAction::kStatsMenu || 
								currentReq.fromAction == InputAction::kTweenMenu)
							{
								setAsChosen = true;
								DBG
								(
									"StatsMenu: {} is in control of menu.", p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash
						(
							RE::TrainingMenu::MENU_NAME.data(), RE::TrainingMenu::MENU_NAME.size()
						):
						{
							// TrainingMenu must be open and request queued by activation.
							auto trainingMenu = ui->GetMenu<RE::TrainingMenu>();
							if (!trainingMenu || currentReq.fromAction != InputAction::kActivate)
							{
								break;
							}

							auto assocRefrPtr = Util::GetRefrPtrFromHandle
							(
								currentReq.assocRefrHandle
							); 
							if (!assocRefrPtr || !assocRefrPtr->GetObjectReference())
							{
								break;
							}

							// Check if the associated refr has the same name as the trainer too.
							auto baseObj = assocRefrPtr->GetObjectReference();
							auto trainer = trainingMenu->trainer;
							isSameRefr = 
							{ 
								(assocRefrPtr.get() == trainer) || 
								(
									baseObj->Is
									(
										RE::FormType::Activator, RE::FormType::TalkingActivator
									) && 
									trainer && 
									Hash(assocRefrPtr->GetName()) == Hash(trainer->GetName())
								)
							};
							if (isSameRefr)
							{
								setAsChosen = true;
								DBG
								(
									"TrainingMenu: {} is receiving training from {}.",
									p->coopActor->GetName(), 
									assocRefrPtr->GetName()
								);
							}

							break;
						}
						case Hash
						(
							RE::TweenMenu::MENU_NAME.data(), RE::TweenMenu::MENU_NAME.size()
						):
						{
							// Wants to open the TweenMenu.
							if (isRequestedMenu || 
								currentReq.fromAction == InputAction::kTweenMenu)
							{
								setAsChosen = true;
								DBG
								(
									"TweenMenu: {} is in control of menu.", 
									p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash
						(
							GlobalCoopData::CUSTOM_MENU.data(), GlobalCoopData::CUSTOM_MENU.size()
						):
						{
							// Could be any menu triggered by script using SKSE's CustomMenu.
							// Support all actions that trigger UIExtensions menus here.
							// For the most part, however, the mod's scripts directly set 
							// the requesting menu control PID, 
							// which bypasses queued request checks done here. 
							// Serves more as a failsafe.
							bool isDirectRequest = 
							(
								(isRequestedMenu) ||
								(
									currentReq.fromAction == InputAction::kCoopDebugMenu ||
									currentReq.fromAction == InputAction::kCoopIdlesMenu ||
									currentReq.fromAction == InputAction::kCoopMiniGamesMenu ||
									currentReq.fromAction == InputAction::kCoopSummoningMenu ||
									currentReq.fromAction == InputAction::kStatsMenu ||
									currentReq.fromAction == InputAction::kTeleportToPlayer || 
									currentReq.fromAction == InputAction::kTradeWithPlayer
								)
							);
							if (isDirectRequest)
							{
								setAsChosen = true;
								DBG
								(
									"{} (UIExtensions): {} is in control of menu.", 
									GlobalCoopData::CUSTOM_MENU, p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash
						(
							GlobalCoopData::LOOT_MENU.data(), GlobalCoopData::LOOT_MENU.size()
						):
						{
							// Crosshair pick data valid and request queued by crosshair movement.
							auto crosshairPickData = RE::CrosshairPickData::GetSingleton(); 
							if ((!isRequestedMenu || !crosshairPickData) || 
								(
									currentReq.fromAction != InputAction::kMoveCrosshair
								))
							{
								break;
							}
							
							auto assocRefrPtr = Util::GetRefrPtrFromHandle
							(
								currentReq.assocRefrHandle
							); 
							if (!assocRefrPtr || !assocRefrPtr->GetObjectReference())
							{
								break;
							}
							
							// Get the container to display with the LootMenu.
							auto reqContainerRefrPtr = Util::GetRefrPtrFromHandle
							(
								glob.reqQuickLootContainerHandle
							); 
							if (!reqContainerRefrPtr)
							{
								break;
							}

							// Compare associated refr/linked ash pile refr 
							// to crosshair-selected form stored in pad.
							auto ashPileRefPtr = Util::GetRefrPtrFromHandle
							(
								assocRefrPtr->extraList.GetAshPileRef()
							);
							auto baseObj = assocRefrPtr->GetObjectReference();
							isSameRefr = 
							{ 
								(assocRefrPtr == reqContainerRefrPtr) ||
								(ashPileRefPtr && ashPileRefPtr == reqContainerRefrPtr) ||
								(
									baseObj->Is
									(
										RE::FormType::Activator, RE::FormType::TalkingActivator
									) && 
									(
										Hash(assocRefrPtr->GetName()) == 
										Hash(reqContainerRefrPtr->GetName())
									)
								)
							};
							if (isSameRefr)
							{
								setAsChosen = true;
								DBG
								(
									"{} is in control of {}'s QuickLoot menu.",
									p->coopActor->GetName(), 
									assocRefrPtr->GetName()
								);

								// Store which player will receive control of the LootMenu.
								glob.quickLootControlPID = p->playerID;
							}

							break;
						}
						case Hash
						(
							GlobalCoopData::ENHANCED_HERO_MENU.data(), 
							GlobalCoopData::ENHANCED_HERO_MENU.size()
						):
						{
							// Request to open the enhanced Hero Menu.
							if (isRequestedMenu || 
								currentReq.fromAction == InputAction::kStatsMenu)
							{
								setAsChosen = true;
								DBG
								(
									"{}: {} is in control of menu.", 
									GlobalCoopData::ENHANCED_HERO_MENU, p->coopActor->GetName()
								);
							}

							break;
						}
						default:
						{
							DBG
							(
								"FALLTHROUGH for {}.", a_menuName
							);
							break;
						}
						}

						if (setAsChosen)
						{
							secsSinceChosenReq = secsSinceReq;
							resolvedPID = p->playerID;
							chosenReqType = kOldest;
						}
					} 
					
					// If this player's request was not chosen, 
					// for menus that have no clearly defined trigger, 
					// check which player submitted the most recent request 
					// and give that player control.
					// Currently only for MessageBoxMenus and CustomMenus, 
					// if triggered by activation.
					// Once a request is chosen and it is direct, 
					// do not continue looking for the newest request for this player.
					bool checkForNewestReq = 
					(
						(
							chosenReqType == kNone || 
							chosenReqType == kNewestDirect || 
							chosenReqType == kNewestIndirect
						) &&
						(
							(isMessageBoxMenu) || 
							(isCustomMenu && currentReq.fromAction == InputAction::kActivate)
						) && 
						(
							(!secsSinceChosenReq.has_value()) || 
							(secsSinceReq < secsSinceChosenReq.value())
						)
					);
					if (checkForNewestReq)
					{
						DBG
						(
							"{}: Check for newest req for {}.", a_menuName, p->coopActor->GetName()
						);
						// Direct requests can considered up to 5 seconds after enqueueing.
						bool directlyRequested = isRequestedMenu && secsSinceReq < 5.0f;
						// Message box menus can be triggered by a variety of things,
						// so if no direct request was made, 
						// choose the player that most recently activated an object.
						// Shorter maximum request lifetime of 2 seconds here, 
						// since more often than not, P1 should gain control of the menu.
						bool throughActivation = 
						(
							currentReq.fromAction == InputAction::kActivate && 
							secsSinceReq < 2.0f
						);
						if (directlyRequested || throughActivation) 
						{
							// Update seconds since most recent request.
							secsSinceChosenReq = secsSinceReq;
							resolvedPID = p->playerID;
							chosenReqType = directlyRequested ? kNewestDirect : kNewestIndirect;
							DBG
							(
								"{}: {} is in control of menu by {}.",
								a_menuName,
								p->coopActor->GetName(), 
								directlyRequested ? "direct request" : "indirect activation"
							);

							// NOTE:
							// Do not remove chosen newest requests,
							// since multiple queued message box menus tend to 
							// open in quick succession, and we want the same player 
							// to retain control over all menus queued to open.
							// The request will clear out when its lifetime expires
							// (2 or 5 seconds).

							// Move on to the next request.
							reqQueue.pop_back();
							continue;
						}
					}
					
					// Remove since this request was handled, fulfilled or not.
					if (a_modifyReqQueue) 
					{
						DBG
						(
							"{}: Removed handled request (menu: {}, action: {}, refr: {}) for {}.",
							a_menuName,
							currentReq.reqMenuName,
							currentReq.fromAction,
							Util::HandleIsValid(currentReq.assocRefrHandle) ?
							currentReq.assocRefrHandle.get()->GetName() :
							"NONE",
							p->coopActor->GetName()
						);
						menuOpeningActionRequests[pid].pop_front();
					}

					// Move on to the next request.
					reqQueue.pop_back();
				}

				DBG
				(
					"Active request queue size is now {} for {} after processing.", 
					menuOpeningActionRequests[pid].size(),
					p->coopActor->GetName()
				);
			}
		}

		DBG("Resolved PID from requests: {}", resolvedPID);
		// If there are no valid requests to open/close the current supported menu,
		// give control to the last player who controlled open menus.
		// Or if P1's managers are not active, such as when the co-op camera is disabled,
		// give P1 control of menus.
		if (resolvedPID == -1 && GlobalCoopData::SUPPORTED_MENU_NAMES.contains(a_menuName)) 
		{
			// Always give P1 control of the RaceSex/Console Menus when opened in co-op, 
			// since companion players should not customize P1 
			// and cannot control the keyboard anyways.
			bool givePreviousPlayerControl = 
			(
				(
					a_menuName != RE::Console::MENU_NAME && 
					a_menuName != RE::RaceSexMenu::MENU_NAME
				) && 
				(
					(glob.coopPlayers[0]->IsRunning()) ||
					(glob.supportedMenuOpen && glob.mim->IsRunning())
				)
			);
			if (givePreviousPlayerControl) 
			{
				DBG
				(
					"No valid requests to open supported menu {}, set to last menu PID: {}. "
					"Supported menus open: {}, data copied over: 0x{:X}.", 
					a_menuName,
					glob.prevMenuPID,
					glob.supportedMenuOpen.load(),
					*glob.copiedPlayerDataTypes
				);
				resolvedPID = glob.prevMenuPID;
			}
			else
			{
				DBG
				(
					"No valid requests to open supported menu {} "
					"and P1's managers are inactive or the Console Menu is opening/closing, "
					"set to P1 PID. " 
					"Supported menus open: {}, data copied over: 0x{:X}.", 
					a_menuName,
					glob.supportedMenuOpen.load(),
					*glob.copiedPlayerDataTypes

				);
				resolvedPID = 0;
			}
		}

		DBG("Final resolved PID: {}, for menu {}.", resolvedPID, a_menuName);

		return resolvedPID;
	}
}

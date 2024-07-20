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
		: Manager(ManagerType::kMIM), 
		gamepadDevice(RE::INPUT_DEVICE::kGamepad),
		keyboardDevice(RE::INPUT_DEVICE::kKeyboard),
		mouseDevice(RE::INPUT_DEVICE::kMouse),
		controllerDevice(RE::INPUT_DEVICE::kGamepad),
		bookContext(RE::UserEvents::INPUT_CONTEXT_ID::kBook),
		consoleContext(RE::UserEvents::INPUT_CONTEXT_ID::kConsole),
		cursorContext(RE::UserEvents::INPUT_CONTEXT_ID::kCursor),
		debugOverlayContext(RE::UserEvents::INPUT_CONTEXT_ID::kDebugOverlay),
		favoritesContext(RE::UserEvents::INPUT_CONTEXT_ID::kFavorites),
		gameplayContext(RE::UserEvents::INPUT_CONTEXT_ID::kGameplay),
		itemMenuContext(RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu),
		inventoryContext(RE::UserEvents::INPUT_CONTEXT_ID::kInventory),
		journalContext(RE::UserEvents::INPUT_CONTEXT_ID::kJournal),
		lockpickingContext(RE::UserEvents::INPUT_CONTEXT_ID::kLockpicking),
		mapContext(RE::UserEvents::INPUT_CONTEXT_ID::kMap),
		menuContext(RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode),
		statsContext(RE::UserEvents::INPUT_CONTEXT_ID::kStats)
	{
		// Menu-controlling player IDs.
		managerMenuCID = -1;
		managerMenuPlayerID = 0;
		// Default to P1's PID.
		pmcPlayerID = 0;
		// Handles for open container, source container, and co-op player actors.
		fromContainerHandle = RE::ObjectRefHandle();
		menuContainerHandle = RE::ObjectRefHandle();
		menuCoopActorHandle = RE::ActorHandle();
		gifteePlayerHandle = RE::ActorHandle();
		// Form selected in menu.
		selectedForm = nullptr;
		// Equip index. Defaults to right hand.
		reqEquipIndex = EquipIndex::kRightHand;

		// Event type to handle next.
		currentMenuInputType = MenuInputEventType::kEmulateInput;
		// Opened menu info.
		openedMenuType = SupportedMenu::kDefault;
		menuName = "";

		// Ints
		managedCoopMenusCount = 0;
		// Bools
		isCoopInventory = false;
		placeholderMagicChanged = false;
		shouldFavorite = false;
		shouldRefreshMenu = false;
		spellFavoriteStatusChanged = false;
		takeAll = false;
		// Player menu control outline overlay alpha value.
		pmcFadeInterpData = std::make_unique<TwoWayInterpData>();
		pmcFadeInterpData->SetInterpInterval(1.0f, true);
		pmcFadeInterpData->SetInterpInterval(1.0f, false);
		// Clear maps, sets, vectors.
		favMenuIndexToEntryMap.clear();
		favEntryEquipStates.clear();
		magEntryEquipStates.clear();
		magFormsList.clear();
		magFormEquippedIndices.clear();
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
		inventoryMenu = nullptr;
		journalMenu = nullptr;
		lockpickingMenu = nullptr;
		magicMenu = nullptr;
		mapMenu = nullptr;
		logger::debug("[MIM] MIM created.");
	}
	
#pragma region MANAGER_FUNCS_IMPL
	void MenuInputManager::MainTask()
	{
		if (newMenuAtopStack)
		{
			// Set menu control map and refresh data when a new menu is opened.
			SetMenuControlMap();
			RefreshData();

			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[MIM] MainTask: Try to lock: 0x{:X}.", hash);
			{
				std::unique_lock<std::mutex> lock(openedMenuMutex, std::try_to_lock);
				if (lock)
				{
					// REMOVE
					logger::debug("[MIM] MainTask: Lock acquired and data updated. Setting new menu opened flag to false.");
					newMenuAtopStack = false;
				}
				else
				{
					// REMOVE
					logger::debug("[MIM] MainTask: Could not acquire lock after updating data. Better luck next time.");
				}
			}
		}

		// Update controller state and event type to handle, if any.
		CheckControllerInput();

		// Update equip state when signalled to externally or when there is a delayed request.
		bool delayedReq = (delayedEquipStateRefresh && Util::GetElapsedSeconds(lastEquipStateRefreshReqTP) > 1.0f);
		if (equipEventRefreshReq || delayedReq) 
		{
			if (delayedReq) 
			{
				const auto& p = glob.coopPlayers[managerMenuCID];
				p->em->RefreshEquipState(RefreshSlots::kAll);
			}

			if (openedMenuType == SupportedMenu::kMagic)
			{
				RefreshMagicMenuEquipState(true);
			}
			else if(openedMenuType == SupportedMenu::kFavorites)
			{
				RefreshFavoritesMenuEquipState(true);
			}

			if (equipEventRefreshReq) 
			{
				// REMOVE
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				logger::debug("[MIM] MainTask: Refresh equip state lock. Try to lock: 0x{:X}.", hash);
				{
					std::unique_lock<std::mutex> lock(equipEventMutex, std::try_to_lock);
					if (lock)
					{
						// REMOVE
						logger::debug("[MIM] MainTask: Lock acquired and data updated. Resetting refresh equip state flags from {}, {} to false.", equipEventRefreshReq, delayedEquipStateRefresh);

						// Equip state refresh request fired before delayed equip refresh request, so we can clear the delayed one.
						equipEventRefreshReq = false;
						shouldRefreshMenu = true;
					}
					else
					{
						// REMOVE
						logger::debug("[MIM] MainTask: Could not acquire lock after updating data. Better luck next time.");
					}
				}

			}
			else
			{
				// Delayed refresh, no equip event fired in time.
				shouldRefreshMenu = true;
			}

			// Menu has been signalled to refresh, clear delayed req flag.
			delayedEquipStateRefresh = false;
		}

		// Refresh menu after sending event.
		if (shouldRefreshMenu)
		{
			RefreshMenu();
			// Clear flag after refresh.
			shouldRefreshMenu = false;
		}
	}

	void MenuInputManager::PrePauseTask()
	{
		// Release all inputs if menu closed when input(s) were being held.
		logger::debug("[MIM] PrePauseTask.");
		for (auto& [xMask, info] : menuControlMap)
		{
			if (info.eventType == MenuInputEventType::kEmulateInput)
			{
				if (xMask != XMASK_LS && xMask != XMASK_RS)
				{
					// Release button.
					std::unique_ptr<RE::InputEvent* const> buttonEvent = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(info.device, info.eventName, info.idCode, 0.0f, info.heldTimeSecs));
					// Set pad to indicate that the event should be ignored.
					(*buttonEvent.get())->AsIDEvent()->pad24 = 0xDEAD;
					Util::SendInputEvent(buttonEvent);
				}
				else
				{
					// Release LS and RS.
					auto thumbstickEvent = std::make_unique<RE::InputEvent* const>(Util::CreateThumbstickEvent(info.eventName, 0.0f, 0.0f, xMask == XMASK_LS));
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

		// Reset menu control CID before pausing.
		managerMenuCID = -1;
		managerMenuPlayerID = 0;
	}

	void MenuInputManager::PreStartTask()
	{
		return;
	}

	void MenuInputManager::RefreshData()
	{
		// Refresh all menu-related data.

		if (managerMenuCID != -1)
		{
			logger::debug("[MIM] RefreshData: Menu controller ID: {}", managerMenuCID);
			if (glob.coopSessionActive)
			{
				menuCoopActorHandle = glob.coopPlayers[managerMenuCID]->coopActor->GetHandle();
			}

			// Reset general menu data.
			currentMenuInputType = MenuInputEventType::kReleasedNoEvent;
			reqEquipIndex = EquipIndex::kRightHand;
			delayedEquipStateRefresh = false;
			equipEventRefreshReq = false;
			isCoopInventory = false;
			placeholderMagicChanged = false;
			shouldFavorite = false;
			shouldRefreshMenu = false;
			spellFavoriteStatusChanged = false;
			takeAll = false;
			fromContainerHandle = RE::ObjectRefHandle();
			menuContainerHandle = RE::ObjectRefHandle();
			selectedForm = nullptr;
			lastEquipStateRefreshReqTP = SteadyClock::now();

			// Clear any lingering queued input events.
			for (auto& ptr : queuedInputEvents)
			{
				ptr.release();
			}
			queuedInputEvents.clear();


			// Initialize menu-specific data.
			if (containerMenu)
			{
				// Get the container reference and check if it is the co-op player.
				// If so, the co-op player is accessing their inventory, and the handling of
				// certain button presses changes:
				// e.g. the "X" button does not take all but instead drops the selected item.
				// Binds remain the same either way.
				RE::NiPointer<RE::TESObjectREFR> containerRefr;
				bool succ = RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle(), containerRefr);
				if (!succ)
				{
					// NOTE: Keeping this commented out for now.
					// A bug used to arise where the target ref handle returned
					// would be invalid once a co-op companion player looted the container,
					// and attempted to access the container a second time.
					// Increment the GPtr ref to
					// update the container's target ref handle.
					/*containerMenu->AddRef();
					succ = RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle(), containerRefr);
					logger::debug("[MIM] RefreshData: Could not get container ref. Incrementing GRef. Succ after: {}. New handle and container: {}, {}.",
						succ, RE::ContainerMenu::GetTargetRefHandle(), containerRefr && containerRefr.get() ? containerRefr.get()->GetName() : "NONE");*/
				}

				menuContainerHandle = containerRefr && containerRefr.get() ? containerRefr->GetHandle() : RE::ObjectRefHandle();
				// If the container is the co-op companion player themselves, they are accessing their inventory.
				isCoopInventory = { 
					containerMenu->GetContainerMode() == RE::ContainerMenu::ContainerMode::kNPCMode && 
					Util::HandleIsValid(menuContainerHandle) && menuContainerHandle == menuCoopActorHandle
				};

				logger::debug("[MIM] RefreshData: Container {} mode: {}", 
					(menuContainerHandle && menuContainerHandle.get()) ? menuContainerHandle.get()->GetName() : "NONE",
					static_cast<uint32_t>(containerMenu->GetContainerMode()));
			}
			else if (magicMenu)
			{
				// Set the initial magic item equip states.
				InitMagicMenuEquippedStates();
				// Refresh menu after intializing equip states/favorited indices.
				RefreshMenu();
			}
			else if (favoritesMenu)
			{
				// Don't allow saving when the co-op player opens the favorites menu, as this will
				// overwrite player 1's favorites.
				favoritesMenu->menuFlags.reset(RE::UI_MENU_FLAGS::kAllowSaving);
				InitFavoritesEntries();
				// Refresh menu after intializing equip states/favorited indices.
				RefreshMenu();
			}

			// Set controlmap initial input states so that
			// inputs already held as the menu opens do not trigger
			// any input events until they are released and pressed again.
			XINPUT_STATE buttonState;
			ZeroMemory(&buttonState, sizeof(buttonState));
			// First, check for input presses as given by the menu control map above
			if (XInputGetState(managerMenuCID, &buttonState) == ERROR_SUCCESS)
			{
				for (auto iter = menuControlMap.begin(); iter != menuControlMap.end(); ++iter)
				{
					if (buttonState.Gamepad.wButtons & iter->first)
					{
						// Set to emulated input initially to prevent previous event type from triggering.
						menuControlMap[iter->first].eventType = MenuInputEventType::kEmulateInput;
					}
				}
			}
		}
		else
		{
			logger::critical("[MIM] ERR: RefreshData: Got invalid controller ID (-1).");
		}
	}

	const ManagerState MenuInputManager::ShouldSelfPause()
	{
		// Pause self if the menu controller's state is inaccessible.
		XINPUT_STATE buttonState;
		ZeroMemory(&buttonState, sizeof(buttonState));
		if (managerMenuCID == -1 || XInputGetState(managerMenuCID, &buttonState) != ERROR_SUCCESS)
		{
			// Leave error message before returning.
			if (managerMenuCID != -1) 
			{
				logger::error("[MIM] ERR: ShouldSelfPause: Could not get XINPUT state for controller ID {}. Pausing menu input manager.", managerMenuCID);
			}

			return ManagerState::kPaused;
		}

		// Switch to P1 control if the current active container menu tab is P1's inventory.
		if (auto taskInterface = SKSE::GetTaskInterface(); taskInterface)
		{
			if (auto ui = RE::UI::GetSingleton(); ui)
			{
				if (auto containerMenu = ui->GetMenu<RE::ContainerMenu>(); containerMenu && containerMenu.get())
				{
					if (auto view = containerMenu->uiMovie; view)
					{
						RE::GFxValue result;
						view->Invoke("_root.Menu_mc.isViewingContainer", std::addressof(result), nullptr, 0);
						// Viewing P1's inventory from container.
						if (bool isViewingContainer = result.GetBool(); !isViewingContainer) 
						{
							RE::NiPointer<RE::TESObjectREFR> containerRefr;
							RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle(), containerRefr);
							// The container is a co-op companion's inventory.
							if (GlobalCoopData::IsCoopPlayer(containerRefr)) 
							{
								logger::debug("[MIM] ShouldSelfPause: Set menu CID to P1's: {}.", glob.player1CID);
								GlobalCoopData::SetMenuCIDs(glob.player1CID);
								return ManagerState::kPaused;
							}
						}
					}
				}
			}
		}

		return currentState;
	}

	const ManagerState MenuInputManager::ShouldSelfResume()
	{
		// Switch back to co-op companion control if the current active container menu tab is their inventory.
		if (auto taskInterface = SKSE::GetTaskInterface(); taskInterface)
		{
			if (auto ui = RE::UI::GetSingleton(); ui)
			{
				if (auto containerMenu = ui->GetMenu<RE::ContainerMenu>(); containerMenu && containerMenu.get())
				{
					if (auto view = containerMenu->uiMovie; view)
					{
						RE::GFxValue result;
						view->Invoke("_root.Menu_mc.isViewingContainer", std::addressof(result), nullptr, 0);
						// Is viewing the container and not P1's inventory.
						if (bool isViewingContainer = result.GetBool(); isViewingContainer)
						{
							RE::NiPointer<RE::TESObjectREFR> containerRefr;
							RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle(), containerRefr);
							// The container is a co-op companion's inventory.
							if (auto pIndex = GlobalCoopData::GetCoopPlayerIndex(containerRefr); pIndex != -1) 
							{
								managerMenuCID = pIndex;
								managerMenuPlayerID = glob.coopPlayers[managerMenuCID]->playerID;
								logger::debug("[MIM] ShouldSelfResume: Set menu CID to saved co-op player's: {}.", managerMenuCID);
								GlobalCoopData::SetMenuCIDs(managerMenuCID);

								return ManagerState::kRunning;
							}
						}
					}
				}
			}
		}

		return currentState;
	}

#pragma endregion

	void MenuInputManager::CheckControllerInput()
	{
		// Update controller input state and set menu event type to handle.

		auto& paInfo = glob.paInfoHolder;
		XINPUT_STATE buttonState;
		ZeroMemory(&buttonState, sizeof(buttonState));
		if (managerMenuCID < 0 || managerMenuCID >= ALYSLC_MAX_PLAYER_COUNT) 
		{
			logger::error("[MIM] ERR: CheckControllerInput: manager menu controller ID is invalid: ({}).", managerMenuCID);
			return;
		}

		if (XInputGetState(managerMenuCID, &buttonState) == ERROR_SUCCESS)
		{
			// Hardcoded deadzone equal to half of the trigger's depressible range.
			const BYTE triggerDeadzone = UCHAR_MAX / 2;
			// Check for button state changes.
			for (auto& [xMask, bindInfo] : menuControlMap) 
			{
				// Three separate input types (triggers, buttons, analog sticks).
				bool handleTriggerPress = (xMask == XMASK_LT && buttonState.Gamepad.bLeftTrigger > triggerDeadzone) ||
										  (xMask == XMASK_RT && buttonState.Gamepad.bRightTrigger > triggerDeadzone);
				bool handleButtonPress = !handleTriggerPress && (xMask & buttonState.Gamepad.wButtons);
				bool handleAnalogStickMovement = false;
				bool shouldCheckAnalogStick = (xMask == XMASK_LS || xMask == XMASK_RS);
				if (shouldCheckAnalogStick)
				{
					handleAnalogStickMovement = glob.cdh->GetAnalogStickState(managerMenuCID, xMask == XMASK_LS).normMag > 0.0f;
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
					
					// Update menu event to send if just pressed.
					if (justPressed) 
					{
						// Default to no event.
						currentMenuInputType = MenuInputEventType::kReleasedNoEvent;
						if (handleButtonPress)
						{
							logger::debug("[MIM] CheckControllerInput: Handle button (0x{:X}) press with opened menu type {}. Processing.", xMask, openedMenuType);
							switch (openedMenuType)
							{
							case SupportedMenu::kBarter:
							{
								ProcessBarterMenuButtonInput(xMask);
								break;
							}
							case SupportedMenu::kBook:
							{
								ProcessBookMenuButtonInput(xMask);
								break;
							}
							case SupportedMenu::kContainer:
							{
								ProcessContainerMenuButtonInput(xMask);
								break;
							}
							case SupportedMenu::kDialogue:
							{
								ProcessDialogueMenuButtonInput(xMask);
								break;
							}
							case SupportedMenu::kInventory:
							{
								ProcessInventoryMenuButtonInput(xMask);
								break;
							}
							case SupportedMenu::kMagic:
							{
								ProcessMagicMenuButtonInput(xMask);
								break;
							}
							case SupportedMenu::kMap:
							{
								ProcessMapMenuButtonInput(xMask);
								break;
							}
							case SupportedMenu::kFavorites:
							{
								ProcessFavoritesMenuButtonInput(xMask);
								break;
							}
							case SupportedMenu::kLoot:
							{
								ProcessLootMenuButtonInput(xMask);
								break;
							}
							default:
							{
								break;
							}
							}
						}
						else if (handleTriggerPress)
						{
							// Trigger presses handled here for all menus.
							ProcessTriggerInput(xMask == XMASK_LT);
						}

						// Event type to send was not changed while processing above, so emulate input.
						if (currentMenuInputType == MenuInputEventType::kReleasedNoEvent) 
						{
							currentMenuInputType = MenuInputEventType::kEmulateInput;
						}
					}
					else if (bindInfo.eventType == MenuInputEventType::kEmulateInput)
					{
						// Continue to send input events while the button/trigger/analog is held.
						currentMenuInputType = MenuInputEventType::kEmulateInput;
					}
							
					// If unable to set bind info, set event type to none.
					if (!SetEmulatedInputEventInfo(xMask, bindInfo))
					{
						currentMenuInputType = MenuInputEventType::kReleasedNoEvent;
					}
					else if (currentMenuInputType == MenuInputEventType::kEmulateInput)
					{
						if (handleAnalogStickMovement) 
						{
							const auto& stickData = glob.cdh->GetAnalogStickState(managerMenuCID, xMask == XMASK_LS);
							const auto& xComp = stickData.xComp;
							const auto& yComp = stickData.yComp;
							const auto& stickMag = stickData.normMag;
							// Enqueue thumbstick event.
							// Set pad to indicate that the co-op player sent the input, not P1.
							auto thumbstickEvent = std::make_unique<RE::InputEvent* const>(Util::CreateThumbstickEvent(bindInfo.eventName, xComp * stickMag, yComp * stickMag, xMask == XMASK_LS));
							(*thumbstickEvent)->AsIDEvent()->pad24 = 0xCA11;
							queuedInputEvents.emplace_back(std::move(thumbstickEvent));
						}
						else
						{
							// Enqueue button input event.
							// Set pad to indicate that the co-op player sent the input, not P1.
							auto buttonEvent = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(bindInfo.device, bindInfo.eventName, bindInfo.idCode, bindInfo.value, bindInfo.heldTimeSecs));
							(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
							queuedInputEvents.emplace_back(std::move(buttonEvent));
						}
					}

					// Update linked event type for this binding.
					// Stored for handling later on release.
					menuControlMap[xMask].eventType = currentMenuInputType;
					currentBindInfo = menuControlMap[xMask];
					// Handle the resolved event type.
					HandleMenuEvent();
				}
				else
				{
					// Analog stick centered or button/trigger released.
					// 
					// Update press value on release.
					if (bindInfo.value == 1.0f)
					{
						bindInfo.value = 0.0f;
					}

					// Only send release event if an event was handled before.
					if (bindInfo.eventType != MenuInputEventType::kReleasedNoEvent)
					{
						// If unable to set bind info, set event type to none.
						if (!SetEmulatedInputEventInfo(xMask, bindInfo))
						{
							currentMenuInputType = MenuInputEventType::kReleasedNoEvent;
						}
						else
						{
							if (bindInfo.eventType == MenuInputEventType::kEmulateInput)
							{
								// Send button released/analog stick centered event.
								currentMenuInputType = MenuInputEventType::kEmulateInput;
								if (shouldCheckAnalogStick)
								{
									// Enqueue thumbstick event.
									// Set pad to indicate that the co-op player sent the input, not P1.
									auto thumbstickEvent = std::make_unique<RE::InputEvent* const>(Util::CreateThumbstickEvent(bindInfo.eventName, 0.0f, 0.0f, xMask == XMASK_LS));
									(*thumbstickEvent)->AsIDEvent()->pad24 = 0xCA11;
									queuedInputEvents.emplace_back(std::move(thumbstickEvent));
								}
								else
								{
									// Enqueue button input event.
									// Set pad to indicate that the co-op player sent the input, not P1.
									auto buttonEvent = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(bindInfo.device, bindInfo.eventName, bindInfo.idCode, bindInfo.value, bindInfo.heldTimeSecs));
									(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
									queuedInputEvents.emplace_back(std::move(buttonEvent));
								}
							}
							else
							{
								// No P1 emulation event on release is tied to this bind.
								// Nothing to send.
								currentMenuInputType = MenuInputEventType::kReleasedNoEvent;
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
	}

	void MenuInputManager::DebugPrintMenuBinds()
	{
		// Print out all binds (event name -> input ID code) per context and for each device.

		logger::debug("===============================================================================================================================");
		logger::debug("++++++++++++++++++++++++++++++++BOOK++++++++++++++++++++++++++++++++++");
		logger::debug("-------------------------------Gamepad--------------------------------");
		for (auto& binds : controlMap->controlMap[bookContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Keyboard-------------------------------");
		for (auto& binds : controlMap->controlMap[bookContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Mouse----------------------------------");
		for (auto& binds : controlMap->controlMap[bookContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++CONSOLE++++++++++++++++++++++++++++++++++");
		logger::debug("-------------------------------Gamepad----------------------------------");
		for (auto& binds : controlMap->controlMap[consoleContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Keyboard---------------------------------");
		for (auto& binds : controlMap->controlMap[consoleContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Mouse------------------------------------");
		for (auto& binds : controlMap->controlMap[consoleContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("++++++++++++++++++++++++++++++++CURSOR++++++++++++++++++++++++++++++++++");
		logger::debug("--------------------------------Gamepad---------------------------------");
		for (auto& binds : controlMap->controlMap[cursorContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("--------------------------------Keyboard--------------------------------");
		for (auto& binds : controlMap->controlMap[cursorContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("--------------------------------Mouse-----------------------------------");
		for (auto& binds : controlMap->controlMap[cursorContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++DEBUGOVERLAY++++++++++++++++++++++++++++++++++");
		logger::debug("---------------------------------Gamepad-------------------------------------");
		for (auto& binds : controlMap->controlMap[debugOverlayContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("---------------------------------Keyboard------------------------------------");
		for (auto& binds : controlMap->controlMap[debugOverlayContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("---------------------------------Mouse---------------------------------------");
		for (auto& binds : controlMap->controlMap[debugOverlayContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++FAVORITES++++++++++++++++++++++++++++++++++");
		logger::debug("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : controlMap->controlMap[favoritesContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("--------------------------------Keyboard----------------------------------");
		for (auto& binds : controlMap->controlMap[favoritesContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("--------------------------------Mouse-------------------------------------");
		for (auto& binds : controlMap->controlMap[favoritesContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++GAMEPLAY++++++++++++++++++++++++++++++++++");
		logger::debug("-------------------------------Gamepad-----------------------------------");
		for (auto& binds : controlMap->controlMap[gameplayContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Keyboard----------------------------------");
		for (auto& binds : controlMap->controlMap[gameplayContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Mouse-------------------------------------");
		for (auto& binds : controlMap->controlMap[gameplayContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++INVENTORY++++++++++++++++++++++++++++++++++");
		logger::debug("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : controlMap->controlMap[inventoryContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("--------------------------------Keyboard----------------------------------");
		for (auto& binds : controlMap->controlMap[inventoryContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("--------------------------------Mouse-------------------------------------");
		for (auto& binds : controlMap->controlMap[inventoryContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++ITEMMENU++++++++++++++++++++++++++++++++++");
		logger::debug("-------------------------------Gamepad-----------------------------------");
		for (auto& binds : controlMap->controlMap[itemMenuContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Keyboard----------------------------------");
		for (auto& binds : controlMap->controlMap[itemMenuContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Mouse-------------------------------------");
		for (auto& binds : controlMap->controlMap[itemMenuContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++JOURNAL++++++++++++++++++++++++++++++++++");
		logger::debug("-------------------------------Gamepad----------------------------------");
		for (auto& binds : controlMap->controlMap[journalContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Keyboard---------------------------------");
		for (auto& binds : controlMap->controlMap[journalContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Mouse----------------------------------");
		for (auto& binds : controlMap->controlMap[journalContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++LOCKPICKING++++++++++++++++++++++++++++++++++");
		logger::debug("---------------------------------Gamepad------------------------------------");
		for (auto& binds : controlMap->controlMap[lockpickingContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("---------------------------------Keyboard-----------------------------------");
		for (auto& binds : controlMap->controlMap[lockpickingContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("---------------------------------Mouse-------------------------------------");
		for (auto& binds : controlMap->controlMap[lockpickingContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++MAP++++++++++++++++++++++++++++++++++");
		logger::debug("-----------------------------Gamepad--------------------------------");
		for (auto& binds : controlMap->controlMap[mapContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-----------------------------Keyboard-------------------------------");
		for (auto& binds : controlMap->controlMap[mapContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-----------------------------Mouse----------------------------------");
		for (auto& binds : controlMap->controlMap[mapContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++MENUMODE++++++++++++++++++++++++++++++++++");
		logger::debug("-------------------------------Gamepad------------------------------------");
		for (auto& binds : controlMap->controlMap[menuContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Keyboard----------------------------------");
		for (auto& binds : controlMap->controlMap[menuContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("-------------------------------Mouse-------------------------------------");
		for (auto& binds : controlMap->controlMap[menuContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		logger::debug("===============================================================================================================================");
		logger::debug("+++++++++++++++++++++++++++++++STATS++++++++++++++++++++++++++++++++++");
		logger::debug("------------------------------Gamepad---------------------------------");
		for (auto& binds : controlMap->controlMap[statsContext]->deviceMappings[gamepadDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("------------------------------Keyboard--------------------------------");
		for (auto& binds : controlMap->controlMap[statsContext]->deviceMappings[keyboardDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		logger::debug("------------------------------Mouse-----------------------------------");
		for (auto& binds : controlMap->controlMap[statsContext]->deviceMappings[mouseDevice])
		{
			logger::debug("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
	}

	void MenuInputManager::DrawPlayerMenuControlOverlay()
	{
		// Draw screen border to indicate which player is controlling menus.
		// Border is drawn while co-op is active or when in the Summoning Menu,
		// and has the same color as the menu-controlling player's crosshair.

		bool tempMenuOpen = !Util::MenusOnlyAlwaysOpenInStack();
		// Update interpolated value and direction change flag + interpolation direction.
		// Use to set the overlay alpha value.
		const float interpValue = pmcFadeInterpData->UpdateInterpolatedValue(tempMenuOpen);
		if ((glob.globalDataInit) && ((glob.coopSessionActive) || ((!menuNamesHashSet.empty() && menuNamesHashSet.contains(Hash(GlobalCoopData::SETUP_MENU_NAME))) || pmcFadeInterpData->interpToMin)))
		{
			/*logger::info("[MIM] DrawPlayerMenuControlOverlay: temp menu open: {}, interp to max, min: {}, {}, current value: {}, direction change: {}, value at direction change: {}. Menu CIDs: current {}, prev: {}. Last set PMC PID: {}.",
				tempMenuOpen, 
				pmcFadeInterpData->interpToMax, pmcFadeInterpData->interpToMin, 
				pmcFadeInterpData->value, pmcFadeInterpData->directionChangeFlag,
				pmcFadeInterpData->valueAtDirectionChange,
				glob.menuCID, glob.prevMenuCID, pmcPlayerID);*/

			// Draw when a temporary menu is open or while still fading in/out.
			if (tempMenuOpen || pmcFadeInterpData->interpToMax || pmcFadeInterpData->interpToMin) 
			{
				const uint8_t alpha = static_cast<uint8_t>(static_cast<float>(0x3F) * interpValue);
				if (glob.coopSessionActive)
				{
					// Co-op session active.
					// Set to the player ID of the player who last controlled opened menus,
					// or P1's PID (0) if there is no recorded previous menu CID.
					pmcPlayerID = glob.prevMenuCID != -1 ? glob.coopPlayers[glob.prevMenuCID]->playerID : 0;
				}
				else if (IsRunning()) 
				{
					// Co-op session not active and a player is in the co-op setup menu, 
					// so set to the player ID of the player requesting control of this menu.
					// Set to P1's PID (0) if there is no valid manager player PID.
					pmcPlayerID = managerMenuPlayerID != -1 ? managerMenuPlayerID : 0;
				}

				// Should never happen, but if not a valid player ID, return.
				if (pmcPlayerID == -1)
				{
					return;
				}

				uint32_t uiRGBA = (Settings::vuOverlayRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha;
				const auto& thickness = Settings::fPlayerMenuControlOverlayOutlineThickness;
				const auto halfThickness = 0.5f * thickness;
				const float rectWidth = DebugAPI::screenResX;
				const float rectHeight = DebugAPI::screenResY;

				// No overlap at corners by making sure only one of the edges is drawn all the way to the corner.
				// Left Edge.
				DebugAPI::QueueLine2D(glm::vec2(halfThickness, thickness), glm::vec2(halfThickness, rectHeight - thickness), uiRGBA, thickness);
				// Right Edge.
				DebugAPI::QueueLine2D(glm::vec2(rectWidth - halfThickness, thickness), glm::vec2(rectWidth - halfThickness, rectHeight - thickness), uiRGBA, thickness);
				// Top Edge.
				DebugAPI::QueueLine2D(glm::vec2(0.0f, halfThickness), glm::vec2(rectWidth, halfThickness), uiRGBA, thickness);
				// Bottom Edge.
				DebugAPI::QueueLine2D(glm::vec2(0.0f, rectHeight - halfThickness), glm::vec2(rectWidth, rectHeight - halfThickness), uiRGBA, thickness);
			}
		}
	}

	void MenuInputManager::EquipP1QSForm()
	{
		// Update equipped quick slot item/spell for P1
		// and update the Favorites Menu quick slot tag(s) as needed.
		// NOTE: Run from MenuControls hook.

		logger::debug("[MIM] EquipP1QSForm.");
		auto ui = RE::UI::GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto taskInterface = SKSE::GetTaskInterface();
		if (!ui || !p1 || !taskInterface)
		{
			logger::error("[MIM] ERR: EquipP1QSForm: UI invalid: {}, player 1 invalid: {}, task interface invalid: {}.", 
				(bool)!ui, (bool)!p1, (bool)!taskInterface);
			return;
		}

		// Global data not set or players not initialized, so return.
		if (!glob.globalDataInit || !glob.allPlayersInit)
		{
			return;
		}

		// Failsafe: Initialize index-to-entry map if it is empty. 
		if (favMenuIndexToEntryMap.empty()) 
		{
			logger::debug("[MIM] EquipP1QSForm: Favorites Menu index-to-entry map was empty. Initializing now.");
			InitP1QSFormEntries();
		}

		// P1 is controlling the menu.
		menuCoopActorHandle = p1->GetHandle();
		taskInterface->AddUITask(
			[this]() {
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					// Favorites menu must be open.
					if (auto favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); favoritesMenu && favoritesMenu.get())
					{
						if (auto view = favoritesMenu->uiMovie; view)
						{
							const auto& em = glob.coopPlayers[glob.player1CID]->em;
							// Index allows us to get the selected form.
							RE::GFxValue selectedIndex;
							view->GetVariable(std::addressof(selectedIndex), "_root.MenuHolder.Menu_mc.itemList.selectedEntry.index");
							const uint32_t index = static_cast<uint32_t>(selectedIndex.GetNumber());
							// Get mapped entry for the selected index.
							// Entry is used to update the item's text in the menu.
							if (favMenuIndexToEntryMap.contains(index))
							{
								const uint32_t selectedEntryNum = favMenuIndexToEntryMap.at(index);
								const auto form = favoritesMenu->favorites[index].item;
								if (form)
								{
									bool isConsumable = form->Is(RE::FormType::AlchemyItem, RE::FormType::Ingredient);
									bool isSpell = form->Is(RE::FormType::Spell);
									// Quick slot-supported item.
									if (isConsumable || isSpell)
									{
										bool equipped = false;
										// Unequipped if already equipped; otherwise, equip the new item/spell.
										if (isConsumable)
										{
											em->quickSlotItem = form == em->quickSlotItem ? nullptr : form;
											equipped = em->quickSlotItem;
										}
										else
										{
											em->quickSlotSpell = form == em->quickSlotSpell ? nullptr : form->As<RE::SpellItem>();
											equipped = em->quickSlotSpell;
										}

										RE::GFxValue entry;
										view->GetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", selectedEntryNum, std::addressof(entry), 1);
										RE::GFxValue entryText;
										entry.GetMember("text", std::addressof(entryText));
										std::string entryStr = entryText.GetString();
										// Update entry text with the quick slot tag.
										if (equipped)
										{
											entryStr = fmt::format("[*QS{}*] {}", isConsumable ? "I" : "S", entryStr);
											// Index corresponding to the previously equipped QS item/spell.
											uint32_t equippedQSIndex = isConsumable ? em->equippedQSItemIndex : em->equippedQSSpellIndex;
											// Remove item/spell tag from old quick slot item/spell entry if needed.
											if (equippedQSIndex != -1 && index != equippedQSIndex)
											{
												uint32_t oldEntryNum = favMenuIndexToEntryMap[equippedQSIndex];
												RE::GFxValue oldEntry;
												view->GetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", oldEntryNum, std::addressof(oldEntry), 1);
												RE::GFxValue oldEntryText;
												oldEntry.GetMember("text", std::addressof(oldEntryText));
												std::string oldEntryStr = oldEntryText.GetString();
												auto qsTagStartIndex = oldEntryStr.find("[*QS", 0);
												// Tag found and item name has a non-zero length.
												if (qsTagStartIndex != std::string::npos && qsTagStartIndex + qsPrefixTagLength <= oldEntryStr.length())
												{
													// Restore old entry text with tag removed.
													oldEntryStr = oldEntryStr.substr(qsTagStartIndex + qsPrefixTagLength);
													oldEntryText.SetString(oldEntryStr);
													oldEntry.SetMember("text", oldEntryText);
													view->SetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", oldEntryNum, std::addressof(oldEntry), 1);
												}
											}
										}
										else
										{
											// Remove QS tag from the current entry.
											auto qsTagStartIndex = entryStr.find("[*QS", 0);
											if (qsTagStartIndex != std::string::npos && qsTagStartIndex + qsPrefixTagLength <= entryStr.length())
											{
												entryStr = entryStr.substr(qsTagStartIndex + qsPrefixTagLength);
											}
										}

										entryText.SetString(entryStr);
										entry.SetMember("text", entryText);
										view->SetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", selectedEntryNum, std::addressof(entry), 1);

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
										glob.coopPlayers[glob.player1CID]->em->RefreshEquipState(RefreshSlots::kAll);
										// Update the list to reflect our changes.
										view->InvokeNoReturn("_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0);
									}
								}
							}
							else
							{
								logger::debug("[MIM] EquipP1QSForm: Player 1's favorites menu indices to entries map is empty: {}, does not contain the selected index: {}",
									favMenuIndexToEntryMap.empty(), !favMenuIndexToEntryMap.contains(index));
							}
						}
					}
				}
			});
	}

	RE::TESForm* MenuInputManager::GetSelectedMagicMenuSpell()
	{
		// Get the spell/shout that the player has selected in the Magic Menu.

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			logger::error("[MIM] ERR: GetSelectedMagicMenuSpell: Menu player's ptr is invalid.");
			return nullptr;
		}

		// Form for spell to equip.
		RE::TESForm* formToEquip = nullptr;
		// Not actually an ItemList, but the cast does preserve some of the structure
		// found in the ItemList class and all we need is some way of identifying the spell.
		auto magicItemList = reinterpret_cast<RE::ItemList*>(magicMenu->unk30);
		RE::ItemList::Item* selectedItem = GetSelectedItem(magicItemList);
		if (selectedItem)
		{
			// First, if SKYUI is installed (it should be), this will grab the selected form based on its form ID,
			// which is the most accurate way of getting the selected magic item.
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

			if (index < magicItemList->items.size())
			{
				RE::GFxValue entry;
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
					if (RE::TESForm* magicForm = Util::GetFormFromFID(formID); magicForm)
					{
						return magicForm;
					}
				}
			}

			// If the above method fails, we have to compare the selected item's name with known spells/shouts, 
			// which will sometimes fail to match with the correct spell/shout if the player knows multiple
			// spells/shouts with the same name.

			//=======================================================================================
			// First ensure both the player in the menu and P1 have the same known spells and shouts.
			//=======================================================================================

			// Ensure placeholder spells/shout are not added to P1.
			const auto& p = glob.coopPlayers[managerMenuCID];
			auto placeholderSpell2H = p->em->placeholderMagic[!PlaceholderMagicIndex::k2H];
			auto placeholderSpellLH = p->em->placeholderMagic[!PlaceholderMagicIndex::kLH];
			auto placeholderSpellRH = p->em->placeholderMagic[!PlaceholderMagicIndex::kRH];

			// Add spells that the co-op companion player learned.
			for (auto spellItem : menuCoopActorPtr->addedSpells)
			{
				if (!glob.player1Actor->HasSpell(spellItem) && !glob.placeholderSpellsSet.contains(spellItem))
				{
					glob.player1Actor->AddSpell(spellItem);
					break;
				}
			}

			auto spellList = menuCoopActorPtr->GetActorBase()->actorEffects->spells;
			if (spellList)
			{
				uint32_t spellListSize = menuCoopActorPtr->GetActorBase()->actorEffects->numSpells;
				// Add spells that the co-op companion player has by virtue of their actor base.
				for (uint32_t i = 0; i < spellListSize; ++i)
				{
					auto spellItem = spellList[i];
					if (!glob.player1Actor->HasSpell(spellItem) && !glob.placeholderSpellsSet.contains(spellItem))
					{
						glob.player1Actor->AddSpell(spellItem);
						break;
					}
				}
			}

			auto shoutList = menuCoopActorPtr->GetActorBase()->actorEffects->shouts;
			if (shoutList)
			{
				uint32_t shoutListSize = menuCoopActorPtr->GetActorBase()->actorEffects->numShouts;
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

			auto chosenMagicItemName = selectedItem->data.GetName();

			// Match spell name with one of player 1's learned spells.
			for (auto spellItem : glob.player1Actor->addedSpells)
			{
				if (strcmp(spellItem->GetName(), chosenMagicItemName) == 0)
				{
					formToEquip = spellItem;
					break;
				}
			}

			if (spellList = glob.player1Actor->GetActorBase()->actorEffects->spells; spellList)
			{
				uint32_t spellListSize = glob.player1Actor->GetActorBase()->actorEffects->numSpells;
				// Match spell name with one of player 1's actorbase spells.
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

			if (shoutList = glob.player1Actor->GetActorBase()->actorEffects->shouts; shoutList)
			{
				uint32_t shoutListSize = glob.player1Actor->GetActorBase()->actorEffects->numShouts;
				// Match with shouts that player 1 has by virtue of their actor base.
				for (uint32_t i = 0; i < shoutListSize; ++i)
				{
					// Some unused shouts exist.
					// All have one-character names.
					if (shoutList[i] && strlen(shoutList[i]->GetName()) > 1 && strcmp(shoutList[i]->GetName(), chosenMagicItemName) == 0)
					{
						formToEquip = shoutList[i];
					}
				}
			}

			// Failsafe: Ensure placeholder spell/shout is not selected.
			if (formToEquip &&
				(formToEquip == placeholderSpell2H ||
				formToEquip == placeholderSpellLH ||
				formToEquip == placeholderSpellRH)) 
			{
				formToEquip = nullptr;
			}
		}

		// REMOVE when done debugging.
		logger::debug("[MIM] GetSelectedMagicMenuSpell: Matched selected item with p1 magic form: {}.",
			(formToEquip) ? formToEquip->GetName() : "NONE");
		return formToEquip;
	}

	void MenuInputManager::HandleMenuEvent()
	{
		// Handle resolved menu event type.

		switch (currentMenuInputType)
		{
		case MenuInputEventType::kEquipReq:
		{
			// REMOVE when done debugging.
			logger::debug("[MIM] HandleMenuEvent: Equip Request Event: from container: {}, form: {}, equip index: {}, placeholder spell changed: {}.",
				(Util::HandleIsValid(fromContainerHandle)) ? fromContainerHandle.get()->GetName() : "NONE",
				(selectedForm) ? selectedForm->GetName() : "NONE",
				reqEquipIndex, placeholderMagicChanged);
			// Equip/unequip the selected form.
			glob.coopPlayers[managerMenuCID]->em->HandleMenuEquipRequest(fromContainerHandle, selectedForm, reqEquipIndex, placeholderMagicChanged);
			// Reset placeholder magic changed flag and equip index.
			placeholderMagicChanged = false;
			reqEquipIndex = EquipIndex::kRightHand;
			break;
		}
		case MenuInputEventType::kEmulateInput:
		{
			SendQueuedInputEvents();
			if (magicMenu)
			{
				if (spellFavoriteStatusChanged)
				{
					// Have to set cyclable favorited spells after a spell is favorited/unfavorited.
					RefreshCyclableSpells();
					// Equip "carets" get cleared, so we have to update the equip state (un)favoriting.
					RefreshMagicMenuEquipState(true);
					spellFavoriteStatusChanged = false;
				}
			}

			break;
		}
		case MenuInputEventType::kTakeItemReq:
		{
			// REMOVE when done debugging.
			logger::debug("[MIM] HandleMenuEvent: Take Item(s) Request Event: from container: {}, form: {}, take all: {}",
				(Util::HandleIsValid(fromContainerHandle)) ? fromContainerHandle.get()->GetName() : "NONE",
				(selectedForm) ? selectedForm->GetName() : "NONE",
				takeAll);
			HandleLootRequest();
			// Reset flags afterward.
			shouldRefreshMenu = true;
			takeAll = false;
			break;
		}
		default:
		{
			break;
		}
		}

		// Reset menu event type to none after handling the event.
		currentMenuInputType = MenuInputEventType::kReleasedNoEvent;
	}

	void MenuInputManager::HandleLootRequest()
	{
		// Loot all items or all of the selected item in the Container Menu.
		// Transfer to P1 and then to the requesting player.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1) 
		{
			logger::error("[MIM] ERR: HandleLootRequest: P1 is invalid.");
			return;
		}

		auto fromContainerPtr = Util::GetRefrPtrFromHandle(fromContainerHandle);
		// If the container to loot from is invalid or if the player has not selected
		// and item and is not trying to take all the items in the container, return here.
		if (!fromContainerPtr || (!selectedForm && !takeAll))
		{
			return;
		}

		if (takeAll) 
		{
			const auto inventory = fromContainerPtr->GetInventory();
			// Walk through inventory and remove all of each item.
			for (const auto& [boundObj, countEntryPair] : inventory) 
			{
				if (boundObj && countEntryPair.first > 0) 
				{
					// To P1 and then to the requesting actor through the subsequent container changed event.
					fromContainerPtr->RemoveItem(boundObj, countEntryPair.first, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, p1);
				}
			}
		}
		else if (auto boundObj = selectedForm->As<RE::TESBoundObject>(); boundObj)
		{
			// Loot a specific item.
			int32_t count = -1;
			const auto invCounts = fromContainerPtr->GetInventoryCounts();
			if (invCounts.contains(boundObj)) 
			{
				count = fromContainerPtr->GetInventoryCounts().at(boundObj);
			}

			// Not within inventory, so nothing to loot.
			if (count == -1) 
			{
				return;
			}

			// To P1 and then to the requesting actor through the subsequent container changed event.
			fromContainerPtr->RemoveItem(boundObj, count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, p1);
		}
	}

	void MenuInputManager::InitFavoritesEntries()
	{
		// Update equip states in the Favorites Menu for forms equipped by the co-op companion player.

		logger::debug("[MIM] InitFavoritesEntries.");
		// Ensure cached favorited items are up to date.
		const auto& em = glob.coopPlayers[managerMenuCID]->em;
		em->RefreshEquipState(RefreshSlots::kAll);
		em->SetFavoritedForms(true);
		// Update menu equip state with the refreshed favorites data.
		RefreshFavoritesMenuEquipState(true);
	}

	void MenuInputManager::InitMagicMenuEquippedStates()
	{
		// Update equip states in the Magic Menu for spells/shouts equipped by the co-op companion player.

		logger::debug("[MIM] InitMagicMenuEquippedStates: Menu just opened. Setting initial equip states.");
		// Ensure placeholder spell/shouts are NOT learned by P1.
		if (managerMenuCID != -1) 
		{
			const auto& p = glob.coopPlayers[managerMenuCID];
			auto placeholderSpell2H = p->em->placeholderMagic[!PlaceholderMagicIndex::k2H];
			auto placeholderSpellLH = p->em->placeholderMagic[!PlaceholderMagicIndex::kLH];
			auto placeholderSpellRH = p->em->placeholderMagic[!PlaceholderMagicIndex::kRH];
			
			if (glob.player1Actor->HasSpell(placeholderSpell2H->As<RE::SpellItem>()))
			{
				glob.player1Actor->RemoveSpell(placeholderSpell2H->As<RE::SpellItem>());
			}

			if (glob.player1Actor->HasSpell(placeholderSpellLH->As<RE::SpellItem>()))
			{
				glob.player1Actor->RemoveSpell(placeholderSpellLH->As<RE::SpellItem>());
			}

			if (glob.player1Actor->HasSpell(placeholderSpellRH->As<RE::SpellItem>()))
			{
				glob.player1Actor->RemoveSpell(placeholderSpellRH->As<RE::SpellItem>());
			}
		}

		// Set selectable magic forms list first.
		SetMagicMenuFormsList();
		// Update magic menu equip state after.
		RefreshMagicMenuEquipState(true);
	}
	
	void MenuInputManager::InitP1QSFormEntries()
	{
		// Set quick slot tags for any equipped quick slot items/spells and update index-to-entry map.

		logger::debug("[MIM] InitP1QSFormEntries.");
		auto ui = RE::UI::GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto taskInterface = SKSE::GetTaskInterface();
		if (!ui || !p1 || !taskInterface)
		{
			logger::error("[MIM] ERR: InitP1QSFormEntries: UI invalid: {}, player 1 invalid: {}, task interface invalid: {}.",
				(bool)!ui, (bool)!p1, (bool)!taskInterface);
			return;
		}

		if (ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME))
		{
			if (favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); favoritesMenu)
			{
				menuCoopActorHandle = p1->GetHandle();
				const auto& em = glob.coopPlayers[glob.player1CID]->em;
				const auto& favoritesList = favoritesMenu->favorites;
				// Clear the menu entry to list index map before reconstructing it below.
				favMenuIndexToEntryMap.clear();
				// Update cached P1 favorites.
				em->RefreshEquipState(RefreshSlots::kAll);
				em->SetFavoritedForms(true);

				// Iterate through the favorites list in order, get the list indices for
				// any quick slot forms and check if any current quick slot forms are still
				// in the favorites list.
				bool itemStillFavorited = false;
				bool spellStillFavorited = false;
				RE::TESForm* favForm = nullptr;
				for (auto i = 0; i < favoritesList.size(); ++i)
				{
					favForm = favoritesList[i].item;
					if (favForm->Is(RE::FormType::Spell))
					{
						// Check if quick slot spell is favorited.
						auto quickSlotSpell = em->quickSlotSpell;
						if (quickSlotSpell && quickSlotSpell == favForm)
						{
							logger::debug("[MIM] InitP1QSFormEntries: Quick slot spell {} favorited.",
								favForm->GetName());
							em->equippedQSSpellIndex = i;
							spellStillFavorited = true;
						}
					}
					else if (favForm->Is(RE::FormType::AlchemyItem, RE::FormType::Ingredient))
					{
						// Check if quick slot item is equipped.
						auto quickSlotItem = em->quickSlotItem;
						if (quickSlotItem && quickSlotItem == favForm)
						{
							logger::debug("[MIM] InitP1QSFormEntries: Quick slot item {} favorited.",
								favForm->GetName());
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

				// Update the Favorites Menu UI entries to reflect the initial equip state of quick slot items/spells
				// and update index-to-entry map.
				taskInterface->AddUITask(
					[this, &em, favoritesList]() {
						if (auto ui = RE::UI::GetSingleton(); ui)
						{
							if (auto favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); favoritesMenu && favoritesMenu.get())
							{
								if (auto view = favoritesMenu->uiMovie; view)
								{
									// Entry positions in the menu DO NOT correspond to their indices in the favorites list.
									// Have to get the index for each entry.
									RE::GFxValue entryList;
									view->CreateArray(std::addressof(entryList));
									view->GetVariable(std::addressof(entryList), "_root.MenuHolder.Menu_mc.itemList.entryList");
									double numEntries = view->GetVariableDouble("_root.MenuHolder.Menu_mc.itemList.entryList.length");
									for (uint32_t i = 0; i < numEntries; ++i)
									{
										RE::GFxValue entryIndex;
										RE::GFxValue entry;
										view->GetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", i, std::addressof(entry), 1);
										entry.GetMember("index", std::addressof(entryIndex));

										uint32_t index = static_cast<uint32_t>(entryIndex.GetNumber());
										RE::GFxValue entryText;
										entry.GetMember("text", std::addressof(entryText));
										std::string entryStr = entryText.GetString();

										// Update equip state for index.
										// Normal items receive an update to the "caret" equipped icon,
										// while quick slot items have their entry text modified.
										// This tag gets wiped whenever the favorites menu is opened,
										// so it must be re-applied each time.
										if (index == em->equippedQSItemIndex || index == em->equippedQSSpellIndex)
										{
											bool isConsumable = index == em->equippedQSItemIndex;
											if (entryStr.find("[*QS", 0) == std::string::npos)
											{
												entryStr = fmt::format("[*QS{}*] {}", isConsumable ? "I" : "S", entryStr);
												entryText.SetString(entryStr);
												entry.SetMember("text", entryText);
												view->SetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", i, std::addressof(entry), 1);
												logger::debug("[MIM] InitP1QSFormEntries: Entry text is now: {}", entryStr);
											}
										}

										// Insert (key = favorites list index, value = UI entry number) pairs into map.
										favMenuIndexToEntryMap.insert_or_assign(index, i);
									}

									// Update list to reflect changes.
									view->InvokeNoReturn("_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0);
								}
							}
						}
					});
			}
		}
	}

	void MenuInputManager::PerformEnderalSkillLevelUp(RE::AlchemyItem* a_skillbook)
	{
		// Get player requesting use to level up with the book (the player controlling the Inventory Menu).

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr || !a_skillbook)
		{
			logger::critical("[MIM] PerformEnderalSkillLevelUp: Menu player's ptr is invalid: {}, skillbook invalid: {}.",
				(bool)!menuCoopActorPtr, (bool)!a_skillbook);
			return;
		}

		logger::debug("[MIM] PerformEnderalSkillLevelUp: {} -> {}.", menuCoopActorPtr->GetName(), a_skillbook->GetName());
		auto skillAV = RE::ActorValue::kNone;
		auto skillbookTier = EnderalSkillbookTier::kTotal;
		const RE::FormID& fid = a_skillbook->formID;
		// Get tier and skill to level up for this skillbook.
		if (GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.contains(fid)) 
		{
			const auto& tierSkillPair = GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.at(fid);
			skillbookTier = tierSkillPair.first;
			skillAV = tierSkillPair.second;
			logger::debug("[MIM] PerformEnderalSkillLevelUp: Learning/crafting book {} teaches skill {}, tier {}.",
				a_skillbook->GetName(), skillAV, skillbookTier);
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
					// REMOVE when done debugging.
					logger::debug("[MIM] PerformEnderalSkillLevelUp: {}'s skill {} is at {}, which matches the {} tier. Can use book {} to level up skill.",
						menuCoopActorPtr->GetName(), skillName, avLvl, skillbookTier, a_skillbook->GetName());
					canUseToLevelUp = true;
				}

				break;
			}
			case EnderalSkillbookTier::kAdept:
			{
				if (avLvl <= 49.0f)
				{
					// REMOVE when done debugging.
					logger::debug("[MIM] PerformEnderalSkillLevelUp: {}'s skill {} is at {}, which matches the {} tier. Can use book {} to level up skill.",
						menuCoopActorPtr->GetName(), skillName, avLvl, skillbookTier, a_skillbook->GetName());
					canUseToLevelUp = true;
				}

				break;
			}
			case EnderalSkillbookTier::kExpert:
			{
				if (avLvl <= 74.0f)
				{
					// REMOVE when done debugging.
					logger::debug("[MIM] PerformEnderalSkillLevelUp: {}'s skill {} is at {}, which matches the {} tier. Can use book {} to level up skill.",
						menuCoopActorPtr->GetName(), skillName, avLvl, skillbookTier, a_skillbook->GetName());
					canUseToLevelUp = true;
				}

				break;
			}
			case EnderalSkillbookTier::kMaster:
			{
				if (avLvl <= 99.0f)
				{
					// REMOVE when done debugging.
					logger::debug("[MIM] PerformEnderalSkillLevelUp: {}'s skill {} is at {}, which matches the {} tier. Can use book {} to level up skill.",
						menuCoopActorPtr->GetName(), skillName, avLvl, skillbookTier, a_skillbook->GetName());
					canUseToLevelUp = true;
				}

				break;
			}
			default:
			{
				logger::error("[MIM] ERR: PerformEnderalSkillLevelUp: Could not get tier for learning/crafting book {}.",
					a_skillbook->GetName(), skillAV);
				break;
			}
			}
		}
		
		// Check if the skill to level up is shared.
		bool isShared = GlobalCoopData::SHARED_SKILL_AVS_SET.contains(skillAV);
		// The skills which are leveled via crafting points are the same as the shared skills in co-op.
		// What a happy coincidence!
		bool pointsAvailable = 
		{
			(!isShared && glob.learningPointsGlob->value > 0.0f) ||
			(isShared && glob.craftingPointsGlob->value > 0.0f)
		};

		if (canUseToLevelUp && pointsAvailable)
		{
			// REMOVE when done debugging.
			logger::debug("[MIM] PerformEnderalSkillLevelUp: Learning/crafting book {} levels up skill {}. Level up skill for {}.",
				a_skillbook->GetName(), skillAV, menuCoopActorPtr->GetName());

			auto& skillIncList = glob.serializablePlayerData.at(menuCoopActorPtr->formID)->skillLevelIncreasesList;
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

			if (skillAVIndex != -1)
			{
				const float avLvl = menuCoopActorPtr->GetBaseActorValue(skillAV);

				// REMOVE when done debugging.
				logger::debug("[MIM] PerformEnderalSkillLevelUp: {} leveled up Skill {} from level {} to {} by reading {}. Skill inc list entry goes from {} to {}.",
					menuCoopActorPtr->GetName(),
					skillAV,
					avLvl,
					avLvl + 1,
					a_skillbook->GetName(),
					skillIncList[skillAVIndex],
					skillIncList[skillAVIndex] + 1);

				// Update serialized skill increments list and increment skill AV level.
				skillIncList[skillAVIndex]++;
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
				RE::DebugMessageBox(fmt::format(
					"[ALYSLC] {} increased to {}! {} Points left: {}",
					skillName,
					avLvl + 1,
					isShared ? "Crafting" : "Learning",
					isShared ? glob.craftingPointsGlob->value : glob.learningPointsGlob->value).c_str()
				);

				// Remove consumed book.
				menuCoopActorPtr->RemoveItem(a_skillbook, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				// Refresh item list.
				shouldRefreshMenu = true;
			}
		}
		else
		{
			if (!pointsAvailable)
			{
				// No points to use.
				RE::DebugMessageBox(fmt::format("[ALYSLC] You do not have enough {} Points!", isShared ? "Crafting" : "Learning").c_str());
			}
			else
			{
				if (skillAV != RE::ActorValue::kNone)
				{
					// Not the correct tier.
					RE::DebugMessageBox(fmt::format("[ALYSLC] You already have developed this skill too well to benefit from this learning/crafting book!", a_skillbook->GetName()).c_str());
				}
				else
				{
					// Not valid for leveling.
					RE::DebugMessageBox(fmt::format("[ALYSLC] Cannot use {} to level up skill.", a_skillbook->GetName()).c_str());
				}
			}
		}
	}

	void MenuInputManager::ProcessBarterMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle BarterMenu input.

		auto ue = RE::UserEvents::GetSingleton(); 
		if (!barterMenu || !ue) 
		{
			logger::error("[MIM] ERR: ProcessBaterMenuButtonInput: Menu invalid: {}, user events invalid: {}.",
				(bool)!barterMenu, (bool)!ue);
			return;
		}

		// Default: 'A' button to sell item.
		auto acceptIDCode = controlMap->GetMappedKey(ue->accept, gamepadDevice, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
		if (a_xMask == glob.cdh->GAMEMASK_TO_XIMASK.at(acceptIDCode))
		{
			// Ensure that the co-op companion cannot sell any object that is currently equipped by player 1.
			if (auto selectedItem = barterMenu->itemList->GetSelectedItem(); selectedItem && selectedItem->data.GetEquipState() != 0)
			{
				RE::DebugMessageBox("[ALYSLC] Cannot sell an item currently equipped by Player 1!");
				currentMenuInputType = MenuInputEventType::kPressedNoEvent;
			}
		}
		if (barterMenu->itemList)
		{
			if (auto currentItem = barterMenu->itemList->GetSelectedItem(); !currentItem)
			{
				// Should not refresh otherwise, since refreshing while attempting to select the number of
				// an item to sell through the bottom right item card quantity menu (I don't know the proper name for it)
				// glitches the entire barter menu until the item card quantity menu is closed.
				shouldRefreshMenu = true;
			}
		}
	}

	void MenuInputManager::ProcessBookMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle BookMenu input.

		auto ue = RE::UserEvents::GetSingleton();
		if (!bookMenu || !ue)
		{
			logger::error("[MIM] ERR: ProcessBookMenuButtonInput: Menu invalid: {}, user events invalid: {}.",
				(bool)!bookMenu, (bool)!ue);
			return;
		}

		auto acceptIDCode = controlMap->GetMappedKey(ue->accept, gamepadDevice, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode); 
		// Take book.
		if (a_xMask == glob.cdh->GAMEMASK_TO_XIMASK.at(acceptIDCode))
		{
			auto cancelIDCode = controlMap->GetMappedKey(ue->cancel, gamepadDevice, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
			// Opened while the book refr is in worldspace, and not in the player's inventory. Can loot and give to P1.
			if (auto bookRef = bookMenu->GetTargetReference(); bookRef && bookRef->GetObjectReference())
			{
				// Close the book menu.
				std::unique_ptr<RE::InputEvent* const> buttonEvent = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(gamepadDevice, ue->cancel, cancelIDCode, 1.0f, 0.0f));
				(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
				std::unique_ptr<RE::InputEvent* const> buttonEvent2 = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(gamepadDevice, ue->cancel, cancelIDCode, 0.0f, 1.0f));
				(*buttonEvent2.get())->AsIDEvent()->pad24 = 0xCA11;
				Util::SendInputEvent(buttonEvent);
				Util::SendInputEvent(buttonEvent2);

				if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
				{
					// Add book to P1 after exiting the menu.
					p1->PickUpObject(bookRef, 1);
				}

				currentMenuInputType = MenuInputEventType::kPressedNoEvent;
			}
		}
	}

	void MenuInputManager::ProcessContainerMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle ContainerMenu input.

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			logger::error("[MIM] ERR: ProcessContainerMenuButtonInput: menu player's ptr is invalid.");
			return;
		}

		auto ue = RE::UserEvents::GetSingleton();
		if (!containerMenu || !ue)
		{
			logger::error("[MIM] ERR: ProcessContainerMenuButtonInput: Menu invalid: {}, user events invalid: {}.",
				(bool)!containerMenu, (bool)!ue);
			return;
		}

		auto containerRefr = RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle());
		const auto& mode = containerMenu->GetContainerMode();
		RE::ItemList::Item* selectedItem = GetSelectedItem(containerMenu->itemList);

		// NOTE: Inventory tab switch needs thorough testing for bugs.
		// TEST: Disable switch to P1 inventory since item transfer from P1 to a coop player has been moved to the Gift Menu.
		// In addition, the container menu's data corrupts switching over to P1's inventory and then exiting the menu.
		// Upon re-entering the co-op player's inventory and switching to P1's inventory, the item list is broken
		// and attempting to retrieve the currently selected item returns nothing.

		// Don't allow switching to P1's inventory when looting a container (not another player's inventory).
		if (!isCoopInventory && glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask) == controlMap->GetMappedKey(ue->wait, gamepadDevice))
		{
			RE::DebugNotification("[ALYSLC] P1's inventory is not accessible to other players while looting.");
			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
		}

		if (a_xMask == XINPUT_GAMEPAD_X)
		{
			// Drop one of the item when in the player's inventory.
			if (isCoopInventory)
			{
				// Handled here; no event to send.
				currentMenuInputType = MenuInputEventType::kPressedNoEvent;
				if (selectedItem)
				{
					auto boundObj = selectedItem->data.objDesc->object;
					RE::DebugNotification(fmt::format("{} is dropping 1 {}", menuCoopActorPtr->GetName(), boundObj->GetName()).c_str());
					// Place in front of player.
					auto dropPos = Util::Get3DCenterPos(menuCoopActorPtr.get()) + Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(menuCoopActorPtr->GetHeading(false))) * 0.5f * menuCoopActorPtr->GetHeight();
					int32_t currentCount = 0;
					auto inventory = menuCoopActorPtr->GetInventory();
					if (inventory.contains(boundObj))
					{
						currentCount = inventory.at(boundObj).first;
					}

					// If none of this item will remain after dropping one.
					if (currentCount <= 1)
					{
						const auto& p = glob.coopPlayers[managerMenuCID];
						// Remove dropped object from desired equipped objects list if no more remain in the player's inventory.
						if (p->isActive)
						{
							auto foundIter = std::find_if(p->em->desiredEquippedForms.begin(), p->em->desiredEquippedForms.end(), [boundObj](RE::TESForm* a_form) { return a_form == boundObj; });
							if (foundIter != p->em->desiredEquippedForms.end())
							{
								auto index = foundIter - p->em->desiredEquippedForms.begin();
								p->em->desiredEquippedForms[index] = nullptr;
							}
						}
					}

					// Unequip before dropping to avoid crash.
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem) 
					{
						aem->UnequipObject(menuCoopActorPtr.get(), boundObj);
					}

					auto droppedRefrHandle = menuCoopActorPtr->RemoveItem(boundObj, 1, RE::ITEM_REMOVE_REASON::kDropping, nullptr, nullptr, &dropPos);
					shouldRefreshMenu = true;
				}
			}
			// Take all when looting from a container.
			// No need to have a selected item.
			// Everything is looted by P1 and then transfered to the menu-controlling player.
			else if (mode == RE::ContainerMenu::ContainerMode::kLoot || mode == RE::ContainerMenu::ContainerMode::kNPCMode)
			{
				logger::debug("[MIM] ProcessContainerMenuButtonInput: Non-co-op entity container. Not stealing/pickpocketing. Take all.");
			}
		}

		if (selectedItem)
		{
			if (a_xMask == XINPUT_GAMEPAD_A)
			{
				if (selectedForm = selectedItem->data.objDesc->object; selectedForm)
				{
					// NOTE: Keeping commented out until inventory tab switch is thoroughly tested for bugs.
					// Can transfer keys/gold through inventory since the gift menu does not allow for transfer of these items to P1.
				
					//bool isGiftMenuBlacklistedItem = selectedForm->IsGold() || selectedForm->IsKey();
					//if (isGiftMenuBlacklistedItem || (!isCoopInventory && (mode == RE::ContainerMenu::ContainerMode::kLoot || mode == RE::ContainerMenu::ContainerMode::kNPCMode)))
					//{
					//	logger::debug("[MIM] ProcessContainerMenuButtonInput: Adding (x1) {} to {}.", selectedForm->GetName(), menuCoopActorPtr->GetName());
					//}
					//else if (isCoopInventory)
					//{ 
					//	// No transfer to P1 here. Now using the Gift Menu.
					//	RE::DebugMessageBox("[ALYSLC] Aside from gold and keys, please use the Gift Menu to transfer items between players.");
					//	currentMenuInputType = MenuInputEventType::kPressedNoEvent;
					//}
				}
			}
			// Favorite the selected item.
			else if (a_xMask == XINPUT_GAMEPAD_Y)
			{
				// Handled here; no event to send.
				currentMenuInputType = MenuInputEventType::kPressedNoEvent;
				// Favorite the item when in the player's inventory.
				// Credit to po3 for the code to check if the item has been favorited.
				// From an older version of:
				// https://github.com/powerof3/PapyrusExtenderSSE/
				if (isCoopInventory)
				{
					selectedForm = selectedItem->data.objDesc->object;
					if (selectedForm)
					{
						shouldFavorite = true;
						auto inventory = menuCoopActorPtr->GetInventory();
						RE::InventoryEntryData* entryData = nullptr;
						for (auto& inventoryEntry : inventory)
						{
							if (inventoryEntry.first && inventoryEntry.first == selectedForm)
							{
								auto& entryData = inventoryEntry.second.second;
								if (entryData)
								{
									auto inventoryChanges = menuCoopActorPtr->GetInventoryChanges();
									if (entryData->extraLists && !entryData->extraLists->empty())
									{
										RE::ExtraDataList* exDataList = nullptr;
										for (auto& exData : *entryData->extraLists)
										{
											// Already has favorited data, so unfavorite instead.
											if (exData->HasType(RE::ExtraDataType::kHotkey))
											{
												shouldFavorite = false;
												exDataList = exData;
												break;
											}
										}

										if (shouldFavorite)
										{
											exDataList = entryData->extraLists->front();
											if (inventoryChanges)
											{
												logger::debug("[MIM] ProcessContainerMenuButtonInput: Favoriting {}", selectedForm->GetName());
												Util::NativeFunctions::Favorite(inventoryChanges, entryData.get(), exDataList);
											}
										}
										else
										{
											if (inventoryChanges)
											{
												logger::debug("[MIM] ProcessContainerMenuButtonInput: Unfavoriting {}", selectedForm->GetName());
												Util::NativeFunctions::Unfavorite(inventoryChanges, entryData.get(), exDataList);
											}
										}

										const auto& em = glob.coopPlayers[managerMenuCID]->em;
										// Since the player's favorited physical forms have changed, 
										// update the co-op player's corresponding list of cyclable forms.
										switch (*selectedForm->formType)
										{
										case RE::FormType::Ammo:
										{
											em->SetCyclableFavForms(CyclableForms::kAmmo);
											break;
										}
										case RE::FormType::Weapon:
										{
											em->SetCyclableFavForms(CyclableForms::kWeapon);
											break;
										}
										default:
										{
											break;
										}
										}

										break;
									}
									else
									{
										// Entry data may not have a list of extra data, but we can still favorite the item.
										Util::NativeFunctions::Favorite(inventoryChanges, entryData.get(), nullptr);
									}
								}
							}
						}

						// Refresh menu to display the changed favorites status indicator.
						shouldRefreshMenu = true;
					}
				}
			}
		}
	}

	void MenuInputManager::ProcessDialogueMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle DialogueMenu input.

		auto ue = RE::UserEvents::GetSingleton();
		if (!ue)
		{
			logger::error("[MIM] ERR: ProcessDialogueMenuButtonInput: User events invalid.", (bool)!ue);
			return;
		}

		auto buttonGroup = glob.paInfoHolder->XIMASKS_TO_INPUT_GROUPS.at(a_xMask);
		const auto gameMask = glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask);
		auto acceptIDCode = controlMap->GetMappedKey(ue->accept, gamepadDevice, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
		auto cancelIDCode = controlMap->GetMappedKey(ue->cancel, gamepadDevice, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
		// Can only use the DPad to navigate and either close the menu or choose dialogue options.
		// Block all other controls from being emulated.
		if (buttonGroup != InputGroup::kDPad && gameMask != cancelIDCode && gameMask != acceptIDCode)
		{
			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
		}
	}

	void MenuInputManager::ProcessFavoritesMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle FavoritesMenu input.

		// Attempt to equip item in the quick slot.
		if (a_xMask == XINPUT_GAMEPAD_START)
		{
			auto taskInterface = SKSE::GetTaskInterface();
			// Can't update QS tag if task interface is invalid.
			if (!taskInterface)
			{
				logger::error("[MIM] ERR: ProcessFavoritesMenuButtonInput: Task interface invalid.");
				return;
			}

			taskInterface->AddUITask(
				[this]() {
					if (auto ui = RE::UI::GetSingleton(); ui)
					{
						if (auto favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); favoritesMenu && favoritesMenu.get())
						{
							if (auto view = favoritesMenu->uiMovie; view)
							{
								RE::GFxValue selectedIndex;
								view->GetVariable(std::addressof(selectedIndex), "_root.MenuHolder.Menu_mc.itemList.selectedEntry.index");

								// Index in favorites list.
								uint32_t index = static_cast<uint32_t>(selectedIndex.GetNumber());
								uint32_t selectedEntryNum = favMenuIndexToEntryMap.at(index);
								auto form = favoritesMenu->favorites[index].item;

								// Quick slot item.
								if (form)
								{
									const auto& em = glob.coopPlayers[managerMenuCID]->em;
									bool isConsumable = form->Is(RE::FormType::AlchemyItem, RE::FormType::Ingredient);
									bool isSpell = form->Is(RE::FormType::Spell);
									if (isConsumable || isSpell)
									{
										bool shouldEquip = false;
										// Check if the selected item is the player's current quick slot item/spell,
										// and if it is, remove the equipped tag. Otherwise, add the equipped tag.
										if (isConsumable)
										{
											em->quickSlotItem = form == em->quickSlotItem ? nullptr : form;
											shouldEquip = em->quickSlotItem;
										}
										else
										{
											em->quickSlotSpell = form == em->quickSlotSpell ? nullptr : form->As<RE::SpellItem>();
											shouldEquip = em->quickSlotSpell;
										}

										RE::GFxValue entry;
										view->GetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", selectedEntryNum, std::addressof(entry), 1);
										RE::GFxValue entryText;
										entry.GetMember("text", std::addressof(entryText));
										std::string entryStr = entryText.GetString();

										if (shouldEquip)
										{
											entryStr = fmt::format("[*QS{}*] {}", isConsumable ? "I" : "S", entryStr);
											// Previously equipped item/spell's index.
											uint32_t equippedQSIndex = isConsumable ? em->equippedQSItemIndex : em->equippedQSSpellIndex;
											// Remove item/spell tag from old quick slot item/spell entry if needed.
											if (equippedQSIndex != -1 && index != equippedQSIndex)
											{
												uint32_t oldEntryNum = favMenuIndexToEntryMap[equippedQSIndex];
												RE::GFxValue oldEntry;
												view->GetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", oldEntryNum, std::addressof(oldEntry), 1);
												RE::GFxValue oldEntryText;
												oldEntry.GetMember("text", std::addressof(oldEntryText));
												std::string oldEntryStr = oldEntryText.GetString();

												auto qsTagStartIndex = oldEntryStr.find("[*QS", 0);
												if (qsTagStartIndex != std::string::npos && qsTagStartIndex + qsPrefixTagLength <= oldEntryStr.length())
												{
													oldEntryStr = oldEntryStr.substr(qsTagStartIndex + qsPrefixTagLength);
													oldEntryText.SetString(oldEntryStr);
													oldEntry.SetMember("text", oldEntryText);
													view->SetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", oldEntryNum, std::addressof(oldEntry), 1);
												}
											}
											else
											{
												// Should not happen if everything is working.
												logger::critical("[MIM] ProcessFavoritesMenuButtonInput: Same item in quick slot after (un)equip request: {}, equippedIndex is -1: {}, old and new indices are the same: {} ({}).",
													index == equippedQSIndex, equippedQSIndex == -1, equippedQSIndex == index, equippedQSIndex);
											}
										}
										else
										{
											// 'Unequipping': remove the equipped tag.
											auto qsTagStartIndex = entryStr.find("[*QS", 0);
											if (qsTagStartIndex != std::string::npos && qsTagStartIndex + qsPrefixTagLength <= entryStr.length())
											{
												entryStr = entryStr.substr(qsTagStartIndex + qsPrefixTagLength);
											}
										}

										logger::debug("[MIM] ProcessFavoritesMenuButtonInput: {}'s entry text is now: {}", form->GetName(), entryStr);
										// Update text and entry list.
										entryText.SetString(entryStr);
										entry.SetMember("text", entryText);
										view->SetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", selectedEntryNum, std::addressof(entry), 1);
										view->InvokeNoReturn("_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0);

										// Set new equipped quick slot item/spell index.
										if (isConsumable)
										{
											em->equippedQSItemIndex = shouldEquip ? index : -1;
											logger::debug("[MIM] ProcessFavoritesMenuButtonInput: {}, index {}, is now {} in the item quick slot.",
												form->GetName(), em->equippedQSItemIndex, shouldEquip ? "equipped" : "unequipped");
										}
										else
										{
											em->equippedQSSpellIndex = shouldEquip ? index : -1;
											logger::debug("[MIM] ProcessFavoritesMenuButtonInput: {}, index {}, is now {} in the spell quick slot.",
												form->GetName(), em->equippedQSSpellIndex, shouldEquip ? "equipped" : "unequipped");
										}

										// Refresh equip state after (un)equipping quick slot item/spell.
										glob.coopPlayers[managerMenuCID]->em->RefreshEquipState(RefreshSlots::kAll);
									}
								}
							}
						}
					}
				});

			// No event to handle.
			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
		}
		else if (a_xMask == XINPUT_GAMEPAD_DPAD_LEFT || a_xMask == XINPUT_GAMEPAD_DPAD_RIGHT)
		{
			// Changing categories clears our changes to the equip "carets", (the empty/LH/RH arrow to the left of each equipped menu entry),
			// and imports P1's item equip state. Have to reimport the co-op companion player's equip state, 
			// but no need to refresh the cached equipped data, which has not changed.
			RefreshFavoritesMenuEquipState(false);
		}
		else if (a_xMask == XINPUT_GAMEPAD_A)
		{
			// Ignore equip attempts with the "A" button, as emulating input here will equip this player's favorited item on P1.
			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
		}
	}

	void MenuInputManager::ProcessInventoryMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle InventoryMenu input.

		auto ue = RE::UserEvents::GetSingleton();
		if (!ue)
		{
			logger::error("[MIM] ERR: ProcessInventoryMenuButtonInput: User events invalid.");
			return;
		}

		auto buttonGroup = glob.paInfoHolder->XIMASKS_TO_INPUT_GROUPS.at(a_xMask);
		// Can only move through P1's inventory or exit.
		auto cancelIDCode = controlMap->GetMappedKey(ue->cancel, gamepadDevice, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
		if (buttonGroup != InputGroup::kDPad && glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask) != cancelIDCode)
		{
			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
		}
	}

	void MenuInputManager::ProcessLootMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle LootMenu input.

		auto ue = RE::UserEvents::GetSingleton();
		if (!ue)
		{
			logger::error("[MIM] ERR: ProcessLootMenuButtonInput: User events invalid.");
			return;
		}

		auto buttonGroup = glob.paInfoHolder->XIMASKS_TO_INPUT_GROUPS.at(a_xMask);
		const auto gameMask = glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask);
		auto acceptIDCode = controlMap->GetMappedKey(ue->accept, gamepadDevice, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
		auto cancelIDCode = controlMap->GetMappedKey(ue->cancel, gamepadDevice, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
		auto readyWeaponIDCode = controlMap->GetMappedKey(ue->readyWeapon, gamepadDevice, RE::UserEvents::INPUT_CONTEXT_ID::kGameplay);
		
		// Close the LootMenu with cancel bind.
		if (gameMask == cancelIDCode)
		{
			// Exit menu and relinquish control when cancel bind is pressed.
			if (auto crosshairPickData = RE::CrosshairPickData::GetSingleton(); crosshairPickData)
			{
				Util::SendCrosshairEvent(nullptr);
			}

			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
		}
		else if (buttonGroup != InputGroup::kDPad && gameMask != cancelIDCode && gameMask != acceptIDCode && gameMask != readyWeaponIDCode)
		{
			// Ignore all button presses (do not send emulated input event)
			// that are not the DPad or the cancel, accept, or ready weapon binds.
			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
		}
	}

	void MenuInputManager::ProcessMagicMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle MagicMenu input.

		if (a_xMask == XINPUT_GAMEPAD_A)
		{
			shouldRefreshMenu = true;
		}
		else if (a_xMask == XINPUT_GAMEPAD_Y)
		{
			// Save selected form to add to cyclable spells list later after
			// the spell is favorited.
			selectedForm = GetSelectedMagicMenuSpell();
			spellFavoriteStatusChanged = true;
			shouldRefreshMenu = true;
		}
		else if (a_xMask == XINPUT_GAMEPAD_DPAD_LEFT || a_xMask == XINPUT_GAMEPAD_DPAD_RIGHT)
		{
			// Changing categories clears our changes to the equip "carets", (the empty/LH/RH arrow to the left of each equipped menu entry),
			// and imports P1's spell equip state. Have to reimport the co-op companion player's equip state,
			// but no need to refresh the cached equipped data, which has not changed.
			RefreshMagicMenuEquipState(false);
		}
	}

	void MenuInputManager::ProcessMapMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle MapMenu input.

		// All inputs forwarded as emulated P1 inputs.
		currentMenuInputType = MenuInputEventType::kEmulateInput;
	}

	void MenuInputManager::ProcessTriggerInput(const bool& a_isLT)
	{
		// Perform menu-dependent actions based on trigger press.

		logger::error("[MIM] ProcessTriggerInput: {}.", menuName);
		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			logger::error("[MIM] ERR: ProcessTriggerInput: Menu-controlling player is invalid.");
			return;
		}

		switch (openedMenuType)
		{
		case SupportedMenu::kContainer:
		{
			if (!containerMenu) 
			{
				logger::error("[MIM] ERR: ProcessTriggerInput: ContainerMenu invalid.");
				return;
			}

			// No events to send by default. Do not want to equip selected items onto P1 through trigger presses.
			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
			RE::ItemList::Item* selectedItem = GetSelectedItem(containerMenu->itemList);
			if (selectedItem)
			{
				auto obj = selectedItem ? selectedItem->data.objDesc->object : nullptr; 
				if (obj)
				{
					if (auto asBook = obj->As<RE::TESObjectBOOK>(); asBook)
					{
						// If this is a skillbook, co-op companions will level up the accompanying skill in the activation hook.
						if (const auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
						{
							currentMenuInputType = MenuInputEventType::kEmulateInput;
						}
					}
					else if (ALYSLC::EnderalCompat::g_enderalSSEInstalled && obj->As<RE::AlchemyItem>() && obj->As<RE::AlchemyItem>()->HasKeywordString("Lehrbuch"))
					{
						auto item = obj->As<RE::AlchemyItem>();
						// Not a +2 learning points or +1 memory point book.
						// These books can be used by P1 to gain points for the entire party and aren't handled here.
						if (item->formID != 0xCE135 && item->formID != 0x12E1FC)
						{
							// Level up with Enderal skillbook.
							PerformEnderalSkillLevelUp(item);
							currentMenuInputType = MenuInputEventType::kPressedNoEvent;
						}
					}
					else
					{
						// Setup equip request.
						currentMenuInputType = MenuInputEventType::kEquipReq;
						fromContainerHandle = isCoopInventory ? menuCoopActorHandle : menuContainerHandle;
						reqEquipIndex = a_isLT ? EquipIndex::kLeftHand : EquipIndex::kRightHand;
						selectedForm = obj;
						placeholderMagicChanged = false;
						
						delayedEquipStateRefresh = true;
						lastEquipStateRefreshReqTP = SteadyClock::now();

						// If container reference is not the player, add the item-to-equip to the co-op player.
						if (!isCoopInventory && obj)
						{
							logger::debug("[MIM] ProcessTriggerInput: Container Menu: moving {} to player inventory before equipping.", obj->GetName());
							if (auto containerRefrPtr = Util::GetRefrPtrFromHandle(menuContainerHandle); containerRefrPtr) 
							{
								containerRefrPtr->RemoveItem(obj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, menuCoopActorPtr.get());
							}
						}
					}
				}
				else
				{
					logger::debug("[MIM] ProcessTriggerInput: Container Menu: no selected object.");
				}
			}
			else
			{
				logger::debug("[MIM] ProcessTriggerInput: Container Menu: no selected item.");
			}

			break;
		}
		case SupportedMenu::kFavorites:
		{
			if (!favoritesMenu)
			{
				logger::error("[MIM] ERR: ProcessTriggerInput: FavoritesMenu invalid.");
				return;
			}

			if (auto view = favoritesMenu->uiMovie; view)
			{
				// Get the selected form from the selected entry's index.
				RE::GFxValue selectedIndex;
				RE::GFxValue selectedFID;
				view->GetVariable(std::addressof(selectedIndex), "_root.MenuHolder.Menu_mc.itemList.selectedEntry.index");
				uint32_t index = static_cast<uint32_t>(selectedIndex.GetNumber());
				selectedForm = favoritesMenu->favorites[index].item;
				auto boundObj = selectedForm->As<RE::TESBoundObject>();
				// If the favorited item is a spell/shout or a physical item that exists
				// in the co-op player's inventory, then attempt to equip and update the menu.
				bool equipable = 
				{
					(selectedForm) &&
					(selectedForm->Is(RE::FormType::Shout, RE::FormType::Spell)) ||
					(boundObj && menuCoopActorPtr->GetInventory().contains(boundObj) && menuCoopActorPtr->GetInventory().at(boundObj).first > 0)
				};
				if (equipable)
				{
					currentMenuInputType = MenuInputEventType::kEquipReq;
					fromContainerHandle = menuCoopActorHandle;
					reqEquipIndex = EquipIndex::kRightHand;
					EntryEquipState newEquipState = EntryEquipState::kNone;
					if (selectedForm->Is(RE::FormType::Spell, RE::FormType::Weapon))
					{
						auto asSpell = selectedForm->As<RE::SpellItem>();
						auto asWeapon = selectedForm->As<RE::TESObjectWEAP>();
						// Weapon or hand spell.
						if ((asWeapon) || (asSpell && asSpell->GetSpellType() == RE::MagicSystem::SpellType::kSpell))
						{
							auto equipType = selectedForm->As<RE::BGSEquipType>();
							if (equipType && equipType->equipSlot->flags.any(RE::BGSEquipSlot::Flag::kUseAllParents))
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
								// The RH life drain spell is pre-determined by player level and stays equipped.
								bool isVampireLord = Util::IsVampireLord(glob.coopPlayers[managerMenuCID]->coopActor.get());
								newEquipState = a_isLT || isVampireLord ? EntryEquipState::kLH : EntryEquipState::kRH;
								reqEquipIndex = a_isLT || isVampireLord ? EquipIndex::kLeftHand : EquipIndex::kRightHand;
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
					else if (selectedForm->Is(RE::FormType::Ammo, RE::FormType::Armor, RE::FormType::Armature, RE::FormType::Shout))
					{
						// NOTE: Shield is equipped to left hand slot but uses the default entry equip caret.
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
						delayedEquipStateRefresh = true;
						lastEquipStateRefreshReqTP = SteadyClock::now();
					}
					else if (selectedForm->Is(RE::FormType::AlchemyItem, RE::FormType::Ingredient))
					{
						UpdateFavoritedConsumableCount(selectedForm, index);
					}

					auto spellToEquip = selectedForm->As<RE::SpellItem>();
					bool isHandSlotSpell = spellToEquip && spellToEquip->GetSpellType() == RE::MagicSystem::SpellType::kSpell;
					const auto& em = glob.coopPlayers[managerMenuCID]->em;
					if (isHandSlotSpell)
					{
						// Check if a hand placeholder magic form is about to be changed.
						if (newEquipState == EntryEquipState::kRH)
						{
							placeholderMagicChanged = selectedForm->formID != em->copiedMagicFormIDs[!PlaceholderMagicIndex::kRH];
						}
						else if (newEquipState == EntryEquipState::kLH)
						{
							placeholderMagicChanged = selectedForm->formID != em->copiedMagicFormIDs[!PlaceholderMagicIndex::kLH];
						}
						else if (newEquipState == EntryEquipState::kBothHands)
						{
							placeholderMagicChanged = selectedForm->formID != em->copiedMagicFormIDs[!PlaceholderMagicIndex::k2H];
						}

						logger::debug("[MIM] ProcessTriggerInput: FavoritesMenu: About to (un)equip hand spell {} (0x{:X}) from equip index {}, event: {}, placeholder magic changed: {}",
							spellToEquip->GetName(), spellToEquip->formID, reqEquipIndex, currentMenuInputType, placeholderMagicChanged);
					}
					else if (spellToEquip)
					{
						// Is voice spell, no copying to placeholder.
						placeholderMagicChanged = false;
						logger::debug("[MIM] ProcessTriggerInput: FavoritesMenu: About to (un)equip voice spell {} (0x{:X}) from equip index {}, event: {}, placeholder magic changed: {}",
							spellToEquip->GetName(), spellToEquip->formID, reqEquipIndex, currentMenuInputType, placeholderMagicChanged);
					}
				}
				else
				{
					// Not equipable.
					selectedForm = nullptr;
				}
			}

			break;
		}
		case SupportedMenu::kInventory:
		{
			// Do not send emulated trigger input because this qill
			// equip the selected item onto P1, not the player controlling menus.
			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
			break;
		}
		case SupportedMenu::kMagic:
		{
			if (!magicMenu)
			{
				logger::error("[MIM] ERR: ProcessTriggerInput: MagicMenu invalid.");
				return;
			}

			if (selectedForm = GetSelectedMagicMenuSpell(); selectedForm)
			{
				if (auto magicItemList = reinterpret_cast<RE::ItemList*>(magicMenu->unk30); magicItemList)
				{
					currentMenuInputType = MenuInputEventType::kEquipReq;
					fromContainerHandle = menuCoopActorHandle;
					reqEquipIndex = EquipIndex::kRightHand;
					EntryEquipState newEquipState = EntryEquipState::kNone;

					auto spellToEquip = selectedForm->As<RE::SpellItem>();
					bool isHandSlotSpell = spellToEquip && spellToEquip->GetSpellType() == RE::MagicSystem::SpellType::kSpell;
					if (isHandSlotSpell)
					{
						if (spellToEquip->equipSlot->flags.any(RE::BGSEquipSlot::Flag::kUseAllParents))
						{
							newEquipState = EntryEquipState::kBothHands;
							// Right hand index for 2H equip req.
							reqEquipIndex = EquipIndex::kRightHand;
						}
						else
						{
							newEquipState = a_isLT ? EntryEquipState::kLH : EntryEquipState::kRH;
							reqEquipIndex = a_isLT ? EquipIndex::kLeftHand : EquipIndex::kRightHand;
						}

						// Check if the placeholder spell is about to be changed.
						const auto& em = glob.coopPlayers[managerMenuCID]->em;
						if (newEquipState == EntryEquipState::kRH)
						{
							placeholderMagicChanged = selectedForm->formID != em->copiedMagicFormIDs[!PlaceholderMagicIndex::kRH];
						}
						else if (newEquipState == EntryEquipState::kLH)
						{
							placeholderMagicChanged = selectedForm->formID != em->copiedMagicFormIDs[!PlaceholderMagicIndex::kLH];
						}
						else if (newEquipState == EntryEquipState::kBothHands)
						{
							placeholderMagicChanged = selectedForm->formID != em->copiedMagicFormIDs[!PlaceholderMagicIndex::k2H];
						}

						// REMOVE when done debugging.
						logger::debug("[MIM] ProcessTriggerInput: MagicMenu: Trying to equip hand magic form [{}] {}. Placeholder magic changed: {}.",
							selectedForm->Is(RE::FormType::Spell) ? "spell" : selectedForm->Is(RE::FormType::Shout) ? "shout" : "NOT EQUIPABLE",
							selectedForm->GetName(),
							placeholderMagicChanged);
					}
					else
					{
						// REMOVE when done debugging.
						logger::debug("[MIM] ProcessTriggerInput: MagicMenu: Trying to equip magic form [{}] {}.",
							selectedForm->Is(RE::FormType::Spell) ? "spell" : selectedForm->Is(RE::FormType::Shout) ? "shout" : "NOT EQUIPABLE",
							selectedForm->GetName());

						newEquipState = EntryEquipState::kDefault;
						// Voice slot for power/shout/any other magic.
						reqEquipIndex = EquipIndex::kVoice;
					}

					// Signal to update equip states for spells.
					delayedEquipStateRefresh = true;
					lastEquipStateRefreshReqTP = SteadyClock::now();
				}
			}
			else
			{
				logger::debug("[MIM] ProcessTriggerInput MagicMenu: Could not get selected magic menu spell.");
				RE::DebugNotification(fmt::format("[ALYSLC] Cannot equip this spell for {}", menuCoopActorPtr->GetName()).c_str());
				currentMenuInputType = MenuInputEventType::kPressedNoEvent;
			}

			break;
		}
		case SupportedMenu::kLoot:
		{
			// No event to send in Loot Menu.
			currentMenuInputType = MenuInputEventType::kPressedNoEvent;
			break;
		}
		default:
		{
			break;
		}
		}
	}

	void MenuInputManager::SetOpenedMenu(const RE::BSFixedString& a_menuName, const bool& a_isOpened)
	{
		// Update this manager's menu stack, accounted-for menu names, and handled menu count.
		// Then update the opened menu type and menu pointers.

		auto newHash = Hash(a_menuName);
		auto oldHash = Hash(menuName);

		// REMOVE when done debugging.
		logger::debug("[MIM] SetOpenedMenu: {}, hash {} ({}), old hash: {} ({}). Menu is opened: {}", a_menuName, newHash, a_menuName, oldHash, menuName, a_isOpened);

		auto stackCopy = menuNamesStack;
		uint32_t menuIndex = 0;
		while (!stackCopy.empty()) 
		{
			auto name = stackCopy.front();
			logger::debug("[MIM] SetOpenedMenu: Index {}: {}", menuIndex, name);
			menuIndex++;
			stackCopy.pop_front();
		}

		if (a_isOpened)
		{
			// Only push onto stack if menu is not already accounted for.
			if (!menuNamesHashSet.contains(newHash))
			{
				menuNamesStack.emplace_front(a_menuName);
			}

			// Add new menu name.
			if (!menuNamesHashSet.contains(newHash)) 
			{
				menuNamesHashSet.insert(newHash);
			}
		}
		else
		{
			// Closing menu may not always be the most recently opened menu.
			// Example: Previously opened LootMenu closes while ContainerMenu is atop the stack.
			if (!menuNamesStack.empty())
			{
				menuNamesStack.remove(a_menuName);
			}

			// Remove menu name.
			if (menuNamesHashSet.contains(newHash)) 
			{
				menuNamesHashSet.erase(newHash);
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
			logger::debug("[MIM] SetOpenedMenu: New menu on top of the stack: {}", menuName);
		}
		else
		{
			menuName = "";
			logger::debug("[MIM] SetOpenedMenu: Menu stack is now empty.");
		}

		newHash = Hash(menuName);
		// Only update menu type if a new menu is atop the stack.
		if (newHash != oldHash) 
		{
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[MIM] SetOpenedMenu: Lock: 0x{:X}.", hash);
			{
				std::unique_lock<std::mutex> lock(openedMenuMutex);
				logger::debug("[MIM] SetOpenedMenu: Lock obtained: 0x{:X}.", hash);
				newMenuAtopStack = true;
			}

			// Have to update menu type as soon as possible after the menu opens to avoid
			// setting the container menu's container to an invalid reference. The container
			// menu target ref handle seems to change to 0 not long after the menu opens (???).
			UpdateMenuType();
		}
	}

	void MenuInputManager::RefreshCyclableSpells() const
	{
		// Based on the selected form, update cyclable favorited forms of the same type.

		if (selectedForm)
		{
			const auto& em = glob.coopPlayers[managerMenuCID]->em;
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
	}

	void MenuInputManager::RefreshFavoritesMenuEquipState(bool&& a_updateCachedEquipState)
	{
		// Refresh displayed and/or cached equip state while in the FavoritesMenu.

		if (!favoritesMenu) 
		{
			logger::error("[MIM] ERR: RefreshFavoritesMenuEquipState: FavoritesMenu invalid");
			return;
		}

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			logger::critical("[MIM] RefreshFavoritesMenuEquipState: Menu player is invalid.");
			return;
		}

		// Set favorites list and set favorites' equip states for the co-op
		// player controlling the menu.
		const auto& favoritesList = favoritesMenu->favorites;
		const auto& em = glob.coopPlayers[managerMenuCID]->em;
		auto favoritedFormIDsSet = em->favoritedFormIDs;
		// Maintain P1's favorites list if transformed into a Vampire Lord.
		bool isVampireLord = Util::IsVampireLord(glob.coopPlayers[managerMenuCID]->coopActor.get());
		// Update cached equip states for all favorited items.
		if (a_updateCachedEquipState) 
		{
			const auto& equippedForms = em->equippedForms;
			// Clear before reconstructing below.
			favMenuIndexToEntryMap.clear();
			// Clear both because the favorites list and/or equip states of favorites may
			// have changed significantly since the favorites menu was last opened.
			favEntryEquipStates.clear();
			favEntryEquipStates = std::vector<EntryEquipState>(favoritesList.size(), EntryEquipState::kNone);
			auto& favItemsEquippedIndices = em->favItemsEquippedIndices;
			favItemsEquippedIndices.clear();

			// Iterate through the favorites list in order, set the equipped state
			// for each form equipped by the co-op player, and get the indices of all
			// equipped favorited forms.
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
				auto favForm = favoritesList[i].item;
				isFavoritedByCoopPlayer = isVampireLord || favoritedFormIDsSet.contains(favForm->formID);
				if (favForm && isFavoritedByCoopPlayer)
				{
					// Weapons
					if (auto asWeap = favForm->As<RE::TESObjectWEAP>(); asWeap)
					{
						// Check LH and RH for the same weapon.
						// LH
						if (equippedForms[!EquipIndex::kLeftHand] && equippedForms[!EquipIndex::kLeftHand] == favForm)
						{
							// Is two-handed weapon.
							if (asWeap && asWeap->equipSlot->flags.any(RE::BGSEquipSlot::Flag::kUseAllParents))
							{
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in both hands.", favForm->GetName());
								favEntryEquipStates[i] = EntryEquipState::kBothHands;
							}
							// One-handed.
							else
							{
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in left hand.", favForm->GetName());
								favEntryEquipStates[i] = EntryEquipState::kLH;
							}

							favItemsEquippedIndices.insert(i);
						}
						// RH
						else if (equippedForms[!EquipIndex::kRightHand] && equippedForms[!EquipIndex::kRightHand] == favForm)
						{
							// Is two-handed weapon.
							if (asWeap && asWeap->equipSlot->flags.any(RE::BGSEquipSlot::Flag::kUseAllParents))
							{
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in both hands.", favForm->GetName());
								favEntryEquipStates[i] = EntryEquipState::kBothHands;
							}
							// One-handed.
							else
							{
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in right hand.", favForm->GetName());
								favEntryEquipStates[i] = EntryEquipState::kRH;
							}

							favItemsEquippedIndices.insert(i);
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
							bool is2HSpell = lhObj && lhObj->As<RE::SpellItem>() && lhObj->As<RE::SpellItem>()->equipSlot == glob.bothHandsEquipSlot;
							// Set hand spells to copied spells if they are currently placeholder spells.
							if (is2HSpell)
							{
								auto copied2HSpell = em->GetCopiedMagic(PlaceholderMagicIndex::k2H);
								lhObj = lhObj == em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? copied2HSpell : lhObj;
								rhObj = rhObj == em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? copied2HSpell : rhObj;
							}
							else
							{
								if (lhObj) 
								{
									lhObj = lhObj == em->placeholderMagic[!PlaceholderMagicIndex::kLH] ? em->GetCopiedMagic(PlaceholderMagicIndex::kLH) : lhObj;
								}

								if (rhObj) 
								{
									rhObj = rhObj == em->placeholderMagic[!PlaceholderMagicIndex::kRH] ? em->GetCopiedMagic(PlaceholderMagicIndex::kRH) : rhObj;
								}
							}

							bool favEquippedLH = favForm == lhObj;
							bool favEquippedRH = favForm == rhObj;
							bool favEquippedBothH = (favEquippedLH && favEquippedRH) ||
													(favEquippedLH && lhObj && lhObj->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot) ||
													(favEquippedRH && rhObj && rhObj->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot);

							logger::debug("[MIM] RefreshFavoritesMenuEquipState: Spell {}, fav LH, fav RH, fav Both: {}, {}, {}",
								favForm->GetName(), favEquippedLH, favEquippedRH, favEquippedBothH);

							// Both hands spell.
							if (favEquippedBothH)
							{
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in both hands.", favForm->GetName());
								favEntryEquipStates[i] = EntryEquipState::kBothHands;
								favItemsEquippedIndices.insert(i);
							}
							// LH spell only
							else if (favEquippedLH)
							{
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in left hand.", favForm->GetName());
								favEntryEquipStates[i] = EntryEquipState::kLH;
								favItemsEquippedIndices.insert(i);
							}
							// RH spell only
							else if (favEquippedRH)
							{
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in right hand.", favForm->GetName());
								favEntryEquipStates[i] = EntryEquipState::kRH;
								favItemsEquippedIndices.insert(i);
							}
						}
						// Voice/power spells.
						else if (spellType == RE::MagicSystem::SpellType::kAbility ||
								 spellType == RE::MagicSystem::SpellType::kLesserPower ||
								 spellType == RE::MagicSystem::SpellType::kPower ||
								 spellType == RE::MagicSystem::SpellType::kVoicePower)
						{
							if (auto voiceForm = em->equippedForms[!EquipIndex::kVoice]; favForm == voiceForm)
							{
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in voice slot.", favForm->GetName());
								favEntryEquipStates[i] = EntryEquipState::kDefault;
								favItemsEquippedIndices.insert(i);
							}
						}

						// Check if quick slot spell is equipped.
						if (auto quickSlotSpell = em->quickSlotSpell; quickSlotSpell && quickSlotSpell == favForm)
						{
							logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in quick slot.",
								favForm->GetName());
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
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} with highest var index {} equipped in voice slot",
									asShout->GetName(), em->highestShoutVarIndex);
								favEntryEquipStates[i] = EntryEquipState::kDefault;
								favItemsEquippedIndices.insert(i);
							}
						}
					}
					// Armor
					else if (favForm->Is(RE::FormType::Armature, RE::FormType::Armor))
					{
						for (auto form : equippedForms)
						{
							if (form && form->Is(RE::FormType::Armature, RE::FormType::Armor) && form == favForm)
							{
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} (0x{:X}) armor equipped.",
									form->GetName(), form->formID);
								favEntryEquipStates[i] = EntryEquipState::kDefault;
								favItemsEquippedIndices.insert(i);
								break;
							}
						}
					}
					// Ammo
					else if (favForm->Is(RE::FormType::Ammo))
					{
						if (auto currentAmmo = equippedForms[!EquipIndex::kAmmo]; currentAmmo && currentAmmo == favForm)
						{
							logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} (0x{:X}) ammo equipped.",
								currentAmmo->GetName(), currentAmmo->formID);
							favEntryEquipStates[i] = EntryEquipState::kDefault;
							favItemsEquippedIndices.insert(i);
						}
					}
					// Quick slot items: consumables.
					else if (favForm->Is(RE::FormType::AlchemyItem, RE::FormType::Ingredient))
					{
						// Check if quick slot item is equipped.
						auto quickSlotItem = em->quickSlotItem;
						if (quickSlotItem && quickSlotItem == favForm)
						{
							logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} equipped in quick slot.", favForm->GetName());
							em->equippedQSItemIndex = i;
							itemStillEquipped = true;
						}
					}
					else
					{
						// Everything else -- do not treat as an equipable form.
						favEntryEquipStates[i] = EntryEquipState::kNone;
						favItemsEquippedIndices.erase(i);
					}
				}
				else
				{
					// Favorited form invalid or was not favorited by the co-op player.
					// Set equip state to 0.
					favEntryEquipStates[i] = EntryEquipState::kNone;
					favItemsEquippedIndices.erase(i);
				}
			}

			// Cached quick slot item or spell is no longer favorited and therefore not equipped, so clear as needed.
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

		// Can't update QS tag if task interface is invalid.
		auto taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface)
		{
			logger::error("[MIM] ERR: RefreshFavoritesMenuEquipState: Task interface invalid.");
			return;
		}

		// Update the Favorites Menu UI entries to reflect the updated equip state of all favorited items.
		taskInterface->AddUITask(
			[this, &em, favoritesList, favoritedFormIDsSet, isVampireLord]() {
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					if (auto favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); favoritesMenu && favoritesMenu.get())
					{
						if (auto view = favoritesMenu->uiMovie; view)
						{
							RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
							if (!menuCoopActorPtr)
							{
								logger::critical("[MIM] RefreshFavoritesMenuEquipState: UITask: menu player's ptr is invalid.");
								return;
							}

							double numEntries = view->GetVariableDouble("_root.MenuHolder.Menu_mc.itemList.entryList.length");
							logger::debug("[MIM] RefreshFavoritesMenuEquipState: Number of favorites entries before: {}, number of favorited forms for this player: {}.",
								numEntries, em->numFavoritedItems);
							RE::GFxValue entryList;
							view->CreateArray(std::addressof(entryList));
							view->GetVariable(std::addressof(entryList), "_root.MenuHolder.Menu_mc.itemList.entryList");
							for (uint32_t i = 0; i < numEntries; ++i)
							{
								RE::GFxValue entryIndex;
								RE::GFxValue entry;
								view->GetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", i, std::addressof(entry), 1);
								entry.GetMember("index", std::addressof(entryIndex));
								int32_t index = static_cast<int32_t>(entryIndex.GetNumber());

								// REMOVE when done debugging.
								logger::debug("[MIM] RefreshFavoritesMenuEquipState: Favorites list UI entry {} corresponds to favorites list index: {}", i, index);
								RE::TESForm* favoritedItem = index != -1 ? favoritesList[index].item : nullptr;
								if (!favoritedItem) 
								{
									logger::critical("[MIM] ERR: RefreshFavoritesMenuEquipState: No favorited item with index {}", index);
									continue;
								}

								// Get the form ID of the entry.
								RE::GFxValue entryFormId;
								entry.GetMember("formId", std::addressof(entryFormId));
								uint32_t formID = 0;
								// For SKYUI users (entries have member "formId").
								if (entryFormId.GetNumber() != 0)
								{
									formID = static_cast<uint32_t>(entryFormId.GetNumber());
								}
								else
								{
									// Vanilla UI.
									formID = favoritedItem ? favoritedItem->formID : 0;
								}

								// If transformed into a Vampire Lord, this player shares P1's favorites.
								if (isVampireLord || favoritedFormIDsSet.contains(formID))
								{
									// REMOVE when done debugging.
									logger::debug("[MIM] RefreshFavoritesMenuEquipState: {} was favorited by {}",
										favoritedItem->GetName(), menuCoopActorPtr->GetName());

									RE::GFxValue entryText;
									entry.GetMember("text", std::addressof(entryText));
									std::string entryStr = entryText.GetString();

									// Update item count to reflect the number of that item in the co-op player's inventory, not player 1's.
									// Ignore spells and shouts, which always have count 1.
									auto invCounts = menuCoopActorPtr->GetInventoryCounts();
									if (!favoritedItem->Is(RE::FormType::Spell, RE::FormType::Shout))
									{
										auto boundObj = favoritedItem->As<RE::TESBoundObject>();
										uint32_t count = invCounts.contains(boundObj) ? invCounts.at(boundObj) : 0;
										if (count > 1)
										{
											auto parenPos1 = entryStr.find("(");
											auto parenPos2 = entryStr.find(")");
											if (parenPos1 != std::string::npos && parenPos2 != std::string::npos)
											{
												entryStr = entryStr.substr(0, parenPos1) + "(" + std::to_string(count) + entryStr.substr(parenPos2);
											}
											else
											{
												logger::debug("[MIM] RefreshFavoritesMenuEquipState: No parens around count: {}", entryStr);
												entryStr += " (" + std::to_string(count) + ")";
											}

											logger::debug("[MIM] RefreshFavoritesMenuEquipState: Entry text is now '{}' after count update.", entryStr);
											entryText.SetString(entryStr);
											entry.SetMember("text", entryText);
										}
									}

									// Update equip state for the entry.
									// Normal items receive an update to the "caret" equipped icon,
									// while quick slot items have their entry text modified.
									const auto& equipStateNum = favEntryEquipStates[index];
									logger::debug("[MIM] RefreshFavoritesMenuEquipState: Favorites index {} item {} is getting its equip state set to {}",
										index, favoritedItem->GetName(), equipStateNum);
									RE::GFxValue equipState;
									equipState.SetNumber(static_cast<double>(equipStateNum));
									entry.SetMember("equipState", equipState);

									// Add quick slot item/spell tag.
									if (index == em->equippedQSItemIndex || index == em->equippedQSSpellIndex)
									{
										bool isConsumable = index == em->equippedQSItemIndex;
										if (entryStr.find("[*QS", 0) == std::string::npos)
										{
											entryStr = fmt::format("[*QS{}*] {}", isConsumable ? "I" : "S", entryStr);
											logger::debug("[MIM] RefreshFavoritesMenuEquipState: QS: Entry text is now: {}", entryStr);
											entryText.SetString(entryStr);
											entry.SetMember("text", entryText);
										}
									}

									// Set updated entry in list.
									view->SetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", i, std::addressof(entry), 1);
									// Insert (key = favorites list index, value = UI entry number) pairs into map.
									favMenuIndexToEntryMap.insert_or_assign(index, i);
								}
								else
								{
									logger::debug("[MIM] RefreshFavoritesMenuEquipState: Removing non-coop favorited entry: {}, {}.",
										favoritesList[index].item->GetName(), i);

									// Item was not favorited by the menu-controlling player, so remove it from the list.
									entryList.RemoveElement(i);
									--i;
									view->SetVariableArraySize("_root.MenuHolder.Menu_mc.itemList.entryList", --numEntries);
								}
							}

							// Update the favorites entry list.
							view->InvokeNoReturn("_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0);
						}
						else
						{
							logger::critical("[MIM] RefreshFavoritesMenuEquipState: ERR: Could not get view for Favorites Menu.");
						}
					}
				}
			});
	}


	void MenuInputManager::RefreshMagicMenuEquipState(bool&& a_updateCachedEquipState)
	{
		// Refresh displayed and/or cached equip state while in the MagicMenu.

		if (!magicMenu) 
		{
			logger::error("[MIM] RefreshMagicMenuEquipState: MagicMenu invalid.");
			return;
		}

		// Update cached equip states for each magic item.
		if (a_updateCachedEquipState) 
		{
			// Set list containing all magic forms if it is empty.
			if (magFormsList.empty())
			{
				SetMagicMenuFormsList();
			}

			auto numMagItems = magFormsList.size();
			// If still empty, return early.
			if (numMagItems == 0)
			{
				logger::error("[MIM] RefreshMagicMenuEquipState: No magic items in magic menu.");
				return;
			}

			// Clear out cached data before repopulating.
			magEntryEquipStates.clear();
			magEntryEquipStates = std::vector<EntryEquipState>(numMagItems, EntryEquipState::kNone);
			magFormEquippedIndices.clear();

			const auto& em = glob.coopPlayers[managerMenuCID]->em;
			// Get copied spells in place of equipped placeholder spells.
			auto lhObj = em->equippedForms[!EquipIndex::kLeftHand];
			auto rhObj = em->equippedForms[!EquipIndex::kRightHand];
			bool is2HSpell = lhObj && lhObj->As<RE::SpellItem>() && lhObj->As<RE::SpellItem>()->equipSlot == glob.bothHandsEquipSlot;
			if (is2HSpell)
			{
				auto copied2HSpell = em->GetCopiedMagic(PlaceholderMagicIndex::k2H);
				lhObj = lhObj == em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? copied2HSpell : lhObj;
				rhObj = rhObj == em->placeholderMagic[!PlaceholderMagicIndex::k2H] ? copied2HSpell : rhObj;
			}
			else
			{
				// Set hand spells to copied spells if they are currently placeholder spells.
				if (lhObj)
				{
					lhObj = lhObj == em->placeholderMagic[!PlaceholderMagicIndex::kLH] ?
							em->GetCopiedMagic(PlaceholderMagicIndex::kLH) : lhObj;
				}

				if (rhObj)
				{
					rhObj = rhObj == em->placeholderMagic[!PlaceholderMagicIndex::kRH] ?
							em->GetCopiedMagic(PlaceholderMagicIndex::kRH) : rhObj;
				}
			}

			auto voiceForm = em->equippedForms[!EquipIndex::kVoice];
			// Set voice spell to copied spell if it is currently a placeholder spell.
			if (voiceForm && voiceForm->Is(RE::FormType::Spell))
			{
				voiceForm = voiceForm == em->GetPlaceholderMagic(PlaceholderMagicIndex::kVoice) ?
							em->GetCopiedMagic(PlaceholderMagicIndex::kVoice) : voiceForm;
			}

			for (uint32_t i = 0; i < magFormsList.size(); ++i)
			{
				if (auto magForm = magFormsList[i]; magForm)
				{
					bool magEquippedLH = magForm == lhObj;
					bool magEquippedRH = magForm == rhObj;
					bool magEquippedBothH = (magEquippedLH && magEquippedRH) ||
											(magEquippedLH && lhObj && lhObj->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot) ||
											(magEquippedRH && rhObj && rhObj->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot);
					bool magEquippedVoice = magForm == voiceForm;

					// Both hands.
					if (magEquippedBothH)
					{
						logger::debug("[MIM] RefreshMagicMenuEquipState: {} equipped in both hands.", magForm->GetName());
						magEntryEquipStates[i] = EntryEquipState::kBothHands;
						magFormEquippedIndices.insert(i);
					}
					// LH
					else if (magEquippedLH)
					{
						logger::debug("[MIM] RefreshMagicMenuEquipState: {} equipped in left hand.", magForm->GetName());
						magEntryEquipStates[i] = EntryEquipState::kLH;
						magFormEquippedIndices.insert(i);
					}
					// RH
					else if (magEquippedRH)
					{
						logger::debug("[MIM] RefreshMagicMenuEquipState: {} equipped in right hand.", magForm->GetName());
						magEntryEquipStates[i] = EntryEquipState::kRH;
						magFormEquippedIndices.insert(i);
					}
					// Voice
					else if (magEquippedVoice)
					{
						logger::debug("[MIM] RefreshMagicMenuEquipState: {} equipped in voice slot.", magForm->GetName());
						magEntryEquipStates[i] = EntryEquipState::kDefault;
						magFormEquippedIndices.insert(i);
					}
					// No match or invalid
					else
					{
						magEntryEquipStates[i] = EntryEquipState::kNone;
						magFormEquippedIndices.erase(i);
					}
				}
			}
		}

		// Can't update Magic Menu entries if task interface is invalid.
		auto taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface)
		{
			logger::error("[MIM] ERR: RefreshMagicMenuEquipState: Task interface invalid.");
			return;
		}

		taskInterface->AddUITask(
			[this]() {
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					if (auto magicMenu = ui->GetMenu<RE::MagicMenu>(); magicMenu && magicMenu.get())
					{
						if (auto view = magicMenu->uiMovie; view && magicMenu->unk30)
						{
							auto magicItemList = reinterpret_cast<RE::ItemList*>(magicMenu->unk30);
							if (magicItemList)
							{
								auto& magicEntryList = magicItemList->entryList;
								RE::GFxValue numItemsGFx;
								magicEntryList.GetMember("length", std::addressof(numItemsGFx));
								double numItems = numItemsGFx.GetNumber();
								for (auto i = 0; i < numItems; ++i)
								{
									RE::GFxValue entry;
									magicEntryList.GetElement(i, std::addressof(entry));
									RE::GFxValue newEquipState;
									entry.GetMember("equipState", std::addressof(newEquipState));

									// Set cached equip state.
									newEquipState.SetNumber(static_cast<double>(magEntryEquipStates[i]));
									entry.SetMember("equipState", newEquipState);
									magicEntryList.SetElement(i, entry);
									magicItemList->view->SetVariable("entryList", magicEntryList);
								}
							}
						}
					}
				}
			});
	}

	void MenuInputManager::RefreshMenu() 
	{
		// Refresh the currently opened menu.

		if (containerMenu)
		{
			containerMenu->itemList->Update();
		}
		else if (barterMenu)
		{
			barterMenu->itemList->Update();
		}
		else if (favoritesMenu || magicMenu)
		{
			// Temporary solution to force the menu
			// to update the equip states of the listed items,
			// since equip requests change the menu-controlling
			// co-op player's equip state, not player 1's, and are
			// not reflected in the displayed menu automatically.
			auto userEvents = RE::UserEvents::GetSingleton();
			auto dpadLeftEvent = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(RE::INPUT_DEVICE::kGamepad, userEvents->left, DXSC_DPAD_LEFT, 1.0f, 0.0f));
			auto dpadRightEvent = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(RE::INPUT_DEVICE::kGamepad, userEvents->right, DXSC_DPAD_RIGHT, 1.0f, 0.0f));
			auto dpadLeftEvent2 = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(RE::INPUT_DEVICE::kGamepad, userEvents->left, DXSC_DPAD_LEFT, 0.0f, 1.0f));
			auto dpadRightEvent2 = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(RE::INPUT_DEVICE::kGamepad, userEvents->right, DXSC_DPAD_RIGHT, 0.0f, 1.0f));

			(*(dpadLeftEvent.get()))->AsButtonEvent()->pad24 = (*(dpadRightEvent.get()))->AsButtonEvent()->pad24 = 
			(*(dpadLeftEvent2.get()))->AsButtonEvent()->pad24 = (*(dpadRightEvent2.get()))->AsButtonEvent()->pad24 = 0xCA11;

			Util::SendInputEvent(dpadRightEvent);

			Util::SendInputEvent(dpadRightEvent2);

			Util::SendInputEvent(dpadLeftEvent);

			Util::SendInputEvent(dpadLeftEvent2);
		}

		// NOTE: Might not be necessary. Send update request.
		if (const auto ui = RE::UI::GetSingleton(); ui && !menuNamesStack.empty())
		{
			if (auto currentMenu = RE::UI::GetSingleton()->GetMenu(menuNamesStack.front()); currentMenu)
			{
				auto messageQueue = RE::UIMessageQueue::GetSingleton();
				if (messageQueue)
				{
					messageQueue->AddMessage(menuNamesStack.front(), RE::UI_MESSAGE_TYPE::kUpdate, nullptr);
				}
			}
		}

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			logger::critical("[MIM] ERR: RefreshMenu: Menu player is invalid.");
			return;
		}

		if (menuCoopActorPtr->GetInventoryChanges()->changed)
		{
			// Queue NiNode update for the player if their inventory changed.
			menuCoopActorPtr->Update3DModel();
		}
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

	bool MenuInputManager::SetEmulatedInputEventInfo(const uint32_t& a_xMask, MenuBindInfo& a_bindInfoOut)
	{
		// Set menu bind info based on the XInput mask and event name derived from the given XInputMask
		// in the context of one of the open menus.

		auto ui = RE::UI::GetSingleton();
		if (!ui)
		{
			logger::error("[MIM] ERR: SetEmulatedInputEventInfo: Could not get UI.");
			return false;
		}

		// Send gamepad input event.
		a_bindInfoOut.device = gamepadDevice;
		// Set to invalid ID code at first.
		a_bindInfoOut.idCode = 0xFF;
		if (glob.cdh->XIMASK_TO_GAMEMASK.contains(a_xMask))
		{
			a_bindInfoOut.idCode = glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask);
		}
		else
		{
			logger::error("[MIM] SetEmulatedInputEventInfo: ERR: no game input mask corresponds to an XInput mask of 0x{:X}.", a_xMask);
			return false;
		}

		// Search for a context with a valid event name for the gamepad id code.
		auto context = RE::UserEvents::INPUT_CONTEXT_ID::kNone;
		bool validEventNameFound = false;
		// Check current menu's context first.
		if (const auto& currentMenu = ui->GetMenu(menuName); currentMenu && *currentMenu->inputContext != RE::UserEvents::INPUT_CONTEXT_ID::kNone) 
		{
			context = *currentMenu->inputContext;
			logger::debug("[MIM] SetEmulatedInputEventInfo: Checking current menu {}'s context first ({}).", menuName, context);
			a_bindInfoOut.eventName = controlMap->GetUserEventName(a_bindInfoOut.idCode, gamepadDevice, context);
			validEventNameFound = Hash(a_bindInfoOut.eventName) != Hash(""sv);
		}

		// Fall back to menu mode context.
		if (!validEventNameFound) 
		{
			context = RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode;
			logger::debug("[MIM] SetEmulatedInputEventInfo: Checking menu mode context next ({}).", context);
			a_bindInfoOut.eventName = controlMap->GetUserEventName(a_bindInfoOut.idCode, gamepadDevice, context);
			validEventNameFound = Hash(a_bindInfoOut.eventName) != Hash(""sv);
		}

		// Fall back to checking the menu stack for a valid bind from each menu's context.
		if (!validEventNameFound)
		{
			for (const auto& menu : ui->menuStack)
			{
				// NOTE: Will remove when AE support is added.
				// AE added the kMarketplace context, which incremented kFavor, kTotal, and kNone by 1.
				// Was causing a crash with the latest version of TrueHUD, which has its menu context set to kNone (19), which
				// is not a valid context defined by the SE versions of CommonLib. Clamp here to ensure the context index is valid.
				context = static_cast<RE::UserEvents::INPUT_CONTEXT_ID>(min(!RE::UserEvents::INPUT_CONTEXT_ID::kNone, !(*menu->inputContext)));
				if (context != RE::UserEvents::INPUT_CONTEXT_ID::kNone)
				{
					logger::debug("[MIM] SetEmulatedInputEventInfo: Checking context from menu stack menu ({})", a_bindInfoOut.idCode, context);
					a_bindInfoOut.eventName = controlMap->GetUserEventName(a_bindInfoOut.idCode, gamepadDevice, context);
					if (Hash(a_bindInfoOut.eventName) != Hash(""sv))
					{
						validEventNameFound = true;
						break;
					}
				}
			}
		}

		// Fall-back to gameplay, which is a catch-all context with valid event names for all controller inputs.
		if (!validEventNameFound) 
		{
			context = RE::UserEvents::INPUT_CONTEXT_ID::kGameplay;
			a_bindInfoOut.eventName = controlMap->GetUserEventName(a_bindInfoOut.idCode, gamepadDevice, context);
			validEventNameFound = Hash(a_bindInfoOut.eventName) != Hash(""sv);
		}

		// Should not happen, but bail here if there is still no valid event name.
		if (!validEventNameFound) 
		{
			logger::critical("[MIM] ERR: SetEmulatedInputEventInfo: Could not get event name for XInput button mask 0x{:X} and current menu '{}'.", a_xMask, menuName);
			return false;
		}

		// Set valid context here.
		logger::debug("[MIM] SetEmulatedInputEventInfo: Chose context {}, event name {} for xMask 0x{:X}. Value, held time: {}, {}. Current event type: {}.",
			context, a_bindInfoOut.eventName, a_xMask,
			a_bindInfoOut.value,
			a_bindInfoOut.heldTimeSecs,
			currentMenuInputType);
		a_bindInfoOut.context = context;
		return true;
	}

	void MenuInputManager::SetMagicMenuFormsList()
	{
		// Set all selectable MagicMenu forms.

		magFormsList.clear();
		// Ensure there are no duplicate FIDs.
		std::set<RE::FormID> insertedFIDs{};
		if (auto magicItemList = reinterpret_cast<RE::ItemList*>(magicMenu->unk30); magicItemList)
		{
			auto numMagicItems = magicItemList->items.size();
			// Set magic forms list.
			RE::TESForm* formToAdd = nullptr;
			for (uint32_t i = 0; i < numMagicItems; ++i)
			{
				// Clear out our found form.
				formToAdd = nullptr;
				// Initial, more-accurate attempt.
				// SKYUI ONLY: Get the form ID of the entry and then use the data handler to get the form directly.
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
					if (formToAdd = Util::GetFormFromFID(formID); formToAdd)
					{
						magFormsList.push_back(formToAdd);
						continue;
					}
				}

				// Second attempt. Should be unnecessary unless SKYUI is not installed.
				// Match magic item name with known spells/shouts.
				// Will fail when multiple spells/shouts are known with the same name.
				if (auto magicItem = magicItemList->items[i]; magicItem)
				{
					auto chosenMagicItemName = magicItem->data.GetName();
					// Match spell name with one of player 1's learned spells.
					for (auto spellItem : glob.player1Actor->addedSpells)
					{
						if (strcmp(spellItem->GetName(), chosenMagicItemName) == 0 && !insertedFIDs.contains(spellItem->formID))
						{
							formToAdd = spellItem;
							break;
						}
					}

					// Match with spells that player 1 has by virtue of their actor base
					if (auto spellList = glob.player1Actor->GetActorBase()->actorEffects->spells; spellList)
					{
						uint32_t spellListSize = glob.player1Actor->GetActorBase()->actorEffects->numSpells;
						for (uint32_t i = 0; i < spellListSize; ++i)
						{
							auto spellItem = spellList[i];
							if (strcmp(spellItem->GetName(), chosenMagicItemName) == 0 && !insertedFIDs.contains(spellItem->formID))
							{
								formToAdd = spellItem;
								break;
							}
						}
					}

					// Match with shouts that player 1 has by virtue of their actor base.
					if (auto shoutList = glob.player1Actor->GetActorBase()->actorEffects->shouts; shoutList)
					{
						uint32_t shoutListSize = glob.player1Actor->GetActorBase()->actorEffects->numShouts;
						for (uint32_t i = 0; i < shoutListSize; ++i)
						{
							// Some unused shouts exist in player 1's actor base shouts list.
							// All have one character length names.
							if (shoutList[i] && strlen(shoutList[i]->GetName()) > 1 && 
								strcmp(shoutList[i]->GetName(), chosenMagicItemName) == 0 && 
								!insertedFIDs.contains(shoutList[i]->formID))
							{
								formToAdd = shoutList[i];
								break;
							}
						}
					}

					magFormsList.push_back(formToAdd);
				}
			}
		}
	}

	void MenuInputManager::SetMenuControlMap()
	{
		// Initialize the co-op player's base control map for the most recently opened menu.

		logger::debug("[MIM] SetMenuControlMap");

		// Used for ID code and event name lookups with P1's default binds.
		controlMap = RE::ControlMap::GetSingleton();
		// Save previous control map to copy the tap/hold/release states of all buttons over to the new control map.
		auto oldMenuControlMap = std::unordered_map<uint32_t, MenuBindInfo>(menuControlMap);
		// clang-format off
		menuControlMap = std::unordered_map<uint32_t, MenuBindInfo>(
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

		// Copy over old in-common linked menu events so that held buttons from the previously opened
		// menu do register as presses in the new menu when it opens.
		for (auto& [xMask, menuBindInfo] : oldMenuControlMap) 
		{
			if (menuControlMap.contains(xMask)) 
			{
				menuControlMap[xMask] = menuBindInfo;
			}
		}
	}

	void MenuInputManager::ToggleCoopPlayerMenuMode(const int32_t& a_controllerIDToSet, const int32_t& a_playerIDToSet)
	{
		// Toggle menu mode for the given player CID and PID.

		bool shouldEnter = a_controllerIDToSet != -1;

		// REMOVE when done debugging.
		logger::debug("[MIM] ToggleCoopPlayerMenuMode: Request to toggle menu control for {} to {} for CID {} (player ID {}), thread state before: {}",
			menuName, shouldEnter, a_controllerIDToSet, a_playerIDToSet, currentState);

		if (shouldEnter) 
		{
			// Ensure that menu controller is available before starting/resuming the manager.
			XINPUT_STATE buttonState;
			ZeroMemory(&buttonState, sizeof(buttonState));
			if (XInputGetState(a_controllerIDToSet, &buttonState) != ERROR_SUCCESS)
			{
				logger::critical("[MIM] ERR: ToggleCoopPlayerMenuMode: Got invalid menu controller ID ({}). Exiting.", a_controllerIDToSet);
				return;
			}

			// Set menu controlling player CID and PID.
			managerMenuCID = a_controllerIDToSet;
			if (a_playerIDToSet != -1) 
			{
				// If provided, such as when co-op session is inactive, set directly.
				managerMenuPlayerID = a_playerIDToSet;
			}
			else
			{
				// If not provided, get from player manager.
				managerMenuPlayerID = glob.coopPlayers[managerMenuCID]->playerID;
			}

			// Signal to run.
			RequestStateChange(ManagerState::kRunning);
		}
		else
		{
			// Signal to pause and clear out IDs.
			RequestStateChange(ManagerState::kPaused);
			managerMenuCID = -1;
			managerMenuPlayerID = 0;
		}

		logger::debug("[MIM] ToggleCoopPlayerMenuMode: Performed state change request. MIM is now set to {}.", shouldEnter ? "running" : "paused");
	}

	void MenuInputManager::UpdateFavoritedConsumableCount(RE::TESForm* a_selectedForm, uint32_t a_selectedIndex)
	{
		// Update the shown amount of the given consumable at the given menu index.
		
		// Can't update entry count if task interface is invalid.
		auto taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface)
		{
			logger::error("[MIM] ERR: UpdateFavoritedConsumableCount: Task interface invalid.");
			return;
		}

		taskInterface->AddUITask(
			[this, a_selectedForm, a_selectedIndex]() {
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					if (auto favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); favoritesMenu && favoritesMenu.get())
					{
						if (auto view = favoritesMenu->uiMovie; view)
						{
							RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
							if (!menuCoopActorPtr || !a_selectedForm)
							{
								logger::critical("[MIM] UpdateFavoritedConsumableCount: UITask: Menu player is invalid: {}, selected item is invalid: {}.",
									(bool)!menuCoopActorPtr, (bool)!a_selectedForm);
								return;
							}

							uint32_t selectedEntry = favMenuIndexToEntryMap.at(a_selectedIndex);
							auto invCounts = menuCoopActorPtr->GetInventoryCounts();
							auto boundObj = a_selectedForm->As<RE::TESBoundObject>();
							int32_t newCount = boundObj && invCounts.contains(boundObj) ? invCounts.at(boundObj) : -1;
							if (newCount >= 0)
							{
								RE::GFxValue entry;
								view->GetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", selectedEntry, std::addressof(entry), 1);

								RE::GFxValue entryText;
								entry.GetMember("text", std::addressof(entryText));
								std::string entryStr = entryText.GetString();
								auto parenPos1 = entryStr.find("(");
								auto parenPos2 = entryStr.find(")");
								if (parenPos1 != std::string::npos && parenPos2 != std::string::npos)
								{
									entryStr = entryStr.substr(0, parenPos1) + "(" + std::to_string(newCount) + entryStr.substr(parenPos2);
								}

								// Set the new entry text, place the new entry in the list, and update the entries list.
								entryText.SetString(entryStr);
								entry.SetMember("text", entryText);
								view->SetVariableArray("_root.MenuHolder.Menu_mc.itemList.entryList", selectedEntry, std::addressof(entry), 1);
								view->InvokeNoReturn("_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0);
							}
						}
					}
				}
			});
	}

	MenuBindInfo::MenuBindInfo() :
		idCode(0), device(RE::INPUT_DEVICE::kGamepad), eventName(""sv),
		context(RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode), eventType(MenuInputEventType::kReleasedNoEvent),
		value(0.0f), heldTimeSecs(0.0f), firstPressTP(SteadyClock::now())
	{ }
	
	MenuBindInfo::MenuBindInfo(RE::INPUT_DEVICE a_device, RE::BSFixedString a_eventName, RE::UserEvents::INPUT_CONTEXT_ID a_context) :
		device(a_device), eventName(a_eventName), context(a_context), eventType(MenuInputEventType::kReleasedNoEvent),
		idCode(RE::ControlMap::GetSingleton() ? RE::ControlMap::GetSingleton()->GetMappedKey(eventName, device, context) : 0xFF),
		value(0.0f), heldTimeSecs(0.0f), firstPressTP(SteadyClock::now())
	{ }

	void MenuOpeningActionRequestsManager::ClearAllRequests()
	{
		// Clear out all requests for all active players.
		for (const auto& p : glob.coopPlayers)
		{
			if (p->isActive) 
			{
				ClearRequests(p->controllerID);
			}
		}
	}

	void MenuOpeningActionRequestsManager::ClearRequests(const int32_t& a_controllerID)
	{
		// Clear all menu opening action requests for the given player CID.

		if (a_controllerID > -1 && a_controllerID < menuOpeningActionRequests.size())
		{
			logger::debug("[MIM] MenuOpeningActionRequestsManager: ClearRequests: P{}: clearing menu request queue. Had size {}.",
				a_controllerID + 1, menuOpeningActionRequests[a_controllerID].size());
			menuOpeningActionRequests[a_controllerID].clear();
		}
	}

	bool MenuOpeningActionRequestsManager::InsertRequest(const int32_t& a_controllerID, InputAction a_fromAction, SteadyClock::time_point a_timestamp, RE::BSFixedString a_reqMenuName, RE::ObjectRefHandle a_assocRefrHandle, bool a_isExtRequest)
	{
		// Insert a menu opening action request for the given CID, associated action, menu, and form. 
		// Tag with timestamp.

		if (a_controllerID > -1 && a_controllerID < menuOpeningActionRequests.size())
		{
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[MIM] MenuOpeningActionRequestsManager: InsertRequest: Try to lock: 0x{:X}.", hash);
			{
				std::unique_lock<std::mutex> lock(reqQueueMutexList[a_controllerID], std::try_to_lock);
				if (lock) 
				{
					// REMOVE
					logger::debug("[MIM] MenuOpeningActionRequestsManager: InsertRequest: Lock obtained: 0x{:X}.", hash);

					auto& reqQueue = menuOpeningActionRequests[a_controllerID];
					// Remove oldest request if currently full.
					if (reqQueue.size() == maxCachedRequests)
					{
						reqQueue.pop_back();
					}

					logger::debug("[MIM] MenuOpeningActionRequestsManager: InsertRequest: Adding menu opening action request for CID {}: input action: {}, menu name: {}, associated form: {}",
						a_controllerID,
						a_fromAction,
						Hash(a_reqMenuName) == Hash("") ? "NONE" : a_reqMenuName,
						Util::HandleIsValid(a_assocRefrHandle) ? a_assocRefrHandle.get()->GetName() : "NONE");
					reqQueue.emplace_front(a_fromAction, a_timestamp, a_reqMenuName, a_assocRefrHandle, a_isExtRequest);
					return true;
				}
				else
				{
					// REMOVE
					logger::debug("[MIM] MenuOpeningActionRequestsManager: InsertRequest: Failed to obtain lock: 0x{:X}.", hash);
					return false;
				}
			}
		}

		return false;
	}

	int32_t MenuOpeningActionRequestsManager::ResolveMenuControllerID(const RE::BSFixedString& a_menuName, bool&& a_modifyReqQueue)
	{
		// Get controller ID for player that should control the given menu when it opens/closes.
		// Optionally, modify the menu requests queue for this player to re-order or clear out requests during the resolution process.

		logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: Menu: {}, modify requests queue: {}.", a_menuName, a_modifyReqQueue);

		// Default to none.
		int32_t resolvedCID = -1;
		auto ui = RE::UI::GetSingleton();
		if (!ui) 
		{
			logger::error("[MIM] ERR: MenuOpeningActionRequestsManager: ResolveMenuControllerID: Could not get UI.");
			return resolvedCID;
		}

		// Must be a supported menu or the Lockpicking Menu, which is not handled by the MIM, but supports input
		// from co-op companion players via a special lockpicking task (multiple player inputs simultaneously).
		if ((glob.SUPPORTED_MENU_NAMES.contains(a_menuName) || a_menuName == RE::LockpickingMenu::MENU_NAME))
		{
			// First conditional check for dialogue menu control.
			// If the dialogue menu is open,
			// the player closest to the speaker is given control of the menu.
			// Overriden by valid requests below.
			if (Settings::bUninitializedDialogueWithClosestPlayer && Hash(a_menuName.c_str()) == Hash(RE::DialogueMenu::MENU_NAME))
			{
				if (auto menuTopicManager = RE::MenuTopicManager::GetSingleton(); menuTopicManager)
				{
					auto speakerPtr = Util::HandleIsValid(menuTopicManager->speaker) ? 
									  Util::GetRefrPtrFromHandle(menuTopicManager->speaker) : 
									  Util::GetRefrPtrFromHandle(menuTopicManager->lastSpeaker);
					if (speakerPtr) 
					{
						float closestDistToSpeaker = FLT_MAX;
						for (const auto& p : glob.coopPlayers)
						{
							if (p->isActive)
							{
								if (auto dist = speakerPtr->data.location.GetDistance(p->coopActor->data.location); dist < closestDistToSpeaker) 
								{
									resolvedCID = p->controllerID;
									closestDistToSpeaker = dist;
								}
							}
						}
					}
				}
			}

			float secsSinceChosenReq = FLT_MAX;
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive)
				{
					const auto& cid = p->controllerID;
					// Iterate through copy, even if not modifying the request queue.
					std::list<MenuOpeningActionRequests> reqQueue{ menuOpeningActionRequests[cid] };
					// Clear before adding back unfulfilled requests later.
					if (a_modifyReqQueue && !menuOpeningActionRequests[cid].empty()) 
					{
						ClearRequests(cid);
					}

					while (!reqQueue.empty()) 
					{
						// Oldest to newest.
						auto currentReq = reqQueue.back();
						float secsSinceReq = Util::GetElapsedSeconds(currentReq.timestamp);

						// REMOVE when done debugging.
						logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: Got request for {}. Menu: {}. Seconds since request inserted: {}. Seconds since chosen request inserted: {}. Queue size: {}.", 
							p->coopActor->GetName(), currentReq.reqMenuName, secsSinceReq, secsSinceChosenReq, reqQueue.size());

						// Want to choose the oldest valid request because, for example, if two players
						// activate the same NPC and a menu opens to interact with that NPC, the player
						// that activated the NPC first (oldest request) should control the menu.
						// Ignore requests that are older than 3 seconds.
						bool isMessageBoxMenu = Hash(a_menuName) == Hash(RE::MessageBoxMenu::MENU_NAME);
						// Ignore max 3 second request lifetime for the LootMenu.
						bool ignoreReqExpiration = Hash(a_menuName) == "LootMenu"_h;
						bool checkForOldestReq = { 
							!isMessageBoxMenu &&
							(ignoreReqExpiration || secsSinceReq < 3.0f) && 
							(secsSinceChosenReq == FLT_MAX || secsSinceReq > secsSinceChosenReq) 
						};
						// Alternatively, for menus that have no clearly defined trigger, check which player submitted the most recent request and give that player control.
						// Currently only for MessageBoxMenu.
						bool checkForNewestReq = isMessageBoxMenu && (secsSinceChosenReq == FLT_MAX || secsSinceReq < secsSinceChosenReq);
						// If the menu-associated refr the same as the request's one?
						bool isSameRefr = false;

						// NOTE: If an activator was activated and a menu opens, compare the menu's associated refr's name with the activated refr's name.
						// Can't figure out another way to check an activator's associated refr and compare to the menu's refr, and this will lead to instances
						// where the wrong player gains control of the menu if two players or more submit requests to activate objects of the same name at almost the same time.
						// Will continue looking for a direct way to link activators to references.
						if (checkForOldestReq) 
						{
							// Insert for now. Will remove later if fulfilled.
							if (a_modifyReqQueue) 
							{
								menuOpeningActionRequests[cid].emplace_front(currentReq);
							}

							// Check validity of external requests first (ex. from a script). Only have to compare menu names.
							if (bool validExtRequest = currentReq.isExtRequest && Hash(currentReq.reqMenuName) == Hash(a_menuName); validExtRequest) 
							{
								secsSinceChosenReq = secsSinceReq;
								resolvedCID = p->controllerID;
								logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: External request: {} is in control of {}.",
									p->coopActor->GetName(), a_menuName);
								// Remove the current fulfilled request.
								if (a_modifyReqQueue)
								{
									menuOpeningActionRequests[cid].pop_front();
								}

								reqQueue.pop_back();
								continue;
							}

							switch (Hash(a_menuName.c_str(), a_menuName.size()))
							{
							case Hash(RE::BarterMenu::MENU_NAME.data(), RE::BarterMenu::MENU_NAME.size()):
							{
								// BarterMenu must be open, and request queued by activation.
								auto barterMenu = ui->GetMenu<RE::BarterMenu>();
								if (!barterMenu || currentReq.fromAction != InputAction::kActivate)
								{
									break;
								}

								if (auto assocRefrPtr = Util::GetRefrPtrFromHandle(currentReq.assocRefrHandle); assocRefrPtr && assocRefrPtr->GetObjectReference())
								{
									RE::TESObjectREFRPtr refrPtr;
									RE::TESObjectREFR::LookupByHandle(barterMenu->GetTargetRefHandle(), refrPtr);
									if (refrPtr)
									{
										isSameRefr = 
										{
											(assocRefrPtr == refrPtr) ||
											(assocRefrPtr->GetObjectReference()->Is(RE::FormType::Activator, RE::FormType::TalkingActivator) &&
											 Hash(assocRefrPtr->GetName()) == Hash(refrPtr->GetName()))
										};
										if (isSameRefr)
										{
											secsSinceChosenReq = secsSinceReq;
											resolvedCID = p->controllerID;
											logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: BarterMenu: {} is in control of bartering with {}.",
												p->coopActor->GetName(), assocRefrPtr->GetName());
										}
									}
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

								if (auto assocRefrPtr = Util::GetRefrPtrFromHandle(currentReq.assocRefrHandle); assocRefrPtr && assocRefrPtr->GetObjectReference())
								{
									if (RE::TESObjectREFR* refrPtr = bookMenu->GetTargetReference(); refrPtr)
									{
										isSameRefr = 
										{
											(assocRefrPtr.get() == refrPtr) ||
											(assocRefrPtr->GetObjectReference()->Is(RE::FormType::Activator, RE::FormType::TalkingActivator) &&
											 Hash(assocRefrPtr->GetName()) == Hash(refrPtr->GetName()))
										};
										if (isSameRefr)
										{
											secsSinceChosenReq = secsSinceReq;
											resolvedCID = p->controllerID;
											logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: BookMenu: {} is in control of reading {}.",
												p->coopActor->GetName(), assocRefrPtr->GetName());
										}
									}
								}

								break;
							}
							case Hash(RE::ContainerMenu::MENU_NAME.data(), RE::ContainerMenu::MENU_NAME.size()):
							{
								// ContainerMenu must be open.
								auto containerMenu = ui->GetMenu<RE::ContainerMenu>();
								if (!containerMenu)
								{
									break;
								}

								// Get refr associated with the ContainerMenu.
								auto refrHandle = RE::ContainerMenu::GetTargetRefHandle();
								RE::TESObjectREFRPtr refrPtr;
								bool succ = RE::LookupReferenceByHandle(RE::ContainerMenu::GetTargetRefHandle(), refrPtr);
								// Is player inventory if the associated refr is the requesting player.
								bool isReqPlayersInventory = succ && containerMenu->GetContainerMode() == RE::ContainerMenu::ContainerMode::kNPCMode && refrPtr && refrPtr == p->coopActor;
								// Container is this player's inventory and the player pressed their inventory bind or are requesting to open a container.
								if (isReqPlayersInventory && (currentReq.fromAction == InputAction::kInventory || Hash(currentReq.reqMenuName) == Hash(RE::ContainerMenu::MENU_NAME)))
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: ContainerMenu: {} is in control of their inventory.", p->coopActor->GetName());
								}
								else if (currentReq.fromAction == InputAction::kActivate)
								{
									if (auto assocRefrPtr = Util::GetRefrPtrFromHandle(currentReq.assocRefrHandle); assocRefrPtr && assocRefrPtr->GetObjectReference())
									{
										if (refrPtr)
										{
											// Check if the associated refrs match or if the extra ash pile refr matches the request's associated refr.
											auto extraAshPile = assocRefrPtr->extraList.GetAshPileRef();
											isSameRefr = 
											{
												(assocRefrPtr == refrPtr) ||
												(Util::GetRefrPtrFromHandle(extraAshPile) == refrPtr) ||
												(assocRefrPtr->GetObjectReference()->Is(RE::FormType::Activator, RE::FormType::TalkingActivator) &&
												 Hash(assocRefrPtr->GetName()) == Hash(refrPtr->GetName()))
											};
											if (isSameRefr)
											{
												secsSinceChosenReq = secsSinceReq;
												resolvedCID = p->controllerID;
												logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: ContainerMenu: {} is in control of {}'s container menu.",
													p->coopActor->GetName(), assocRefrPtr->GetName());
											}
										}
									}
								}

								break;
							}
							case Hash(RE::CraftingMenu::MENU_NAME.data(), RE::CraftingMenu::MENU_NAME.size()):
							{
								// Request must be queued by activation.
								if (currentReq.fromAction != InputAction::kActivate)
								{
									break;
								}

								if (auto assocRefrPtr = Util::GetRefrPtrFromHandle(currentReq.assocRefrHandle); assocRefrPtr && assocRefrPtr->GetObjectReference())
								{
									// TODO:
									// Ugh. Would like to match the type of submenu furniture to the associated furniture refr's type,
									// but can't figure out a way to get the crafting menu's submenu furniture without already having P1's inventory data loaded into the menu.
									// Need the menu CID to figure out which player's inventory to copy to P1 and need the sub menu furniture to get the menu CID, but
									// the copied inventory does not display if done after the submenu data is set, and "Reshow" messages fail.
									//
									// So, for now, just check if any furniture with workbench data was activated and choose the oldest of these requests
									// to determine which player gets control over the crafting menu.
									// Will cause issues if multiple players activate menu-triggering furniture at almost the exact same time,
									// but the tradeoff is worth it, in my opinion, since the inventory copying occurs without a hitch afterward, and
									// co-op companion players can use the crafting menu to adjust their own gear.
									if (auto asFurniture = assocRefrPtr->GetObjectReference()->As<RE::TESFurniture>(); asFurniture && *asFurniture->workBenchData.benchType != RE::TESFurniture::WorkBenchData::BenchType::kNone)
									{
										secsSinceChosenReq = secsSinceReq;
										resolvedCID = p->controllerID;
										logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: CraftingMenu: {} is in control of crafting menu by activating {} with workbench data type {}.",
											p->coopActor->GetName(), assocRefrPtr->GetName(), *asFurniture->workBenchData.benchType);
									}
								}

								break;
							}
							case Hash(RE::DialogueMenu::MENU_NAME.data(), RE::DialogueMenu::MENU_NAME.size()):
							{
								// Available menu topic manager and request must be queued by activation.
								auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
								if (!menuTopicManager || currentReq.fromAction != InputAction::kActivate)
								{
									break;
								}

								if (auto assocRefrPtr = Util::GetRefrPtrFromHandle(currentReq.assocRefrHandle); assocRefrPtr && assocRefrPtr->GetObjectReference())
								{
									auto speakerPtr = Util::GetRefrPtrFromHandle(menuTopicManager->speaker);
									auto lastSpeakerPtr = Util::GetRefrPtrFromHandle(menuTopicManager->lastSpeaker);
									isSameRefr = 
									{
										(speakerPtr && speakerPtr == assocRefrPtr) ||
										(lastSpeakerPtr && lastSpeakerPtr == assocRefrPtr) ||
										(assocRefrPtr->GetObjectReference()->Is(RE::FormType::Activator, RE::FormType::TalkingActivator) &&
										 speakerPtr && Hash(assocRefrPtr->GetName()) == Hash(speakerPtr->GetName())) ||
										(assocRefrPtr->GetObjectReference()->Is(RE::FormType::Activator, RE::FormType::TalkingActivator) &&
										 lastSpeakerPtr && Hash(assocRefrPtr->GetName()) == Hash(lastSpeakerPtr->GetName()))
									};
									if (isSameRefr)
									{
										secsSinceChosenReq = secsSinceReq;
										resolvedCID = p->controllerID;
										logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: DialogueMenu: {} is in control of dialogue with {}.",
											p->coopActor->GetName(), speakerPtr ? speakerPtr->GetName() : lastSpeakerPtr ? lastSpeakerPtr->GetName() :
																															 "NONE");
									}
								}

								break;
							}
							case Hash(RE::FavoritesMenu::MENU_NAME.data(), RE::FavoritesMenu::MENU_NAME.size()):
							{
								// Wants to access the FavoritesMenu.
								if (currentReq.fromAction == InputAction::kFavorites || Hash(currentReq.reqMenuName) == Hash(RE::FavoritesMenu::MENU_NAME))
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: FavoritesMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case Hash(RE::GiftMenu::MENU_NAME.data(), RE::GiftMenu::MENU_NAME.size()):
							{
								// Wants to trade with another player.
								if (currentReq.fromAction == InputAction::kTradeWithPlayer)
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: GiftMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case Hash(RE::JournalMenu::MENU_NAME.data(), RE::JournalMenu::MENU_NAME.size()):
							{
								// Wants to pause the game (open JournalMenu).
								if (currentReq.fromAction == InputAction::kPause || Hash(currentReq.reqMenuName) == Hash(RE::JournalMenu::MENU_NAME))
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: JournalMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case Hash(RE::InventoryMenu::MENU_NAME.data(), RE::InventoryMenu::MENU_NAME.size()):
							{
								// Wants to open P1's inventory directly or from the TweenMenu.
								if (currentReq.fromAction == InputAction::kInventory || currentReq.fromAction == InputAction::kTweenMenu)
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: InventoryMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case Hash(RE::LevelUpMenu::MENU_NAME.data(), RE::LevelUpMenu::MENU_NAME.size()):
							{
								// Wants to open the LevelUpMenu through the StatsMenu or TweenMenu.
								if (currentReq.fromAction == InputAction::kStatsMenu || currentReq.fromAction == InputAction::kTweenMenu)
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: LevelUpMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case Hash(RE::LockpickingMenu::MENU_NAME.data(), RE::LockpickingMenu::MENU_NAME.size()):
							{
								// LockpickingMenu must be open and request queued by activation.
								auto lockpickingMenu = ui->GetMenu<RE::LockpickingMenu>(); 
								if (!lockpickingMenu || currentReq.fromAction != InputAction::kActivate)
								{
									break;
								}

								if (auto assocRefrPtr = Util::GetRefrPtrFromHandle(currentReq.assocRefrHandle); assocRefrPtr && assocRefrPtr->GetObjectReference())
								{
									if (auto refrPtr = lockpickingMenu->GetTargetReference(); refrPtr)
									{
										isSameRefr = 
										{
											(assocRefrPtr.get() == refrPtr) || 
											(assocRefrPtr->GetObjectReference()->Is(RE::FormType::Activator, RE::FormType::TalkingActivator) && 
											 Hash(assocRefrPtr->GetName()) == Hash(refrPtr->GetName())) 
										};
										if (isSameRefr)
										{
											secsSinceChosenReq = secsSinceReq;
											resolvedCID = p->controllerID;
											logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: LockpickingMenu: {} is in control of unlocking {}.",
												p->coopActor->GetName(), assocRefrPtr->GetName());
										}
									}
								}

								break;
							}
							case Hash(RE::MagicMenu::MENU_NAME.data(), RE::MagicMenu::MENU_NAME.size()):
							{
								// Wants to open the MagicMenu.
								if (currentReq.fromAction == InputAction::kMagicMenu || Hash(currentReq.reqMenuName) == Hash(RE::MagicMenu::MENU_NAME))
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: MagicMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case Hash(RE::MapMenu::MENU_NAME.data(), RE::MapMenu::MENU_NAME.size()):
							{
								// Wants to open the MapMenu.
								if (currentReq.fromAction == InputAction::kMapMenu || Hash(currentReq.reqMenuName) == Hash(RE::MapMenu::MENU_NAME))
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: MapMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case Hash(RE::SleepWaitMenu::MENU_NAME.data(), RE::SleepWaitMenu::MENU_NAME.size()):
							{
								// Wants to open the WaitMenu.
								if (currentReq.fromAction == InputAction::kWaitMenu || Hash(currentReq.reqMenuName) == Hash(RE::SleepWaitMenu::MENU_NAME))
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: WaitMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case Hash(RE::StatsMenu::MENU_NAME.data(), RE::StatsMenu::MENU_NAME.size()):
							{
								// Wants to open the StatsMenu directly or through the TweenMenu.
								if (currentReq.fromAction == InputAction::kStatsMenu || currentReq.fromAction == InputAction::kTweenMenu)
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: StatsMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case Hash(RE::TrainingMenu::MENU_NAME.data(), RE::TrainingMenu::MENU_NAME.size()):
							{
								// TrainingMenu must be open and request queued by activation.
								auto trainingMenu = ui->GetMenu<RE::TrainingMenu>();
								if (!trainingMenu || currentReq.fromAction != InputAction::kActivate)
								{
									break;
								}

								if (auto assocRefrPtr = Util::GetRefrPtrFromHandle(currentReq.assocRefrHandle); assocRefrPtr && assocRefrPtr->GetObjectReference())
								{
									auto trainer = trainingMenu->trainer;
									isSameRefr = 
									{ 
										(assocRefrPtr.get() == trainer) || 
										(assocRefrPtr->GetObjectReference()->Is(RE::FormType::Activator, RE::FormType::TalkingActivator) && 
										 trainer && Hash(assocRefrPtr->GetName()) == Hash(trainer->GetName())) 
									};
									if (isSameRefr)
									{
										secsSinceChosenReq = secsSinceReq;
										resolvedCID = p->controllerID;
										logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: TrainingMenu: {} is receiving training from {}.",
											p->coopActor->GetName(), assocRefrPtr->GetName());
									}
								}

								break;
							}
							case Hash(RE::TweenMenu::MENU_NAME.data(), RE::TweenMenu::MENU_NAME.size()):
							{
								// Wants to open the TweenMenu.
								if (currentReq.fromAction == InputAction::kTweenMenu || Hash(currentReq.reqMenuName) == Hash(RE::TweenMenu::MENU_NAME))
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: TweenMenu: {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case ("CustomMenu"_h):
							{
								// Could be any menu triggered by script using SKSE's CustomMenu.
								// Support all actions that trigger UIExtensions menus here.
								// For the most part, however, the mod's scripts directly set the requesting menu control CID,
								// which bypasses queued request checks done here. Serves more as a failsafe.
								if (Hash(currentReq.reqMenuName) == "CustomMenu"_h ||
									(currentReq.fromAction == InputAction::kCoopDebugMenu ||
									currentReq.fromAction == InputAction::kCoopIdlesMenu ||
									currentReq.fromAction == InputAction::kCoopMiniGamesMenu ||
									currentReq.fromAction == InputAction::kCoopSummoningMenu ||
									currentReq.fromAction == InputAction::kStatsMenu ||
									currentReq.fromAction == InputAction::kTeleportToPlayer || 
									currentReq.fromAction == InputAction::kTradeWithPlayer))
								{
									secsSinceChosenReq = secsSinceReq;
									resolvedCID = p->controllerID;
									logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: CustomMenu (UIExtensions): {} is in control of menu.", p->coopActor->GetName());
								}

								break;
							}
							case ("LootMenu"_h):
							{
								// Crosshair pick data valid and request queued by crosshair movement.
								auto crosshairPickData = RE::CrosshairPickData::GetSingleton(); 
								if (!crosshairPickData || currentReq.fromAction != InputAction::kMoveCrosshair || Hash(currentReq.reqMenuName) != "LootMenu"_h)
								{
									break;
								}
								
								if (auto assocRefrPtr = Util::GetRefrPtrFromHandle(currentReq.assocRefrHandle); assocRefrPtr && assocRefrPtr->GetObjectReference())
								{
									// Get the container to display with the LootMenu.
									if (auto reqContainerRefrPtr = Util::GetRefrPtrFromHandle(glob.reqQuickLootContainerHandle); reqContainerRefrPtr)
									{
										// Compare associated refr/linked ash pile refr to crosshair-selected form stored in pad.
										auto ashPileRefPtr = Util::GetRefrPtrFromHandle(assocRefrPtr->extraList.GetAshPileRef());
										isSameRefr = 
										{ 
											(assocRefrPtr == reqContainerRefrPtr) ||
											(ashPileRefPtr && ashPileRefPtr == reqContainerRefrPtr) ||
											(assocRefrPtr->GetObjectReference()->Is(RE::FormType::Activator, RE::FormType::TalkingActivator) && 
											 Hash(assocRefrPtr->GetName()) == Hash(reqContainerRefrPtr->GetName())) 
										};
										if (isSameRefr)
										{
											secsSinceChosenReq = secsSinceReq;
											resolvedCID = p->controllerID;
											logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: LootMenu: {} is in control of {}'s quick loot menu.",
												p->coopActor->GetName(), assocRefrPtr->GetName());

											// Store info about which player sent the last crosshair event.
											glob.quickLootReqCID = p->controllerID;
										}
									}
								}

								break;
							}
							default:
							{
								logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: Defaulting to P1 control of {}.", a_menuName);
								break;
							}
							}

							// Remove since this request was fulfilled.
							if (a_modifyReqQueue && resolvedCID == p->controllerID) 
							{
								menuOpeningActionRequests[cid].pop_front();
							}
						}
						else if (checkForNewestReq)
						{
							// Direct requests can considered up to 5 seconds after enqueueing.
							bool directlyRequested = Hash(currentReq.reqMenuName) == Hash(a_menuName) && secsSinceReq < 5.0f;
							// Can be triggered by a variety of things, so if no direct request was made, choose the player that most recently activated an object.
							// Shorter maximum request lifetime of 1 second here, since more often than not, P1 should gain control of the menu.
							bool throughActivation = currentReq.fromAction == InputAction::kActivate && secsSinceReq < 2.0f && secsSinceReq < secsSinceChosenReq;
							if (directlyRequested || throughActivation) 
							{
								// Re-insert and do not remove, since multiple queued message box menus tend to open in quick succession,
								// and we want the same player to retain control over all menus queued to open.
								if (a_modifyReqQueue)
								{
									menuOpeningActionRequests[cid].emplace_front(currentReq);
								}

								// Update seconds since most recent request.
								secsSinceChosenReq = secsSinceReq;
								resolvedCID = p->controllerID;
								logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: MessageBoxMenu: {} is in control of menu by {}.", 
									p->coopActor->GetName(), directlyRequested ? "direct request" : "activation");

								// No need to check other requests once a direct request is found.
								if (directlyRequested)
								{
									break;
								}
							}
						}

						// Move on to the next request.
						reqQueue.pop_back();
					}

					logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: Active request queue size is now {} for {} after processing.", menuOpeningActionRequests[cid].size(), p->coopActor->GetName());
				}
			}
		}

		logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: Resolved CID from requests: {}", resolvedCID);
		// If there are no valid requests to open/close the current supported menu,
		// give control to the last player who controlled open menus.
		if (resolvedCID == -1 && GlobalCoopData::SUPPORTED_MENU_NAMES.contains(a_menuName)) 
		{
			logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: No valid requests to open supported menu {}, set to last menu CID: {}", a_menuName, glob.prevMenuCID);
			resolvedCID = glob.prevMenuCID;
		}

		logger::debug("[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: Final resolved CID: {}, for menu {}.",
			resolvedCID, a_menuName);
		return resolvedCID;
	}
}

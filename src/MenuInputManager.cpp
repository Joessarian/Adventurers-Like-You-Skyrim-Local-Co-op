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
		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
		// Opened menu info.
		openedMenuType = SupportedMenu::kDefault;
		menuName = "";

		// Ints
		managedCoopMenusCount = 0;
		// Bools
		isCoopInventory = false;
		placeholderMagicChanged = false;
		shouldRefreshMenu = false;
		spellFavoriteStatusChanged = false;

		// Player menu control outline overlay alpha value.
		pmcFadeInterpData = std::make_unique<TwoWayInterpData>();
		pmcFadeInterpData->SetInterpInterval(1.0f, true);
		pmcFadeInterpData->SetInterpInterval(1.0f, false);

		// Clear maps, sets, vectors.
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
			RefreshData();

			{
				std::unique_lock<std::mutex> lock(openedMenuMutex, std::try_to_lock);
				if (lock)
				{
					SPDLOG_DEBUG
					(
						"[MIM] MainTask: Lock acquired and data updated (0x{:X}). "
						"Setting new menu opened flag to false.", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
					newMenuAtopStack = false;
				}
				else
				{
					SPDLOG_DEBUG
					(
						"[MIM] MainTask: Could not acquire lock after updating data (0x{:X}). "
						"Better luck next time.", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
				}
			}
		}

		// Update controller state and event type to handle, if any.
		CheckControllerInput();

		bool delayedReq = 
		(
			delayedEquipStateRefresh && 
			Util::GetElapsedSeconds(lastEquipStateRefreshReqTP) > 1.0f
		);
		// Update equip state when signalled to externally or when there is a delayed request.
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
			else if (openedMenuType == SupportedMenu::kFavorites)
			{
				RefreshFavoritesMenuEquipState(true);
			}

			if (equipEventRefreshReq) 
			{
				{
					std::unique_lock<std::mutex> lock(equipEventMutex, std::try_to_lock);
					if (lock)
					{
						SPDLOG_DEBUG
						(
							"[MIM] MainTask: Lock acquired and data updated (0x{:X}). "
							"Resetting refresh equip state flags from {}, {} to false.",
							std::hash<std::jthread::id>()(std::this_thread::get_id()), 
							equipEventRefreshReq, 
							delayedEquipStateRefresh
						);

						// Equip state refresh request fired before delayed equip refresh request, 
						// so we can clear the delayed one.
						equipEventRefreshReq = false;
						shouldRefreshMenu = true;
					}
					else
					{
						SPDLOG_DEBUG
						(
							"[MIM] MainTask: Could not acquire lock after updating data (0x{:X}). "
							"Better luck next time.",
							std::hash<std::jthread::id>()(std::this_thread::get_id())
						);
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
		SPDLOG_DEBUG("[MIM] PrePauseTask.");
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

		if (managerMenuCID == -1) 
		{
			SPDLOG_DEBUG("[MIM] ERR: RefreshData: Got invalid controller ID (-1).");
			return;
		}

		// Get companion player's handle if in co-op.
		if (glob.coopSessionActive)
		{
			menuCoopActorHandle = glob.coopPlayers[managerMenuCID]->coopActor->GetHandle();
		}

		// Reset general menu data.
		currentMenuInputEventType = MenuInputEventType::kReleasedNoEvent;
		reqEquipIndex = EquipIndex::kRightHand;
		delayedEquipStateRefresh = false;
		equipEventRefreshReq = false;
		isCoopInventory = false;
		placeholderMagicChanged = false;
		shouldRefreshMenu = false;
		spellFavoriteStatusChanged = false;
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
			// If the container is the co-op companion player themselves, 
			// they are accessing their inventory.
			isCoopInventory =
			{ 
				containerMenu->GetContainerMode() == RE::ContainerMenu::ContainerMode::kNPCMode && 
				Util::HandleIsValid(menuContainerHandle) && 
				menuContainerHandle == menuCoopActorHandle
			};
		}
		else if (magicMenu)
		{
			// Set the initial magic item equip states.
			InitMagicMenuEquippedStates();
			// Refresh menu after initializing equip states/favorited indices.
			RefreshMenu();
		}
		else if (favoritesMenu)
		{
			// Set initial equip states for favorited items.
			InitFavoritesEntries();
			// Refresh menu after initializing equip states/favorited indices.
			RefreshMenu();
		}

		// Set controlmap initial input states so that inputs already held as the menu opens
		// do not trigger any input events until they are released and pressed again.
		XINPUT_STATE buttonState{ };
		ZeroMemory(&buttonState, sizeof(buttonState));
		// First, check for input presses as given by the menu control map above
		if (XInputGetState(managerMenuCID, &buttonState) == ERROR_SUCCESS)
		{
			for (auto iter = menuControlMap.begin(); iter != menuControlMap.end(); ++iter)
			{
				if ((buttonState.Gamepad.wButtons & iter->first) == 0)
				{
					continue;
				}

				// Set to emulated input initially to prevent previous event type from triggering.
				menuControlMap[iter->first].eventType = MenuInputEventType::kEmulateInput;
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
		auto err = XInputGetState(managerMenuCID, &buttonState);
		if (managerMenuCID == -1 || err != ERROR_SUCCESS)
		{
			// Leave error message before returning.
			if (err != ERROR_SUCCESS && managerMenuCID != -1)
			{
				SPDLOG_DEBUG
				(
					"[MIM] ERR: ShouldSelfPause: Could not get XINPUT state for controller ID {}. "
					"Pausing menu input manager.", 
					managerMenuCID
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
		// Container is not a companion player's inventory.
		if (!GlobalCoopData::IsCoopPlayer(containerRefr)) 
		{
			return currentState;
		}
		
		// The container is a co-op companion's inventory and the tab has been switched
		// to display P1's inventory, so we pause here.
		GlobalCoopData::SetMenuCIDs(glob.player1CID);
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
		RE::NiPointer<RE::TESObjectREFR> containerRefr;
		RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle(), containerRefr);
		auto pIndex = GlobalCoopData::GetCoopPlayerIndex(containerRefr); 
		// The container is not a companion player's inventory.
		if (pIndex == -1) 
		{
			return currentState;
		}
		
		// The container is a companion player's inventory, so we should resume here
		// after giving the player control of menus.
		managerMenuCID = pIndex;
		managerMenuPlayerID = glob.coopPlayers[managerMenuCID]->playerID;
		GlobalCoopData::SetMenuCIDs(managerMenuCID);
		return ManagerState::kRunning;
	}

	void MenuInputManager::CheckControllerInput()
	{
		// Update controller input state and set menu event type to handle.
		
		if (managerMenuCID < 0 || managerMenuCID >= ALYSLC_MAX_PLAYER_COUNT) 
		{
			return;
		}

		auto& paInfo = glob.paInfoHolder;
		XINPUT_STATE buttonState{ };
		ZeroMemory(&buttonState, sizeof(buttonState));
		auto err = XInputGetState(managerMenuCID, &buttonState);
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
					glob.cdh->GetAnalogStickState(managerMenuCID, xMask == XMASK_LS).normMag > 0.0f
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
					
				// Update menu event to send if just pressed.
				if (justPressed) 
				{
					// Default to no event.
					currentMenuInputEventType = MenuInputEventType::kReleasedNoEvent;
					if (handleButtonPress)
					{
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
						case SupportedMenu::kFavorites:
						{
							ProcessFavoritesMenuButtonInput(xMask);
							break;
						}
						case SupportedMenu::kGift:
						{
							ProcessGiftMenuButtonInput(xMask);
							break;
						}
						case SupportedMenu::kInventory:
						{
							ProcessInventoryMenuButtonInput(xMask);
							break;
						}
						case SupportedMenu::kLoot:
						{
							ProcessLootMenuButtonInput(xMask);
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
					
				// Special case (on hold):
				// Preview the hotkey to set for the selected Favorites Menu entry.
				if (openedMenuType == SupportedMenu::kFavorites && 
					xMask == XINPUT_GAMEPAD_RIGHT_THUMB)
				{
					HotkeyFavoritedForm(false);
				}

				// If unable to set bind info, set event type to none.
				if (!SetEmulatedInputEventInfo(xMask, bindInfo))
				{
					currentMenuInputEventType = MenuInputEventType::kReleasedNoEvent;
				}
				else if (currentMenuInputEventType == MenuInputEventType::kEmulateInput)
				{
					if (handleAnalogStickMovement) 
					{
						const auto& stickData = glob.cdh->GetAnalogStickState
						(
							managerMenuCID, xMask == XMASK_LS
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
					if (openedMenuType == SupportedMenu::kFavorites && 
						xMask == XINPUT_GAMEPAD_RIGHT_THUMB)
					{
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
		
		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("++++++++++++++++++++++++++++++++++BOOK++++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("---------------------------------Gamepad----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("---------------------------------Keyboard---------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("---------------------------------Mouse------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kConsole];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("++++++++++++++++++++++++++++++++CONSOLE+++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kCursor];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("+++++++++++++++++++++++++++++++++CURSOR+++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("---------------------------------Gamepad----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("---------------------------------Keyboard---------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("---------------------------------Mouse------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}

		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kDebugOverlay];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("+++++++++++++++++++++++++++++++DEBUGOVERLAY+++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("---------------------------------Gamepad----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kFavorites];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("+++++++++++++++++++++++++++++++FAVORITES++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("+++++++++++++++++++++++++++++++GAMEPLAY+++++++++++++++++++++++_+++++++++++");
		SPDLOG_DEBUG("-------------------------------Gamepad------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("-------------------------------Keyboard-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("-------------------------------Mouse--------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kInventory];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("+++++++++++++++++++++++++++++++INVENTORY++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("+++++++++++++++++++++++++++++++ITEMMENU+++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("-------------------------------Gamepad------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("-------------------------------Keyboard-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("-------------------------------Mouse--------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kJournal];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("++++++++++++++++++++++++++++++++JOURNAL+++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("---------------------------------Mouse------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kLockpicking];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("++++++++++++++++++++++++++++++LOCKPICKING+++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("---------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("---------------------------------Mouse------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kMap];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("++++++++++++++++++++++++++++++++++MAP+++++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("---------------------------------Gamepad----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("+++++++++++++++++++++++++++++++MENUMODE+++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("-------------------------------Gamepad------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("-------------------------------Keyboard-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("-------------------------------Mouse--------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		
		inputContext = inputContextLists[RE::UserEvents::INPUT_CONTEXT_ID::kStats];
		if (!inputContext)
		{
			return;
		}

		SPDLOG_DEBUG("==========================================================================");
		SPDLOG_DEBUG("+++++++++++++++++++++++++++++++++STATS++++++++++++++++++++++++++++++++++++");
		SPDLOG_DEBUG("--------------------------------Gamepad-----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kGamepad])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Keyboard----------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kKeyboard])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
		}
		SPDLOG_DEBUG("--------------------------------Mouse-------------------------------------");
		for (auto& binds : inputContext->deviceMappings[RE::INPUT_DEVICE::kMouse])
		{
			SPDLOG_DEBUG("EventID: {} -> DXSCAN: 0x{:X}", binds.eventID, binds.inputKey);
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
		bool tempMenuOpenForCoop = 
		{ 
			(glob.coopSessionActive || attemptingToOpenSetupMenu) && (!Util::MenusOnlyAlwaysOpen())
		};
		// Update interpolated value and direction change flag + interpolation direction.
		// Use to set the overlay alpha value.
		const float interpValue = pmcFadeInterpData->UpdateInterpolatedValue(tempMenuOpenForCoop);

		// REMOVE when done debugging.
		/*SPDLOG_DEBUG
		(
			"[MIM] DrawPlayerMenuControlOverlay: "
			"Attempting to open setup menu: {}. Temp menu open: {}. "
			"Interped value: {}. Interp to min/max: {}, {}.", 
			attemptingToOpenSetupMenu,
			tempMenuOpenForCoop,
			interpValue,
			pmcFadeInterpData->interpToMax,
			pmcFadeInterpData->interpToMin
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
				// or P1's PID (0) if there is no recorded previous menu CID.
				pmcPlayerID = 
				(
					glob.prevMenuCID != -1 ? glob.coopPlayers[glob.prevMenuCID]->playerID : 0
				);
			}
			else if (IsRunning()) 
			{
				// Co-op session not active and a player is in the co-op setup menu, 
				// so set to the player ID of the player requesting control of this menu.
				// Set to P1's PID (0) if there is no valid manager menu PID.
				pmcPlayerID = managerMenuPlayerID != -1 ? managerMenuPlayerID : 0;
			}

			// Should never happen, but if not a valid player ID, return.
			if (pmcPlayerID == -1)
			{
				return;
			}

			uint32_t uiRGBA = (Settings::vuOverlayRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha;
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
				(Settings::vuOverlayRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
				0.5f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.625f * thickness, 0.75f * thickness), 
				glm::vec2(0.625f * thickness, rectHeight - 0.75f * thickness), 
				(Settings::vuCrosshairInnerOutlineRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.875f * thickness, thickness), 
				glm::vec2(0.875f * thickness, rectHeight - thickness), 
				(Settings::vuCrosshairOuterOutlineRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);

			// Right Edge.
			DebugAPI::QueueLine2D
			(
				glm::vec2(rectWidth - 0.25f * thickness, 0.5f * thickness), 
				glm::vec2(rectWidth - 0.25f * thickness, rectHeight - 0.5f * thickness), 
				(Settings::vuOverlayRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
				0.5f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(rectWidth - 0.625f * thickness, 0.75f * thickness), 
				glm::vec2(rectWidth - 0.625f * thickness, rectHeight - 0.75f * thickness), 
				(Settings::vuCrosshairInnerOutlineRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(rectWidth - 0.875f * thickness, thickness), 
				glm::vec2(rectWidth - 0.875f * thickness, rectHeight - thickness), 
				(Settings::vuCrosshairOuterOutlineRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);

			// Top Edge.
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.0f, 0.25f * thickness), 
				glm::vec2(rectWidth, 0.25f * thickness),
				(Settings::vuOverlayRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
				0.5f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.5f * thickness, 0.625f * thickness), 
				glm::vec2(rectWidth - 0.5f * thickness, 0.625f * thickness),
				(Settings::vuCrosshairInnerOutlineRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha,
				0.25f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.75f * thickness, 0.875f * thickness), 
				glm::vec2(rectWidth - 0.75f * thickness, 0.875f * thickness),
				(Settings::vuCrosshairOuterOutlineRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
				0.25f * thickness
			);

			// Bottom Edge.
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.0f, rectHeight - 0.25f * thickness), 
				glm::vec2(rectWidth, rectHeight - 0.25f * thickness), 
				(Settings::vuOverlayRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
				0.5f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.5f * thickness, rectHeight - 0.625f * thickness), 
				glm::vec2(rectWidth - 0.5f * thickness, rectHeight - 0.625f * thickness), 
				(Settings::vuCrosshairInnerOutlineRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha,
				0.25f * thickness
			);
			DebugAPI::QueueLine2D
			(
				glm::vec2(0.75f * thickness, rectHeight - 0.875f * thickness), 
				glm::vec2(rectWidth - 0.75f * thickness, rectHeight - 0.875f * thickness), 
				(Settings::vuCrosshairOuterOutlineRGBAValues[pmcPlayerID] & 0xFFFFFF00) + alpha, 
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

				const auto& em = glob.coopPlayers[glob.player1CID]->em;
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
				
				SPDLOG_DEBUG
				(
					"[MIM] EquipP1QSForm: {} at index {} is mapped to {}. "
					"Entries to indices map size: {} (empty: {}).", 
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

				bool isConsumable = form->Is(RE::FormType::AlchemyItem, RE::FormType::Ingredient);
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
					entryStr = fmt::format("[*QS{}*] {}", isConsumable ? "I" : "S", entryStr);
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
						auto qsTagStartIndex = oldEntryStr.find("[*QS", 0);
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
					auto qsTagStartIndex = entryStr.find("[*QS", 0);
					if (qsTagStartIndex != std::string::npos && 
						qsTagStartIndex + qsPrefixTagLength <= entryStr.length())
					{
						entryStr = entryStr.substr(qsTagStartIndex + qsPrefixTagLength);
					}
				}

				SPDLOG_DEBUG
				(
					"[MIM] EquipP1QSForm: {} {}.", equipped ? "Equipped" : "Unequipped", entryStr
				);

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
				glob.coopPlayers[glob.player1CID]->em->RefreshEquipState(RefreshSlots::kAll);
				// Update the list to reflect our changes.
				view->InvokeNoReturn("_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0);
			}
		);
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

		auto ue = RE::UserEvents::GetSingleton();
		if (!ue)
		{
			return;
		}

		auto ui = RE::UI::GetSingleton();
		if (!ui || !ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME))
		{
			return;
		}

		int32_t menuCID = glob.menuCID;
		if (menuCID == -1) 
		{
			menuCID = glob.player1CID;
		}

		const auto& rsData = glob.cdh->GetAnalogStickState(menuCID, false);
		if (rsData.normMag == 0.0f)
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

		if (a_setHotkey)
		{
			if (hotkeyEvent == ""sv)
			{
				return;
			}

			// Hotkey the entry through an emulated keyboard input.
			auto hotkeyCode = controlMap->GetMappedKey(hotkeyEvent, RE::INPUT_DEVICE::kKeyboard);
			if (hotkeyCode == 0xFF)
			{
				return;
			}

			taskInterface->AddUITask
			(
				[this, hotkeyEvent, hotkeyCode, hotkeySlotToChange, menuCID]() 
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
					auto form = favoritesMenu->favorites[index].item;
					// Must have a valid selected form.
					if (!form)
					{
						return;
					}

					const auto& p = glob.coopPlayers[menuCID];
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
					bool isP1Hotkeyed = Util::IsHotkeyed
					(
						RE::PlayerCharacter::GetSingleton(), form
					);

					SPDLOG_DEBUG
					(
						"[MIM] HotkeyFavoritedForm: {}: Hotkeying {} into slot {}, "
						"is now hotkeyed by P1: {}.",
						p->coopActor->GetName(),
						form->GetName(), 
						hotkeySlotToChange, 
						isP1Hotkeyed
					);

					if (!p->isPlayer1) 
					{
						// Ensure the companion player has the same hotkey state
						// as P1 for the form.
						Util::ChangeFormHotkeyStatus
						(
							p->coopActor.get(), 
							form, 
							isP1Hotkeyed ? hotkeySlotToChange : -1
						);

						SPDLOG_DEBUG("[MIM] HotkeyFavoritedForm: {} is now hotkeyed by P1: {}.",
							form->GetName(), isP1Hotkeyed);
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
			if (hotkeySlotToChange == -1)
			{
				return;
			}

			taskInterface->AddUITask
			(
				[this, hotkeySlotToChange, menuCID]() 
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
					
					const auto& p = glob.coopPlayers[menuCID];
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
						SPDLOG_DEBUG
						(
							"[MIM] HotkeyFavoritedForm: {}'s favorited form {} "
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
						SPDLOG_DEBUG
						(
							"[MIM] HotkeyFavoritedForm: {}: {} will be hotkeyed in slot {} "
							"on release.",
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
		const auto& p = glob.coopPlayers[managerMenuCID];
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
			SPDLOG_DEBUG
			(
				"[MIM] HandleMenuEvent: Equip Request Event: from container: {}, "
				"form: {}, equip index: {}, placeholder spell changed: {}.",
				(Util::HandleIsValid(fromContainerHandle)) ? 
				fromContainerHandle.get()->GetName() : 
				"NONE",
				(selectedForm) ? selectedForm->GetName() : "NONE",
				reqEquipIndex,
				placeholderMagicChanged
			);
			// Equip/unequip the selected form.
			glob.coopPlayers[managerMenuCID]->em->HandleMenuEquipRequest
			(
				fromContainerHandle, selectedForm, reqEquipIndex, placeholderMagicChanged
			);
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
					// Have to set cyclable favorited spells
					// after a spell is favorited/unfavorited.
					RefreshCyclableSpells();
					// Equip "carets" get cleared, 
					// so we have to update the equip state when (un)favoriting.
					RefreshMagicMenuEquipState(true);
					spellFavoriteStatusChanged = false;
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
		SPDLOG_DEBUG("[MIM] InitFavoritesEntries.");

		// Update equip states in the Favorites Menu 
		// for forms equipped by the co-op companion player.

		// Ensure cached favorited items are up to date.
		const auto& p = glob.coopPlayers[managerMenuCID];
		p->em->RefreshEquipState(RefreshSlots::kAll);
		// Update menu equip state with the refreshed favorites data.
		RefreshFavoritesMenuEquipState(true);
	}

	void MenuInputManager::InitMagicMenuEquippedStates()
	{
		// Update equip states in the Magic Menu 
		// for spells/shouts equipped by the co-op companion player.

		// Ensure companio player placeholder spell/shouts are NOT learned by P1.
		if (managerMenuCID != -1) 
		{
			const auto& p = glob.coopPlayers[managerMenuCID];
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

		// Set selectable magic forms list first.
		SetMagicMenuFormsList();
		// Update magic menu equip state after.
		RefreshMagicMenuEquipState(true);
	}
	
	void MenuInputManager::InitP1QSFormEntries()
	{
		// Set quick slot tags for any equipped quick slot items/spells 
		// and update index-to-entry map.

		SPDLOG_DEBUG("[MIM] InitP1QSFormEntries");
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
		const auto& em = glob.coopPlayers[glob.player1CID]->em;
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
			else if (favForm->Is(RE::FormType::AlchemyItem, RE::FormType::Ingredient))
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
							const auto& em = glob.coopPlayers[glob.player1CID]->em;
							if (index == em->equippedQSItemIndex || 
								index == em->equippedQSSpellIndex)
							{
								if (entryStr.find("[*QS", 0) == std::string::npos)
								{
									bool isConsumable = index == em->equippedQSItemIndex;
									entryStr = fmt::format
									(
										"[*QS{}*] {}", isConsumable ? "I" : "S", entryStr
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
									SPDLOG_DEBUG
									(
										"[MIM] InitP1QSFormEntries. Set {} entry as {}.",
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
			return false;
		}

		// Get the container, which should be the player if they are in their own inventory,
		// or the container the player is attempting to loot the book from.
		auto containerRefrPtr = Util::GetRefrPtrFromHandle(menuContainerHandle); 
		if (!containerRefrPtr) 
		{
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
				return false;
			}

			const float avLvl = menuCoopActorPtr->GetBaseActorValue(skillAV);
			// Update serialized skill base/increments list entry and increment skill AV level.
			// Must have serializable data.
			const auto iter = glob.serializablePlayerData.find(menuCoopActorPtr->formID);
			if (iter == glob.serializablePlayerData.end())
			{
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
					"[ALYSLC] {} increased to {}! {} Points left: {}",
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
				return false;
			}

			if (!pointsAvailable)
			{
				// No points to use.
				RE::DebugMessageBox
				(
					fmt::format
					(
						"[ALYSLC] You do not have enough {} Points!", 
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
							"[ALYSLC] You already have developed this skill too well "
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
							"[ALYSLC] Cannot use {} to level up a skill.",
							a_skillbook->GetName()
						).c_str()
					);
				}
			}

			return false;
		}

		return false;
	}

	void MenuInputManager::ProcessBarterMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle BarterMenu input.

		auto ue = RE::UserEvents::GetSingleton(); 
		if (!barterMenu || !ue) 
		{
			return;
		}

		// Default: 'A' button to sell item.
		auto acceptIDCode = controlMap->GetMappedKey
		(
			ue->accept, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		);
		if (a_xMask == glob.cdh->GAMEMASK_TO_XIMASK.at(acceptIDCode))
		{
			// Ensure that the co-op companion cannot sell any object 
			// that is currently equipped by P1.
			auto selectedItem = barterMenu->itemList->GetSelectedItem(); 
			if (selectedItem && selectedItem->data.GetEquipState() != 0)
			{
				RE::DebugMessageBox
				(
					"[ALYSLC] Cannot sell an item currently equipped by Player 1!"
				);
				currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			}
		}

		if (!barterMenu->itemList)
		{
			return;
		}
		
		// Should not refresh if an item is selected, 
		// since refreshing while attempting to select the number of an item to sell 
		// through the bottom right quantity menu (I don't know the proper name for it)
		// glitches the entire barter menu until the quantity menu is closed.
		auto currentItem = barterMenu->itemList->GetSelectedItem(); 
		if (currentItem)
		{
			return;
		}
		
		// Signal to refresh the menu.
		shouldRefreshMenu = true;
	}

	void MenuInputManager::ProcessBookMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle BookMenu input.

		auto ue = RE::UserEvents::GetSingleton();
		if (!bookMenu || !ue)
		{
			return;
		}

		auto acceptIDCode = controlMap->GetMappedKey
		(
			ue->accept, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		); 
		// Take book with the 'Accept' bind.
		// Ignore other inputs.
		if (a_xMask != glob.cdh->GAMEMASK_TO_XIMASK.at(acceptIDCode))
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
		auto cancelIDCode = controlMap->GetMappedKey
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
					RE::INPUT_DEVICE::kGamepad, ue->cancel, cancelIDCode, 0.0f, 1.0f)
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

	void MenuInputManager::ProcessContainerMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle ContainerMenu input.

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			return;
		}

		auto ue = RE::UserEvents::GetSingleton();
		if (!containerMenu || !ue)
		{
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
		if (!isCoopInventory && 
			!isPickpocketing &&
			glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask) == 
			controlMap->GetMappedKey(ue->wait, RE::INPUT_DEVICE::kGamepad))
		{
			RE::DebugMessageBox
			(
				"[ALYSLC] P1's inventory is not accessible to other players while looting."
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

			return;
		}

		if (a_xMask == XINPUT_GAMEPAD_A || a_xMask == XINPUT_GAMEPAD_X)
		{
			if (isCoopInventory)
			{
				auto boundObj = selectedItem->data.objDesc->object;
				if (!boundObj)
				{
					return;
				}

				const auto& p = glob.coopPlayers[managerMenuCID];
				// Unequip before dropping/transferring to avoid crash.
				auto foundIter = std::find_if
				(
					p->em->desiredEquippedForms.begin(), p->em->desiredEquippedForms.end(), 
					[boundObj](RE::TESForm* a_form) { return a_form == boundObj; }
				);
				if (foundIter != p->em->desiredEquippedForms.end())
				{
					auto index = foundIter - p->em->desiredEquippedForms.begin();
					p->em->UnequipFormAtIndex(static_cast<EquipIndex>(index));
				}
				else if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
				{
					aem->UnequipObject(menuCoopActorPtr.get(), boundObj);
				}
				
				// Get the current number owned before dropping/transferring.
				int32_t currentCount = 0;
				auto inventory = menuCoopActorPtr->GetInventory();
				const auto iter = inventory.find(boundObj);
				if (iter != inventory.end())
				{
					currentCount = iter->second.first;
				}

				// Unfavorite the item if none of this item will remain 
				// after dropping/transferring one.
				if (currentCount <= 1)
				{
					Util::ChangeFormFavoritesStatus(menuCoopActorPtr.get(), boundObj, false);
				}

				// Drop the item.
				if (a_xMask == XINPUT_GAMEPAD_X)
				{
					// Handled here; no event to send.
					currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
					RE::DebugNotification
					(
						fmt::format
						(
							"{} is dropping 1 {}", menuCoopActorPtr->GetName(), boundObj->GetName()
						).c_str()
					);
					// Place in front of the player at torso height.
					auto dropPos = 
					(
						glob.coopPlayers[managerMenuCID]->mm->playerTorsoPosition + 
						Util::RotationToDirectionVect
						(
							0.0f, 
							Util::ConvertAngle
							(
								menuCoopActorPtr->GetHeading(false)
							)
						) * 0.5f * menuCoopActorPtr->GetHeight()
					);
					menuCoopActorPtr->RemoveItem
					(
						boundObj, 1, RE::ITEM_REMOVE_REASON::kDropping, nullptr, nullptr, &dropPos
					);
				}

				// Refresh the menu since an entry was changed
				// upon dropping or transferring the item.
				shouldRefreshMenu = true;
			}
			else if (isPickpocketing)
			{
				// Ensure that the co-op companion cannot reverse-pickpocket any object 
				// that is currently equipped by P1.
				if (selectedItem && selectedItem->data.GetEquipState() != 0)
				{
					RE::DebugMessageBox
					(
						"[ALYSLC] Cannot plant an item currently equipped by Player 1!"
					);
					currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
				}
			}
		}
		// Favorite the selected item.
		else if (a_xMask == XINPUT_GAMEPAD_Y)
		{
			// Handled here; no event to send.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;

			// Need inventory changes to (un)favorite any selected form.
			auto inventoryChanges = menuCoopActorPtr->GetInventoryChanges();
			if (!inventoryChanges)
			{
				return;
			}

			// Companion player's inventory must be open.
			if (!isCoopInventory)
			{
				return;
			}
				
			selectedForm = selectedItem->data.objDesc->object;
			// Must have a selected form to (un)favorite.
			if (!selectedForm)
			{
				return;
			}

			// Favorite the item when in the player's inventory.
			// Credit to po3 for the code to check if the item has been favorited.
			// From an older version of:
			// https://github.com/powerof3/PapyrusExtenderSSE/
			bool shouldFavorite = true;
			auto inventory = menuCoopActorPtr->GetInventory();
			RE::InventoryEntryData* entryData = nullptr;
			for (const auto& inventoryEntry : inventory)
			{
				if (!inventoryEntry.first || inventoryEntry.first != selectedForm)
				{
					continue;
				}

				auto& entryData = inventoryEntry.second.second;
				if (!entryData)
				{
					continue;
				}

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
						exDataList = entryData->extraLists->front();
						Util::NativeFunctions::Favorite
						(
							inventoryChanges, entryData.get(), exDataList
						);
					}
					else
					{
						Util::NativeFunctions::Unfavorite
						(
							inventoryChanges, entryData.get(), exDataList
						);
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

					// Found and (un)favorited our selected item, 
					// so no need to continue looking for it.
					break;
				}
				else
				{
					// Entry data may not have a list of extra data, 
					// but we can still favorite the item.
					Util::NativeFunctions::Favorite(inventoryChanges, entryData.get(), nullptr);
				}
			}

			// Refresh menu to display the changed favorites status indicator.
			shouldRefreshMenu = true;
		}
	}

	void MenuInputManager::ProcessDialogueMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle DialogueMenu input.

		auto ue = RE::UserEvents::GetSingleton();
		if (!ue)
		{
			return;
		}

		auto buttonGroup = glob.paInfoHolder->XIMASKS_TO_INPUT_GROUPS.at(a_xMask);
		const auto gameMask = glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask);
		auto acceptIDCode = controlMap->GetMappedKey
		(
			ue->accept, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		);
		auto cancelIDCode = controlMap->GetMappedKey
		(
			ue->cancel, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		);
		// Can only use the DPad to navigate and either close the menu or choose dialogue options.
		// Block all other controls from being emulated.
		if (buttonGroup != InputGroup::kDPad && 
			gameMask != cancelIDCode && 
			gameMask != acceptIDCode)
		{
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
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
				// No event to handle.
				currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
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

					uint32_t selectedEntryNum = favMenuIndexToEntryMap.at(index);
					auto form = favoritesMenu->favorites[index].item;
					if (!form)
					{
						return;
					}
					
					// Ignore non-quick slot items.
					bool isConsumable = form->Is
					(
						RE::FormType::AlchemyItem, RE::FormType::Ingredient
					);
					bool isSpell = form->Is(RE::FormType::Spell);
					if (!isConsumable && !isSpell)
					{
						return;
					}
					
					const auto& em = glob.coopPlayers[managerMenuCID]->em;
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
						entryStr = fmt::format("[*QS{}*] {}", isConsumable ? "I" : "S", entryStr);
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

							auto qsTagStartIndex = oldEntryStr.find("[*QS", 0);
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
						auto qsTagStartIndex = entryStr.find("[*QS", 0);
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
					glob.coopPlayers[managerMenuCID]->em->RefreshEquipState(RefreshSlots::kAll);
				}
			);

			// No event to handle.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
		else if (a_xMask == XINPUT_GAMEPAD_DPAD_LEFT || a_xMask == XINPUT_GAMEPAD_DPAD_RIGHT)
		{
			// Changing categories clears our changes to the equip "carets", 
			// (the empty/LH/RH arrow to the left of each equipped menu entry),
			// and imports P1's item equip state. 
			// Have to reimport the companion player's equip state, 
			// but no need to refresh the cached equipped data, which has not changed.
			RefreshFavoritesMenuEquipState(false);
		}
		else if (a_xMask == XINPUT_GAMEPAD_A)
		{
			// Ignore equip attempts with the "A" button, 
			// as emulating input here will equip this player's favorited item on P1.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
		else if (a_xMask == XINPUT_GAMEPAD_X)
		{
			auto taskInterface = SKSE::GetTaskInterface();
			if (!taskInterface)
			{
				// No event to handle.
				currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
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
						if (glob.mim->managerMenuCID != -1)
						{
							const auto& p = glob.coopPlayers[glob.mim->managerMenuCID];
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

			// No event to handle.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
		else if (a_xMask == XINPUT_GAMEPAD_RIGHT_THUMB)
		{
			// Hotkey the selected form, if any.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
	}

	void MenuInputManager::ProcessGiftMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle GiftMenu input.

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			return;
		}

		auto ue = RE::UserEvents::GetSingleton();
		if (!ue || !giftMenu)
		{
			return;
		}

		if (a_xMask == XINPUT_GAMEPAD_A)
		{
			RE::ItemList::Item* selectedItem = GetSelectedItem(giftMenu->itemList); 
			if (!selectedItem)
			{
				return;
			}

			auto boundObj = selectedItem->data.objDesc->object;
			if (!boundObj)
			{
				return;
			}

			const auto& p = glob.coopPlayers[managerMenuCID];
			// Unequip before gifting to avoid crash.
			auto foundIter = std::find_if
			(
				p->em->desiredEquippedForms.begin(), p->em->desiredEquippedForms.end(), 
				[boundObj](RE::TESForm* a_form) { return a_form == boundObj; }
			);
			if (foundIter != p->em->desiredEquippedForms.end())
			{
				auto index = foundIter - p->em->desiredEquippedForms.begin();
				p->em->UnequipFormAtIndex(static_cast<EquipIndex>(index));
			}
			else if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				aem->UnequipObject(menuCoopActorPtr.get(), boundObj);
			}

			int32_t currentCount = 0;
			auto inventory = menuCoopActorPtr->GetInventory();
			const auto iter = inventory.find(boundObj);
			if (iter != inventory.end())
			{
				currentCount = iter->second.first;
			}

			// Unfavorite if none of this item will remain after gifting one.
			if (currentCount <= 1)
			{
				Util::ChangeFormFavoritesStatus(menuCoopActorPtr.get(), boundObj, false);
			}

			shouldRefreshMenu = true;
		}
	}

	void MenuInputManager::ProcessInventoryMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle InventoryMenu input.

		auto ue = RE::UserEvents::GetSingleton();
		if (!ue)
		{
			return;
		}

		auto buttonGroup = glob.paInfoHolder->XIMASKS_TO_INPUT_GROUPS.at(a_xMask);
		// Companion players can only move through P1's inventory or exit.
		// Ignore all other input events.
		auto cancelIDCode = controlMap->GetMappedKey
		(
			ue->cancel, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		);
		if (buttonGroup != InputGroup::kDPad && 
			glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask) != cancelIDCode)
		{
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
	}

	void MenuInputManager::ProcessLootMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle LootMenu input.

		auto ue = RE::UserEvents::GetSingleton();
		if (!ue)
		{
			return;
		}
		
		// Button codes for QuickLootMenu binds.
		std::set<uint32_t> allowedButtonIDCodes{ };
		auto buttonGroup = glob.paInfoHolder->XIMASKS_TO_INPUT_GROUPS.at(a_xMask);
		const auto gameMask = glob.cdh->XIMASK_TO_GAMEMASK.at(a_xMask);
		// Add all default QuickLootEE button codes.
		auto acceptIDCode = controlMap->GetMappedKey
		(
			ue->accept, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		);
		auto cancelIDCode = controlMap->GetMappedKey
		(
			ue->cancel, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
		);
		auto readyWeaponIDCode = controlMap->GetMappedKey
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
		if (gameMask == cancelIDCode)
		{
			// Exit menu and relinquish control when cancel bind is pressed.
			if (auto crosshairPickData = RE::CrosshairPickData::GetSingleton(); crosshairPickData)
			{
				SPDLOG_DEBUG("[MIM] ProcessLootMenuButtonInput: {} is closing LootMenu.",
					Util::HandleIsValid(menuCoopActorHandle) ?
					menuCoopActorHandle.get()->GetName() :
					"NONE"
				);
				Util::SendCrosshairEvent(nullptr);
			}

			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
		}
		else if (buttonGroup != InputGroup::kDPad && !allowedButtonIDCodes.contains(gameMask))
		{
			// Ignore all button presses (do not send emulated input event)
			// that are not the DPad or the 'Cancel', 'Accept', or 'Ready Weapon' binds.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
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
			// Save selected form to add to cyclable spells list later 
			// after the spell is favorited.
			selectedForm = GetSelectedMagicMenuSpell();
			if (selectedForm) 
			{
				// Remove any assigned hotkey when (un)favoriting 
				// to prevent lingering hotkey assignments.
				Util::ChangeFormHotkeyStatus
				(
					RE::PlayerCharacter::GetSingleton(), selectedForm, -1
				);
				spellFavoriteStatusChanged = true;
				shouldRefreshMenu = true;
			}
		}
		else if (a_xMask == XINPUT_GAMEPAD_DPAD_LEFT || a_xMask == XINPUT_GAMEPAD_DPAD_RIGHT)
		{
			// Changing categories clears our changes to the equip "carets", 
			// (the empty/LH/RH arrow to the left of each equipped menu entry),
			// and imports P1's spell equip state. 
			// Have to reimport the co-op companion player's equip state,
			// but no need to refresh the cached equipped data, which has not changed.
			RefreshMagicMenuEquipState(false);
		}
	}

	void MenuInputManager::ProcessMapMenuButtonInput(const uint32_t& a_xMask)
	{
		// Handle MapMenu input.

		// All inputs forwarded as emulated P1 inputs.
		currentMenuInputEventType = MenuInputEventType::kEmulateInput;
	}

	void MenuInputManager::ProcessTriggerInput(const bool& a_isLT)
	{
		// Perform menu-dependent actions based on trigger press.

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			return;
		}

		switch (openedMenuType)
		{
		case SupportedMenu::kContainer:
		{
			if (!containerMenu) 
			{
				if (auto ui = RE::UI::GetSingleton(); ui) 
				{
					containerMenu = ui->GetMenu<RE::ContainerMenu>();
				}

				return;
			}

			// No events to send by default. 
			// Do not want to equip selected items onto P1 through trigger presses.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			RE::ItemList::Item* selectedItem = GetSelectedItem(containerMenu->itemList);
			if (!selectedItem)
			{
				return;
			}

			auto obj = selectedItem ? selectedItem->data.objDesc->object : nullptr; 
			if (!obj)
			{
				return;
			}

			const auto p1 = RE::PlayerCharacter::GetSingleton(); 
			auto asBook = obj->As<RE::TESObjectBOOK>(); 
			if (asBook && p1)
			{
				// If this is a skillbook, co-op companions will level up 
				// the accompanying skill in the activation hook after P1 uses the book.
				currentMenuInputEventType = MenuInputEventType::kEmulateInput;
			}
			else if (ALYSLC::EnderalCompat::g_enderalSSEInstalled && 
					 obj->As<RE::AlchemyItem>() && 
					 obj->As<RE::AlchemyItem>()->HasKeywordString("Lehrbuch"))
			{
				auto item = obj->As<RE::AlchemyItem>();
				// Level up with Enderal skillbook.
				bool succ = PerformEnderalSkillLevelUp(item);
				if (succ)
				{
					// No event and we do not want P1 to use the book.
					currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
				}
				else
				{
					// Have P1 use the book on failure.
					currentMenuInputEventType = MenuInputEventType::kEmulateInput;
				}
			}
			else
			{
				const auto mode = containerMenu->GetContainerMode();
				// Only transfer if the player is not pickpocketing the item,
				// or if the pickpocket attempt was successful.
				// Can't simply emulate a trigger press for P1 
				// because even though this will attempt the pickpocket for us,
				// it will equip the item on P1, instead of the companion player, if successful.
				bool canTransfer = true;
				if (mode == RE::ContainerMenu::ContainerMode::kPickpocket)
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
							SPDLOG_DEBUG("{}'s Gold value: {}.", 
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
					fromContainerHandle = 
					(
						isCoopInventory ? menuCoopActorHandle : menuContainerHandle
					);
					reqEquipIndex = a_isLT ? EquipIndex::kLeftHand : EquipIndex::kRightHand;
					selectedForm = obj;
					placeholderMagicChanged = false;
					bool isEquipped = glob.coopPlayers[managerMenuCID]->em->IsEquipped
					(
						selectedForm
					);
					// If the item will be equipped/unequipped from the player's own inventory,
					// or if the item is looted from a container and is not already equipped,
					// we want to wait until the equip event fires
					// before refreshing the player's equip state.
					if (isCoopInventory || !isEquipped)
					{
						// Refresh equip state later once the item is (un)equipped.
						delayedEquipStateRefresh = true;
						lastEquipStateRefreshReqTP = SteadyClock::now();
					}
					else
					{
						// Refresh right away after item removal otherwise.
						shouldRefreshMenu = true;
					}

					// If container reference is the player,
					// or the item is not valid, do not remove it from its container.
					if (isCoopInventory || !obj)
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

								menuCoopActorPtr->PickUpObject(handle.get().get(), 1);
							}
						}
					}
					else
					{
						auto counts = containerRefrPtr->GetInventoryCounts();
						const auto iter2 = counts.find(obj);
						if (iter2 != counts.end())
						{
							auto count = iter2->second;
							if (count > 0)
							{
								// NOTE: 
								// Directly to the menu-controlling companion player
								// to be equipped immediately. 
								// Delaying the transfer may cause the equip call to execute
								// before the item is transfered and the equip will fail.
								containerRefrPtr->RemoveItem
								(	
									obj,
									count, 
									RE::ITEM_REMOVE_REASON::kStoreInContainer,
									nullptr,
									menuCoopActorPtr.get()
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
						const auto& reqP = glob.coopPlayers[glob.menuCID];
						msgQ->AddMessage
						(
							RE::ContainerMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kForceHide, nullptr
						);
						return;
					}

					/*
					auto ue = RE::UserEvents::GetSingleton();
					if (!ue)
					{
						return;
					}

					// Need to send a 'Cancel' input event to close the menu.
					auto cancelIDCode = controlMap->GetMappedKey
					(
						ue->cancel,
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
								RE::INPUT_DEVICE::kGamepad, ue->cancel, cancelIDCode, 0.0f, 1.0f)
						)
					);
					// Sent by a companion player.
					(*buttonEvent2.get())->AsIDEvent()->pad24 = 0xCA11;

					Util::SendInputEvent(buttonEvent);
					Util::SendInputEvent(buttonEvent2);
					*/
				}
			}

			break;
		}
		case SupportedMenu::kFavorites:
		{
			if (!favoritesMenu)
			{
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					favoritesMenu = ui->GetMenu<RE::FavoritesMenu>();
				}

				return;
			}

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

			auto boundObj = selectedForm->As<RE::TESBoundObject>();
			// If the favorited item is a spell/shout or a physical item that exists
			// in the co-op player's inventory, then attempt to equip and update the menu.
			bool equipable = 
			{
				(selectedForm->Is(RE::FormType::Shout, RE::FormType::Spell))
			};
			if (!equipable && boundObj)
			{
				auto inventory = menuCoopActorPtr->GetInventory();
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
						(asSpell && asSpell->GetSpellType() == RE::MagicSystem::SpellType::kSpell))
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
									glob.coopPlayers[managerMenuCID]->coopActor.get()
								)
							);
							newEquipState = 
							(
								a_isLT || isVampireLord ? 
								EntryEquipState::kLH : 
								EntryEquipState::kRH
							);
							reqEquipIndex = 
							(
								a_isLT || isVampireLord ? 
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
					delayedEquipStateRefresh = true;
					lastEquipStateRefreshReqTP = SteadyClock::now();
				}
				else if (selectedForm->Is(RE::FormType::AlchemyItem, RE::FormType::Ingredient))
				{
					UpdateFavoritedConsumableCount(selectedForm, index);
				}

				auto spellToEquip = selectedForm->As<RE::SpellItem>();
				bool isHandSlotSpell = 
				(
					spellToEquip && 
					spellToEquip->GetSpellType() == RE::MagicSystem::SpellType::kSpell
				);
				const auto& em = glob.coopPlayers[managerMenuCID]->em;
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
			}
			else
			{
				// Not equipable.
				selectedForm = nullptr;
			}

			break;
		}
		case SupportedMenu::kInventory:
		{
			// Do not send emulated trigger input because this qill
			// equip the selected item onto P1, not the player controlling menus.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			break;
		}
		case SupportedMenu::kMagic:
		{
			if (!magicMenu)
			{
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					magicMenu = ui->GetMenu<RE::MagicMenu>();
				}

				return;
			}

			selectedForm = GetSelectedMagicMenuSpell(); 
			if (selectedForm)
			{
				auto magicItemList = reinterpret_cast<RE::ItemList*>(magicMenu->unk30); 
				if (!magicItemList)
				{
					return;
				}

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
				delayedEquipStateRefresh = true;
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

			break;
		}
		case SupportedMenu::kLoot:
		{
			// No event to send in the Loot Menu.
			currentMenuInputEventType = MenuInputEventType::kPressedNoEvent;
			break;
		}
		default:
		{
			break;
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
			SPDLOG_DEBUG("[MIM] SetOpenedMenu: New menu on top of the stack: {}", menuName);
		}
		else
		{
			menuName = "";
			SPDLOG_DEBUG("[MIM] SetOpenedMenu: Menu stack is now empty.");
		}

		newHash = Hash(menuName);
		// Only update menu type if a new menu is atop the stack.
		if (newHash != oldHash) 
		{
			SPDLOG_DEBUG("[MIM] SetOpenedMenu: Getting lock. (0x{:X})", 
				std::hash<std::jthread::id>()(std::this_thread::get_id()));
			{
				std::unique_lock<std::mutex> lock(openedMenuMutex);
				SPDLOG_DEBUG("[MIM] SetOpenedMenu: Lock obtained. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id()));
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

	void MenuInputManager::RefreshFavoritesMenuEquipState(bool&& a_updateCachedEquipState)
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
		const auto& em = glob.coopPlayers[managerMenuCID]->em;
		auto favoritedFormIDsSet = em->favoritedFormIDs;
		// Maintain P1's favorites list if transformed into a Vampire Lord.
		bool isVampireLord = Util::IsVampireLord
		(
			glob.coopPlayers[managerMenuCID]->coopActor.get()
		);
		// Update cached equip states for all favorited items.
		if (a_updateCachedEquipState) 
		{
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
			auto& favItemsEquippedIndices = em->favItemsEquippedIndices;
			favItemsEquippedIndices.clear();

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
				auto favForm = favoritesList[i].item;
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
						// Check LH and RH for the same weapon.
						// LH
						if (equippedForms[!EquipIndex::kLeftHand] && 
							equippedForms[!EquipIndex::kLeftHand] == favForm)
						{
							// Is two-handed weapon.
							if (asWeap && 
								asWeap->equipSlot->flags.any
								(
									RE::BGSEquipSlot::Flag::kUseAllParents
								))
							{
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} equipped in both hands.", 
									favForm->GetName()
								);
								favEntryEquipStates[i] = EntryEquipState::kBothHands;
							}
							// One-handed.
							else
							{
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} equipped in left hand.", 
									favForm->GetName()
								);
								favEntryEquipStates[i] = 
								(
									favEntryEquipStates[i] == EntryEquipState::kRH ? 
									EntryEquipState::kBothHands : 
									EntryEquipState::kLH
								);
							}

							favItemsEquippedIndices.insert(i);
						}
						
						// RH
						if (equippedForms[!EquipIndex::kRightHand] && 
							equippedForms[!EquipIndex::kRightHand] == favForm)
						{
							// Is two-handed weapon.
							if (asWeap && 
								asWeap->equipSlot->flags.any
								(
									RE::BGSEquipSlot::Flag::kUseAllParents
								))
							{
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} equipped in both hands.",
									favForm->GetName()
								);
								favEntryEquipStates[i] = EntryEquipState::kBothHands;
							}
							// One-handed.
							else
							{
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} equipped in right hand.", 
									favForm->GetName()
								);
								favEntryEquipStates[i]= 
								(
									favEntryEquipStates[i] == EntryEquipState::kLH ? 
									EntryEquipState::kBothHands : 
									EntryEquipState::kRH
								);
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
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} equipped in both hands.", favForm->GetName()
								);
								favEntryEquipStates[i] = EntryEquipState::kBothHands;
								favItemsEquippedIndices.insert(i);
							}
							// LH spell only
							else if (favEquippedLH)
							{
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} equipped in left hand.", 
									favForm->GetName()
								);
								favEntryEquipStates[i] = EntryEquipState::kLH;
								favItemsEquippedIndices.insert(i);
							}
							// RH spell only
							else if (favEquippedRH)
							{
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} equipped in right hand.", 
									favForm->GetName()
								);
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
							auto voiceForm = em->equippedForms[!EquipIndex::kVoice]; 
							if (favForm == voiceForm)
							{
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} equipped in voice slot.", 
									favForm->GetName()
								);
								favEntryEquipStates[i] = EntryEquipState::kDefault;
								favItemsEquippedIndices.insert(i);
							}
						}

						// Check if quick slot spell is equipped.
						auto quickSlotSpell = em->quickSlotSpell; 
						if (quickSlotSpell && quickSlotSpell == favForm)
						{
							SPDLOG_DEBUG
							(
								"[MIM] RefreshFavoritesMenuEquipState: {} equipped in quick slot.",
								favForm->GetName()
							);
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
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} with highest var index {} equipped in voice slot",
									asShout->GetName(), em->highestShoutVarIndex
								);
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
							if (form && 
								form->Is(RE::FormType::Armature, RE::FormType::Armor) && 
								form == favForm)
							{
								SPDLOG_DEBUG
								(
									"[MIM] RefreshFavoritesMenuEquipState: "
									"{} (0x{:X}) armor equipped.",
									form->GetName(), form->formID
								);
								favEntryEquipStates[i] = EntryEquipState::kDefault;
								favItemsEquippedIndices.insert(i);

								break;
							}
						}
					}
					// Ammo
					else if (favForm->Is(RE::FormType::Ammo))
					{
						auto currentAmmo = equippedForms[!EquipIndex::kAmmo]; 
						if (currentAmmo && currentAmmo == favForm)
						{
							SPDLOG_DEBUG
							(
								"[MIM] RefreshFavoritesMenuEquipState: {} (0x{:X}) ammo equipped.",
								currentAmmo->GetName(), currentAmmo->formID
							);
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
							SPDLOG_DEBUG
							(
								"[MIM] RefreshFavoritesMenuEquipState: {} equipped in quick slot.",
								favForm->GetName()
							);
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

		shouldRefreshMenu = true;
	}


	void MenuInputManager::RefreshMagicMenuEquipState(bool&& a_updateCachedEquipState)
	{
		// Refresh displayed and/or cached equip state while in the MagicMenu.

		if (!magicMenu) 
		{
			if (auto ui = RE::UI::GetSingleton(); ui)
			{
				magicMenu = ui->GetMenu<RE::MagicMenu>();
			}

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
				return;
			}

			// Clear out cached data before repopulating.
			magEntryEquipStates.clear();
			magEntryEquipStates = 
			(
				std::vector<EntryEquipState>(numMagItems, EntryEquipState::kNone)
			);

			const auto& em = glob.coopPlayers[managerMenuCID]->em;
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
						SPDLOG_DEBUG
						(
							"[MIM] RefreshMagicMenuEquipState: {} equipped in both hands.", 
							magForm->GetName()
						);
						magEntryEquipStates[i] = EntryEquipState::kBothHands;
					}
					// LH
					else if (magEquippedLH)
					{
						SPDLOG_DEBUG
						(
							"[MIM] RefreshMagicMenuEquipState: {} equipped in left hand.",
							magForm->GetName()
						);
						magEntryEquipStates[i] = EntryEquipState::kLH;
					}
					// RH
					else if (magEquippedRH)
					{
						SPDLOG_DEBUG
						(
							"[MIM] RefreshMagicMenuEquipState: {} equipped in right hand.", 
							magForm->GetName()
						);
						magEntryEquipStates[i] = EntryEquipState::kRH;
					}
					// Voice
					else if (magEquippedVoice)
					{
						SPDLOG_DEBUG
						(
							"[MIM] RefreshMagicMenuEquipState: {} equipped in voice slot.",
							magForm->GetName()
						);
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

		shouldRefreshMenu = true;
	}

	void MenuInputManager::RefreshMenu() 
	{
		// Refresh the currently opened menu.

		SPDLOG_DEBUG("[MIM] RefreshMenu");
		if (containerMenu)
		{
			containerMenu->itemList->Update();
		}
		else if (barterMenu)
		{
			barterMenu->itemList->Update();
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

		RE::ActorPtr menuCoopActorPtr = Util::GetActorPtrFromHandle(menuCoopActorHandle);
		if (!menuCoopActorPtr)
		{
			return;
		}

		auto invChanges = menuCoopActorPtr->GetInventoryChanges();
		if (invChanges && invChanges->changed)
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
			SPDLOG_DEBUG("[MIM] SetEmulatedEventInfo: Current menu {} has context {}.",
				menuName, *currentMenu->inputContext);
			context = static_cast<RE::UserEvents::INPUT_CONTEXT_ID>
			(
				min(!RE::UserEvents::INPUT_CONTEXT_ID::kNone, !(*currentMenu->inputContext))
			);
			if (context != RE::UserEvents::INPUT_CONTEXT_ID::kNone)
			{
				a_bindInfoOut.eventName = controlMap->GetUserEventName
				(
					a_bindInfoOut.idCode, RE::INPUT_DEVICE::kGamepad, context
				);
				validEventNameFound = Hash(a_bindInfoOut.eventName) != Hash(""sv);
			}
		}

		// Fall back to menu mode context.
		if (!validEventNameFound) 
		{
			context = RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode;
			a_bindInfoOut.eventName = controlMap->GetUserEventName
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
				if (context != RE::UserEvents::INPUT_CONTEXT_ID::kNone)
				{
					a_bindInfoOut.eventName = controlMap->GetUserEventName
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
		// with valid event names for all controller inputs.
		if (!validEventNameFound) 
		{
			context = RE::UserEvents::INPUT_CONTEXT_ID::kGameplay;
			a_bindInfoOut.eventName = controlMap->GetUserEventName
			(
				a_bindInfoOut.idCode, RE::INPUT_DEVICE::kGamepad, context
			);
			validEventNameFound = Hash(a_bindInfoOut.eventName) != Hash(""sv);
		}

		// Should not happen, but bail here if there is still no valid event name.
		if (!validEventNameFound) 
		{
			SPDLOG_DEBUG
			(
				"[MIM] ERR: SetEmulatedInputEventInfo: "
				"Could not get event name for XInput button mask 0x{:X} and current menu '{}'.", 
				a_xMask, menuName
			);
			return false;
		}

		// Set valid context here.
		SPDLOG_DEBUG
		(
			"[MIM] SetEmulatedInputEventInfo: Chose context {}, event name {} for xMask 0x{:X}. "
			"Value, held time: {}, {}. Current event type: {}.",
			context, a_bindInfoOut.eventName, a_xMask,
			a_bindInfoOut.value,
			a_bindInfoOut.heldTimeSecs,
			currentMenuInputEventType
		);
		a_bindInfoOut.context = context;

		return true;
	}

	void MenuInputManager::SetMagicMenuFormsList()
	{
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

	void MenuInputManager::ToggleCoopPlayerMenuMode
	(
		const int32_t& a_controllerIDToSet, const int32_t& a_playerIDToSet
	)
	{
		// Toggle menu mode for the given player CID and PID.

		bool shouldEnter = a_controllerIDToSet != -1;
		if (shouldEnter) 
		{
			// Ensure that menu controller is available before starting/resuming the manager.
			XINPUT_STATE buttonState{ };
			ZeroMemory(&buttonState, sizeof(buttonState));
			if (XInputGetState(a_controllerIDToSet, &buttonState) != ERROR_SUCCESS)
			{
				SPDLOG_DEBUG
				(
					"[MIM] ERR: ToggleCoopPlayerMenuMode: "
					"Got invalid menu controller ID ({}). Exiting.",
					a_controllerIDToSet
				);
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
				// If not provided, get from the menu-controlling player.
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

		SPDLOG_DEBUG
		(
			"[MIM] ToggleCoopPlayerMenuMode: Performed state change request. "
			"MIM is now set to {}.", 
			shouldEnter ? "running" : "paused"
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
				auto invCounts = menuCoopActorPtr->GetInventoryCounts();
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

		SPDLOG_DEBUG("[MIM] UpdateMenuType: Menu name: {}", menuName);
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
			if (menuNameHash == Hash(RE::BarterMenu::MENU_NAME))
			{
				barterMenu = ui->GetMenu<RE::BarterMenu>();
			}
			else if (menuNameHash == Hash(RE::BookMenu::MENU_NAME))
			{
				bookMenu = ui->GetMenu<RE::BookMenu>();
			}
			else if (menuNameHash == Hash(RE::ContainerMenu::MENU_NAME))
			{
				containerMenu = ui->GetMenu<RE::ContainerMenu>();
			}
			else if (menuNameHash == Hash(RE::DialogueMenu::MENU_NAME))
			{
				dialogueMenu = ui->GetMenu<RE::DialogueMenu>();
			}
			else if (menuNameHash == Hash(RE::FavoritesMenu::MENU_NAME))
			{
				favoritesMenu = ui->GetMenu<RE::FavoritesMenu>();
			}
			else if (menuNameHash == Hash(RE::GiftMenu::MENU_NAME))
			{
				giftMenu = ui->GetMenu<RE::GiftMenu>();
			}
			else if (menuNameHash == Hash(RE::InventoryMenu::MENU_NAME))
			{
				inventoryMenu = ui->GetMenu<RE::InventoryMenu>();
			}
			else if (menuNameHash == Hash(RE::JournalMenu::MENU_NAME))
			{
				journalMenu = ui->GetMenu<RE::JournalMenu>();
			}
			else if (menuNameHash == Hash(RE::LockpickingMenu::MENU_NAME))
			{
				lockpickingMenu = ui->GetMenu<RE::LockpickingMenu>();
			}
			else if (menuNameHash == Hash(RE::MagicMenu::MENU_NAME))
			{
				magicMenu = ui->GetMenu<RE::MagicMenu>();
			}
			else if (menuNameHash == Hash(RE::MapMenu::MENU_NAME))
			{
				mapMenu = ui->GetMenu<RE::MapMenu>();
			}
			else if (menuNameHash == Hash(RE::SleepWaitMenu::MENU_NAME))
			{
				sleepWaitMenu = ui->GetMenu<RE::SleepWaitMenu>();
			}
		}

		// Will only be one of the following.
		if (barterMenu)
		{
			openedMenuType = SupportedMenu::kBarter;
		}
		else if (bookMenu)
		{
			openedMenuType = SupportedMenu::kBook;
		}
		else if (containerMenu)
		{
			openedMenuType = SupportedMenu::kContainer;
		}
		else if (dialogueMenu)
		{
			openedMenuType = SupportedMenu::kDialogue;
		}
		else if (favoritesMenu)
		{
			openedMenuType = SupportedMenu::kFavorites;
		}
		else if (giftMenu)
		{
			openedMenuType = SupportedMenu::kGift;
		}
		else if (inventoryMenu)
		{
			openedMenuType = SupportedMenu::kInventory;
		}
		else if (journalMenu)
		{
			openedMenuType = SupportedMenu::kJournal;
		}
		else if (lockpickingMenu)
		{
			openedMenuType = SupportedMenu::kLockpicking;
		}
		else if (magicMenu)
		{
			openedMenuType = SupportedMenu::kMagic;
		}
		else if (mapMenu)
		{
			openedMenuType = SupportedMenu::kMap;
		}
		else if (sleepWaitMenu)
		{
			openedMenuType = SupportedMenu::kSleepWaitMenu;
		}
		else if (ui && 
				 ui->GetMenu(GlobalCoopData::LOOT_MENU) &&
				 menuNameHash == Hash(GlobalCoopData::LOOT_MENU))
		{
			openedMenuType = SupportedMenu::kLoot;
		}
		else
		{
			// Use default menu control binds in all other cases.
			openedMenuType = SupportedMenu::kDefault;
		}

		return oldMenuType != openedMenuType;
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

			ClearRequests(p->controllerID);
		}
	}

	void MenuOpeningActionRequestsManager::ClearRequests(const int32_t& a_controllerID)
	{
		// Clear all menu opening action requests for the given player CID.

		if (a_controllerID <= -1 || a_controllerID >= menuOpeningActionRequests.size())
		{
			return;
		}

		menuOpeningActionRequests[a_controllerID].clear();
	}

	bool MenuOpeningActionRequestsManager::InsertRequest
	(
		const int32_t& a_controllerID, 
		InputAction a_fromAction, 
		SteadyClock::time_point a_timestamp,
		RE::BSFixedString a_reqMenuName,
		RE::ObjectRefHandle a_assocRefrHandle, 
		bool a_isExtRequest
	)
	{
		// Insert a menu opening action request for the given CID, 
		// associated action, menu, and form. 
		// Tag with timestamp.
		// Return true if successfullly inserted.

		if (a_controllerID <= -1 || a_controllerID >= menuOpeningActionRequests.size())
		{
			return false;
		}

		{
			std::unique_lock<std::mutex> lock(reqQueueMutexList[a_controllerID], std::try_to_lock);
			if (lock) 
			{
				auto& reqQueue = menuOpeningActionRequests[a_controllerID];
				// Remove oldest request if currently full.
				if (reqQueue.size() == maxCachedRequests)
				{
					reqQueue.pop_back();
				}

				SPDLOG_DEBUG
				(
					"[MIM] MenuOpeningActionRequestsManager: InsertRequest: "
					"Adding menu opening action request for CID {}: input action: {}, "
					"menu name: {}, associated form: {}",
					a_controllerID,
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
				SPDLOG_DEBUG
				(
					"[MIM] MenuOpeningActionRequestsManager: InsertRequest: "
					"Failed to obtain lock. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
				return false;
			}
		}

		return false;
	}

	int32_t MenuOpeningActionRequestsManager::ResolveMenuControllerID
	(
		const RE::BSFixedString& a_menuName, bool&& a_modifyReqQueue
	)
	{
		// Get controller ID for player that should control the given menu when it opens/closes.
		// Optionally, modify the menu requests queue for this player to re-order 
		// or clear out requests during the resolution process.

		SPDLOG_DEBUG
		(
			"[MIM] MenuOpeningActionRequestsManager: "
			"ResolveMenuControllerID: Menu: {}, modify requests queue: {}.", 
			a_menuName, a_modifyReqQueue
		);

		// Default to none.
		int32_t resolvedCID = -1;
		auto ui = RE::UI::GetSingleton();
		if (!ui) 
		{
			return resolvedCID;
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
			if (Settings::bUninitializedDialogueWithClosestPlayer && 
				a_menuName == RE::DialogueMenu::MENU_NAME)
			{
				if (glob.menuCID != -1)
				{
					resolvedCID = glob.menuCID;
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
								resolvedCID = p->controllerID;
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
			// Ignore max 3 second request lifetime for the LootMenu.
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
					// Insert for now. Will remove later after handling.
					if (a_modifyReqQueue) 
					{
						menuOpeningActionRequests[cid].emplace_front(currentReq);
					}

					float secsSinceReq = Util::GetElapsedSeconds(currentReq.timestamp);
					SPDLOG_DEBUG
					(
						"[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: "
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
							(ignoreReqExpiration || secsSinceReq < 3.0f) && 
							(
								chosenReqType != kExternal ||
								!secsSinceChosenReq.has_value() || 
								secsSinceReq > secsSinceChosenReq.value()
							)
						);
						if (isMaintainControlRequest || isValidExtRequest)
						{
							secsSinceChosenReq = secsSinceReq;
							resolvedCID = p->controllerID;
							SPDLOG_DEBUG
							(
								"[MIM] MenuOpeningActionRequestsManager: "
								"ResolveMenuControllerID: External request: "
								"{} is in control of {}. Should maintain control: {}.",
								p->coopActor->GetName(), a_menuName, isMaintainControlRequest
							);

							// Skip the rest of the requests if maintaining control.
							if (isMaintainControlRequest)
							{
								chosenReqType = kMaintainControl;
								if (a_modifyReqQueue) 
								{
									menuOpeningActionRequests[cid].pop_front();
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
					// Ignore requests that are older than 3 seconds.
					// Also look for a direct request if an indirect one is currently chosen.
					bool checkForOldestReq = 
					(
						(
							chosenReqType == kNone || 
							chosenReqType == kOldest || 
							chosenReqType == kNewestIndirect
						) &&
						(!isMessageBoxMenu) &&
						(ignoreReqExpiration || secsSinceReq < 3.0f) && 
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: BarterMenu: "
									"{} is in control of bartering with {}.",
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: BookMenu: "
									"{} is in control of reading {}.",
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
								refrPtr == p->coopActor
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: ContainerMenu: "
									"{} is in control of their inventory.", 
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
									SPDLOG_DEBUG
									(
										"[MIM] MenuOpeningActionRequestsManager: "
										"ResolveMenuControllerID: ContainerMenu: "
										"{} is in control of {}'s container menu.",
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
							// Need the menu CID to figure out which player's inventory 
							// to copy to P1 and need the sub menu furniture to get the menu CID, 
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: CraftingMenu: "
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: DialogueMenu: "
									"{} is in control of dialogue with {}.",
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: FavoritesMenu: "
									"{} is in control of menu.",
									p->coopActor->GetName()
								);
							}

							break;
						}
						case Hash(RE::GiftMenu::MENU_NAME.data(), RE::GiftMenu::MENU_NAME.size()):
						{
							// Wants to trade with another player.
							if (currentReq.fromAction == InputAction::kTradeWithPlayer)
							{
								setAsChosen = true;
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: GiftMenu: {} is in control of menu.",
									p->coopActor->GetName()
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: JournalMenu: "
									"{} is in control of menu.", 
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: InventoryMenu: "
									"{} is in control of menu.",
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: LevelUpMenu: "
									"{} is in control of menu.", 
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: LockpickingMenu: "
									"{} is in control of unlocking {}.",
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: MagicMenu: "
									"{} is in control of menu.", 
									p->coopActor->GetName()
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: MapMenu: {} is in control of menu.", 
									p->coopActor->GetName()
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: WaitMenu: {} is in control of menu.", 
									p->coopActor->GetName()
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
									SPDLOG_DEBUG
									(
										"[MIM] MenuOpeningActionRequestsManager: "
										"ResolveMenuControllerID: WaitMenu: "
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: StatsMenu: "
									"{} is in control of menu.", 
									p->coopActor->GetName()
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: TrainingMenu: "
									"{} is receiving training from {}.",
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: TweenMenu: "
									"{} is in control of menu.", 
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
							// the requesting menu control CID, 
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: {} (UIExtensions): "
									"{} is in control of menu.", 
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
							if (!isRequestedMenu || 
								!crosshairPickData || 
								currentReq.fromAction != InputAction::kMoveCrosshair)
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: {}: "
									"{} is in control of {}'s quick loot menu.",
									GlobalCoopData::LOOT_MENU, 
									p->coopActor->GetName(), 
									assocRefrPtr->GetName()
								);

								// Store which player will receive control of the LootMenu.
								glob.quickLootControlCID = p->controllerID;
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
								SPDLOG_DEBUG
								(
									"[MIM] MenuOpeningActionRequestsManager: "
									"ResolveMenuControllerID: {}: {} is in control of menu.", 
									GlobalCoopData::ENHANCED_HERO_MENU, p->coopActor->GetName()
								);
							}

							break;
						}
						default:
						{
							SPDLOG_DEBUG
							(
								"[MIM] MenuOpeningActionRequestsManager: "
								"ResolveMenuControllerID: FALLTHROUGH for {}.",
								a_menuName
							);
							break;
						}
						}

						if (setAsChosen)
						{
							secsSinceChosenReq = secsSinceReq;
							resolvedCID = p->controllerID;
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
						SPDLOG_DEBUG
						(
							"[MIM] MenuOpeningActionRequestsManager: "
							"ResolveMenuControllerID: {}: "
							"Check for newest req for {}.",
							a_menuName,
							p->coopActor->GetName()
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
							resolvedCID = p->controllerID;
							chosenReqType = directlyRequested ? kNewestDirect : kNewestIndirect;
							SPDLOG_DEBUG
							(
								"[MIM] MenuOpeningActionRequestsManager: "
								"ResolveMenuControllerID: {}: "
								"{} is in control of menu by {}.",
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
						SPDLOG_DEBUG
						(
							"[MIM] MenuOpeningActionRequestsManager: "
							"ResolveMenuControllerID: {}: "
							"Removed handled request (menu: {}, action: {}, refr: {}) for {}.",
							a_menuName,
							currentReq.reqMenuName,
							currentReq.fromAction,
							Util::HandleIsValid(currentReq.assocRefrHandle) ?
							currentReq.assocRefrHandle.get()->GetName() :
							"NONE",
							p->coopActor->GetName()
						);
						menuOpeningActionRequests[cid].pop_front();
					}

					// Move on to the next request.
					reqQueue.pop_back();
				}

				SPDLOG_DEBUG
				(
					"[MIM] MenuOpeningActionRequestsManager: "
					"ResolveMenuControllerID: Active request queue size is now {} "
					"for {} after processing.", 
					menuOpeningActionRequests[cid].size(),
					p->coopActor->GetName()
				);
			}
		}

		SPDLOG_DEBUG
		(
			"[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: "
			"Resolved CID from requests: {}", 
			resolvedCID
		);
		// If there are no valid requests to open/close the current supported menu,
		// give control to the last player who controlled open menus.
		// Or if P1's managers are not active, such as when the co-op camera is disabled,
		// give P1 control of menus.
		if (resolvedCID == -1 && GlobalCoopData::SUPPORTED_MENU_NAMES.contains(a_menuName)) 
		{
			// Always give P1 control of the RaceSex/Console Menus, 
			// since companion players should not customize P1 
			// and cannot control the keyboard anyways.
			bool givePreviousPlayerControl = 
			(
				(
					a_menuName != RE::Console::MENU_NAME && 
					a_menuName != RE::RaceSexMenu::MENU_NAME
				) && 
				(
					(glob.coopPlayers[glob.player1CID]->IsRunning()) ||
					(glob.supportedMenuOpen && glob.mim->IsRunning())
				)
			);
			if (givePreviousPlayerControl) 
			{
				SPDLOG_DEBUG
				(
					"[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: "
					"No valid requests to open supported menu {}, set to last menu CID: {}. "
					"Supported menus open: {}, data copied over: 0x{:X}.", 
					a_menuName,
					glob.prevMenuCID,
					glob.supportedMenuOpen.load(),
					*glob.copiedPlayerDataTypes
				);
				resolvedCID = glob.prevMenuCID;
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: "
					"No valid requests to open supported menu {} "
					"and P1's managers are inactive or the Console Menu is opening/closing, "
					"set to P1 CID: {}. " 
					"Supported menus open: {}, data copied over: 0x{:X}.", 
					a_menuName,
					glob.player1CID, 
					glob.supportedMenuOpen.load(),
					*glob.copiedPlayerDataTypes

				);
				resolvedCID = glob.player1CID;
			}
		}

		SPDLOG_DEBUG
		(
			"[MIM] MenuOpeningActionRequestsManager: ResolveMenuControllerID: "
			"Final resolved CID: {}, for menu {}.",
			resolvedCID,
			a_menuName
		);

		return resolvedCID;
	}
}

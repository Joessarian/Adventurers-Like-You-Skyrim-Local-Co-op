#pragma once
#include <mutex>
#include <thread>
#include <Enums.h>
#include <Player.h>

namespace ALYSLC 
{
	using SteadyClock = std::chrono::steady_clock;
	
	// Information on menu-specific binds used to send InputEvents and perform other actions.
	struct MenuBindInfo
	{
		MenuBindInfo();
		MenuBindInfo
		(
			RE::INPUT_DEVICE a_device,
			RE::BSFixedString a_eventName, 
			RE::UserEvents::INPUT_CONTEXT_ID a_context
		);

		// Device linked with this bind.
		// Should always be set to controller when sending emulated input events.
		RE::INPUT_DEVICE device;
		// Event name associated with this bind.
		// For examples, see the RE::UserEvents singleton.
		RE::BSFixedString eventName;
		// Menu input context linked with this bind.
		RE::UserEvents::INPUT_CONTEXT_ID context;
		// ID code for the button/thumbstick/trigger/other input source linked with this bind.
		uint32_t idCode;
		// Time the linked input source has been held for.
		float heldTimeSecs;
		// Indicates if the linked input source is pressed.
		// 0 when not pressed, 1 for fully pressed, (0, 1] for triggers.
		float value;
		// Time point at which the input source was first pressed.
		SteadyClock::time_point firstPressTP;
		// Event type to send when this bind is pressed/released.
		MenuInputEventType eventType;
	};

	// Holds information that links player actions to menu opening requests.
	// Used in determining which player should be given control of supported opening menus.
	struct MenuOpeningActionRequestsManager
	{
		struct MenuOpeningActionRequests
		{
			MenuOpeningActionRequests() :
				fromAction(InputAction::kNone),
				timestamp(SteadyClock::now()),
				reqMenuName(""sv),
				assocRefrHandle({}),
				isExtRequest(false)
			{ }

			MenuOpeningActionRequests
			(
				InputAction a_fromAction, 
				SteadyClock::time_point a_timestamp, 
				RE::BSFixedString a_reqMenuName,
				RE::ObjectRefHandle a_assocRefrHandle, 
				bool _isExtRequest
			) :
				fromAction(a_fromAction),
				timestamp(a_timestamp),
				reqMenuName(a_reqMenuName),
				assocRefrHandle(a_assocRefrHandle),
				isExtRequest(_isExtRequest)
			{ }

			// Action linked with this request.
			InputAction fromAction;
			// Request time stamp.
			SteadyClock::time_point timestamp;
			// Linked menu's name.
			RE::BSFixedString reqMenuName;
			// An associated refr's handle, ex. the activated refr for an activation request.
			RE::ObjectRefHandle assocRefrHandle;
			// From outside of the plugin, ex. from a script.
			bool isExtRequest;
		};

		MenuOpeningActionRequestsManager() :
			menuOpeningActionRequests
			(
				{ ALYSLC_MAX_PLAYER_COUNT, std::list<MenuOpeningActionRequests>({}) }
			)
		{ }

		// Clear out all requests for all active players.
		void ClearAllRequests();

		// Clear out all requests for the given player.
		void ClearRequests(const int32_t& a_controllerID);

		// Returns true if request was successfully inserted.
		bool InsertRequest
		(
			const int32_t& a_controllerID, 
			InputAction a_fromAction, 
			SteadyClock::time_point a_timestamp,
			RE::BSFixedString a_reqMenuName, 
			RE::ObjectRefHandle a_assocRefrHandle = RE::ObjectRefHandle(), 
			bool a_isExtRequest = false
		);

		// Determine which player should receive control of the opening menu 
		// and return that player's controller ID
		// or -1 if no valid player was found.
		// Can also keep the requests queue for each player intact 
		// and only compute the menu controller ID.
		// Request queues are traversed and cleared by default.
		int32_t ResolveMenuControllerID
		(
			const RE::BSFixedString& a_menuName, bool&& a_modifyReqQueue = true
		);

		// Max number of queued requests at any given time.
		// The oldest request is discarded if a new request comes in 
		// while the queue is at capacity.
		static inline const uint8_t maxCachedRequests = 5;
		// CID of the player requesting control of the Dialogue Menu 
		// while another player currently has control.
		static inline int32_t reqDialoguePlayerCID = -1;
		// Mutices for each player's menu requests queue.
		std::array<std::mutex, (size_t)ALYSLC_MAX_PLAYER_COUNT> reqQueueMutexList;
		// Mutex for players attempting to request control of the Dialogue Menu.
		std::mutex reqDialogueControlMutex;
		// Queued requests for each player. Inserted from newest to oldest, front to back.
		std::vector<std::list<MenuOpeningActionRequests>> menuOpeningActionRequests;
	};

	class MenuInputManager : public Manager
	{
	public:

		MenuInputManager();

		// Implements ALYSLC::Manager:
		void MainTask() override;
		void PrePauseTask() override;
		void PreStartTask() override;
		void RefreshData() override;
		const ManagerState ShouldSelfPause() override;
		const ManagerState ShouldSelfResume() override;

		//
		// Member funcs
		//
		
		// Signal to refresh equip state.
		inline void SignalRefreshMenuEquipState()
		{
			SPDLOG_DEBUG("[MIM] SignalRefreshMenuEquipState: Getting lock. (0x{:X})", 
				std::hash<std::jthread::id>()(std::this_thread::get_id()));
			{
				std::unique_lock<std::mutex> lock(equipEventMutex);
				SPDLOG_DEBUG
				(
					"[MIM] SignalRefreshMenuEquipState: "
					"Setting refresh equip state flag to true."
				);
				equipEventRefreshReq = true;
			}
		}

		// Reset menu control overlay data.
		inline void ResetPlayerMenuControlOverlay() 
		{
			pmcFadeInterpData->Reset();
		}

		// Draw outline around screen edges to indicate what player is controlling menus.
		// Overlay color matches the player's crosshair color.
		void DrawPlayerMenuControlOverlay();
		// Equip a quick slot item/spell for P1
		// and update the Favorites Menu entry list to reflect the change.
		void EquipP1QSForm();
		// Hotkey the currently selected favorited form, 
		// based on the menu-controlling player's RS orientation.
		void HotkeyFavoritedForm();
		// Check if P1's quick slot item/spell are still favorited, save their indices in the 
		// Favorites Menu entry list, and update the list to reflect their equip state.
		void InitP1QSFormEntries();
		// Set the opened menu atop the stack or remove a menu from the stack.
		// Will then update the current supported menu type.
		void SetOpenedMenu(const RE::BSFixedString& a_menuName, const bool& a_isOpened);
		// Start or stop the menu input manager for the given player.
		void ToggleCoopPlayerMenuMode
		(
			const int32_t& a_controllerIDToSet, const int32_t& a_playerIDToSet = -1
		);

		//
		// Members
		//

		// Menu event type to handle.
		MenuInputEventType currentMenuInputEventType;
		// Current controllable menu's type.
		SupportedMenu openedMenuType;
		// Player to receive gifted items from another non-P1 player
		// via transfer from P1 when the GiftMenu is open.
		// Set by script and cleared when the GiftMenu closes.
		RE::ActorHandle gifteePlayerHandle;
		// Handle for the co-op companion player controlling menus.
		RE::ActorHandle menuCoopActorHandle;
		// Name of the current topmost controllable menu.
		RE::BSFixedString menuName;
		// Handle for the container refr currently open in the Container Menu.
		RE::ObjectRefHandle menuContainerHandle;
		// Most recent opened menu is atop the stack.
		// Using a list to remove closing menus that may not be the most recently opened menu.
		std::list<RE::BSFixedString> menuNamesStack;
		// Set of co-op player-opened menu names' hashes.
		std::set<size_t> menuNamesHashSet;
		// Maps Favorites Menu item list indices (index of item in the backing item list)
		// to entry (displayed text) indices.
		std::unordered_map<uint32_t, uint32_t> favMenuIndexToEntryMap;
		// Entry equip state for each entry in the menu-controlling player's favorites list.
		// Indexed by entry position in the Favorites Menu's entry list.
		std::vector<EntryEquipState> favEntryEquipStates;
		// Equip states for each form in the cached magic forms list above.
		// Indexed by entry position in the magic forms list.
		std::vector<EntryEquipState> magEntryEquipStates;
		// Is the Container Menu opened and showing the player's inventory or not?
		bool isCoopInventory;
		// Controller ID for the co-op companion player controlling menus.
		// -1 when the manager is not active.
		// NOTE: Should never equal P1's CID.
		int32_t managerMenuCID;
		// Player ID for the co-op companion player controlling menus.
		// NOTE: Should never equal P1's player ID (0).
		int32_t managerMenuPlayerID;
		// Player ID to use when drawing the player menu control overlay.
		// Updated in DrawPlayerMenuControlOverlay().
		int32_t pmcPlayerID;
		// Number of menus currently handled by this manager.
		uint32_t managedCoopMenusCount;

	private:

		//
		// Member funcs
		//

		// Get the currently selected form from the Favorites Menu.
		inline RE::TESForm* GetSelectedFormFromFavoritesMenu() 
		{
			auto ui = RE::UI::GetSingleton(); 
			if (!ui || !ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME))
			{
				return nullptr;
			}

			auto favoritesMenu = ui->GetMenu<RE::FavoritesMenu>();
			if (!favoritesMenu || !favoritesMenu.get())
			{
				return nullptr;
			}

			auto view = favoritesMenu->uiMovie; 
			if (!view)
			{
				return nullptr;
			}

			RE::GFxValue selectedIndex;
			view->GetVariable
			(
				std::addressof(selectedIndex), 
				"_root.MenuHolder.Menu_mc.itemList.selectedEntry.index"
			);

			// Index in favorites list.
			uint32_t index = static_cast<uint32_t>(selectedIndex.GetNumber());
			if (index > favoritesMenu->favorites.size())
			{
				return nullptr;
			}

			return favoritesMenu->favorites[index].item;
		}

		// Get the currently selected item from the given item list.
		inline RE::ItemList::Item* GetSelectedItem(RE::ItemList* a_itemList) 
		{
			if (!a_itemList) 
			{
				return nullptr;
			}

			if (a_itemList->items.size() == 0) 
			{
				return nullptr;
			}

			return a_itemList->GetSelectedItem();
		}

		// Update menu control map based on the menu-controlling player's input.
		// Resolve a menu input event type to handle and then handle that event type.
		void CheckControllerInput();

		// Prints all menu event name/DXScancode combos 
		// for each menu input context and device type.
		void DebugPrintMenuBinds();

		// Ensure all players have the same known spells/shouts
		// and then find a matching spell for the currently selected item in the Magic Menu.
		RE::TESForm* GetSelectedMagicMenuSpell();

		// Perform menu-dependent action based on the current menu input event type.
		// Supported actions are:
		// 1. (Un)equip the requested form.
		// 2. Emulate P1 input event by sending an input event that gets processed
		// by the MenuControls hook, as if P1 had pressed the same button 
		// or moved the same analog stick on their controller.
		// 3. Take item(s) from the Container Menu.
		void HandleMenuEvent();

		// Transfer selected or all objects from the currently displayed container to P1.
		// A second transfer from P1 to the co-op companion player is performed in
		// CoopContainerChangedHandler::ProcessEvent(), since additional processing is required.
		void HandleLootRequest(bool&& a_takeAll);

		// When the Favorites Menu opens, refresh the player's equip state, 
		// set their favorited forms, and cache + 
		// update the equip states of each Favorites Menu entry.
		void InitFavoritesEntries();

		// When the Magic Menu opens, save a list of all known magic, 
		// and cache + update the equip states of each Magic Menu entry.
		void InitMagicMenuEquippedStates();

		// Level up the companion player's skill AV by consuming the given Enderal skillbook.
		// Provide notifications of success/failure.
		// Return true on success.
		bool PerformEnderalSkillLevelUp(RE::AlchemyItem* a_skillbook);

		// Refresh favorited cyclable spells list(s) for the player.
		void RefreshCyclableSpells() const;

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Barter Menu.
		void ProcessBarterMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Book Menu.
		void ProcessBookMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Container Menu.
		void ProcessContainerMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Dialogue Menu.
		void ProcessDialogueMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Favorites Menu.
		void ProcessFavoritesMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Gift Menu.
		void ProcessGiftMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Inventory Menu.
		void ProcessInventoryMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Loot Menu.
		void ProcessLootMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Magic Menu.
		void ProcessMagicMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data based on 
		// the given player button input in the Map Menu.
		void ProcessMapMenuButtonInput(const uint32_t& a_xMask);

		// Set menu input event type and accompanying event data for the currently opened menu 
		// based on left or right trigger input.
		void ProcessTriggerInput(const bool& a_isLT);

		// Update cached equip state and Favorites Menu item entries based on 
		// what items the player has equipped.
		void RefreshFavoritesMenuEquipState(bool&& a_updateCachedEquipState);

		// Update cached equip state and Magic Menu item entries based on 
		// what magic the player has equipped.
		void RefreshMagicMenuEquipState(bool&& a_updateCachedEquipState);

		// Refresh visible elements for the topmost supported menu.
		void RefreshMenu();

		// Send any queued emulated input events strung together when checking controller input.
		void SendQueuedInputEvents();

		// Given the XInput mask, map it to the corresponding game mask 
		// and check relevant menu contexts for a matching input event name.
		// Then update the given bind info with relevant data pertaining to
		// the device, event name to send, input id code, context, value, and held time.
		// This data is used to create emulated P1 input events to send.
		bool SetEmulatedInputEventInfo(const uint32_t& a_xMask, MenuBindInfo& a_bindInfoOut);

		// Set a list of all P1's known magic forms.
		void SetMagicMenuFormsList();

		// Initialize the menu-dependent control map.
		void SetMenuControlMap();

		// Based on the given selected consumable form and its index 
		// in the Favorites Menu's item list, append its count to the end of its entry.
		// Done for quick slot items which are consumed by co-op companions because the game
		// does not update the count automatically since the Favorites Menu shows P1's item
		// count for that consumable.
		void UpdateFavoritedConsumableCount(RE::TESForm* a_selectedForm, uint32_t a_selectedIndex);

		// Set handled menu type based on the current menu name atop the stack.
		bool UpdateMenuType();

		//
		// Members
		//

		// Length of quick slot item/spell prefix tag ('[*QSS*] ' or '[*QSI*] ').
		// Used when setting new prefix to denote a favorited item/spell 
		// as equipped in the quick slot.
		const uint8_t qsPrefixTagLength = std::string("[*QS_*] ").length();

		// Control map for P1.
		RE::ControlMap* controlMap;
		// Barter menu.
		RE::GPtr<RE::BarterMenu> barterMenu;
		// Book menu.
		RE::GPtr<RE::BookMenu> bookMenu;
		// Container menu.
		RE::GPtr<RE::ContainerMenu> containerMenu;
		// Dialogue menu.
		RE::GPtr<RE::DialogueMenu> dialogueMenu;
		// Favorites menu.
		RE::GPtr<RE::FavoritesMenu> favoritesMenu;
		// Gift menu.
		RE::GPtr<RE::GiftMenu> giftMenu;
		// Inventory menu.
		RE::GPtr<RE::InventoryMenu> inventoryMenu;
		// Journal menu.
		RE::GPtr<RE::JournalMenu> journalMenu;
		// Lockpicking menu.
		RE::GPtr<RE::LockpickingMenu> lockpickingMenu;
		// Magic menu.
		RE::GPtr<RE::MagicMenu> magicMenu;
		// Map menu.
		RE::GPtr<RE::MapMenu> mapMenu;
		// Container to equip/loot from.
		RE::ObjectRefHandle fromContainerHandle;
		// Form selected from current menu item list.
		RE::TESForm* selectedForm;

		// Refresh equip state on equip event mutex.
		std::mutex equipEventMutex;
		// Update opened menu data mutex.
		std::mutex openedMenuMutex;
		// Player menu control overlay interp data for fading the overlay.
		std::unique_ptr<TwoWayInterpData> pmcFadeInterpData;
		// Menu-dependent control map which maps the XInput masks of each controller input
		// to menu bind information.
		std::unordered_map<uint32_t, MenuBindInfo> menuControlMap;
		// List of (usually) all magic forms displayed in the Magic Menu.
		// NOTE: If a magic form is not found within the Magic Menu's own forms list 
		// or P1's known spells/shouts lists, it will not be added to this list.
		std::vector<RE::TESForm*> magFormsList;
		// Queued emulated P1 InputEvents to send once chained.
		std::vector<std::unique_ptr<RE::InputEvent* const>> queuedInputEvents;

		// Equip index to (un)equip selected item to.
		EquipIndex reqEquipIndex;
		// Menu button binds info for the currently tapped/held/released input 
		// that triggered an event.
		MenuBindInfo currentBindInfo;
		// Time point at which the last equip state delayed refresh request was made.
		SteadyClock::time_point lastEquipStateRefreshReqTP;

		// Delayed request to refresh equip state after attempting to equip an object.
		// Delayed so that failed equip requests can be reflected in the UI,
		// instead of updating the equip state before the item actually gets (un)equipped.
		bool delayedEquipStateRefresh;
		// Should refresh Favorites/Magic Menu equip state 
		// and cached equip data following an equip event.
		bool equipEventRefreshReq;
		// Is a new menu now on top of the menu stack?
		// Update control map and refresh data if so.
		bool newMenuAtopStack;
		// Has either placeholder spell been changed?
		bool placeholderMagicChanged;
		// Should refresh menus.
		bool shouldRefreshMenu;
		// Spell was (un)favorited.
		bool spellFavoriteStatusChanged;
	};
}

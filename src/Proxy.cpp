#include "Proxy.h"
#include <Controller.h>
#include <Enums.h>
#include <Events.h>
#include <GlobalCoopData.h>
#include <MenuInputManager.h>
#include <ModAPI.h>
#include <Player.h>
#include <Serialization.h>
#include <IPluginInterface.h>

namespace ALYSLC
{
	// Global co-op data used to help the proxy delegate papyrus function calls
	// to the corresponding plugin functions.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	//=============================================================================================
	// Initialization functions defined in proper order of execution.
	//=============================================================================================
 
	bool CoopLib::InitializeGlobalData(RE::StaticFunctionTag*, RE::BGSRefAlias* a_player1Ref)
	{
		// Initialize or re-assign global co-op data.
		// Called each time a save is loaded.

		SPDLOG_DEBUG("InitializeGlobalData.");
		// First time initialization.
		bool firstTimeInit = !glob.globalDataInit;
		auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (glob.globalDataInit) 
		{
			// P1 data may change on loading a save (if another player character's save is loaded).
			// Must also ensure the camera manager is not running on save load.

			// Reset P1's DID.
			// Will be automatically re-assigned on the first summoning after save load.
			glob.player1DID = -1;
			// Reset player ID requesting control of menus.
			glob.moarm->reqTransferMenuControlPlayerPID = -1;
			// Set player ref alias, which may have changed.
			glob.player1RefAlias = a_player1Ref;
			// Get P1, which may be a different character.
			glob.player1Actor.reset();
			glob.player1Actor = RE::ActorPtr(RE::PlayerCharacter::GetSingleton());
			// Set living and active players to 0 when not in co-op.
			glob.livingPlayers = glob.activePlayers = 0;
			// Reset QuickLoot menu-opening data.
			glob.quickLootControlPID = -1;
			glob.quickLootReqPID = -1;
			glob.reqQuickLootContainerHandle = RE::ObjectRefHandle();
			// Co-op camera set to paused and not waiting for toggle.
			glob.cam->SetWaitForToggle(false);
			glob.cam->ToggleCoopCamera(false);
			// Reset combat and camera shake state.
			glob.isCameraShakeActive = false;
			glob.isInCoopCombat = false;
			// Set as not summoning yet.
			glob.isSummoningPlayers = false;
			// Make sure the party is not flagged as wiped.
			glob.partyWiped = false;
		}
		else 
		{
			// Only init global data once per play session (on save load).
			GlobalCoopData::InitializeGlobalCoopData(a_player1Ref);
		}

		// Import all settings after initializing co-op data.
		ALYSLC::Settings::ImportAllSettings();
		// Re-register for script events.
		GlobalCoopData::UnregisterEvents();
		GlobalCoopData::RegisterEvents();
		// Reset crosshair text and position.
		GlobalCoopData::SetCrosshairText(true);
		// Reset supported menu open state because it won't reset
		// properly if the previous co-op session ended while a supported menu was open.
		GlobalCoopData::ResetMenuState();
		// Re-enable any controls for P1 that might have been disabled.
		Util::ToggleAllControls(true);
		// Clear any lingering queued input events.
		for (auto& ptr : glob.reqInputEvents)
		{
			ptr.release();
		}

		glob.reqInputEvents.clear();
		// Reset to the default third person camera orientation, 
		// just in case the game was saved while the co-op cam was active.
		Util::ResetTPCamOrientation();
		if (p1) 
		{
			// NOTE: 
			// The game fails to save P1's perks properly at times,
			// either clearing all of them, or only saving the perks unlocked by P1 
			// and not by any other player.
			// I have yet to find a reason why it does this or find a direct solution,
			// so the current workaround is to import P1's perks
			// to ensure that they can access their saved perks, even outside of co-op.
			// Please note that if the mod is uninstalled, 
			// P1 will have to respec all their perks manually,
			// as the function below will not fire to import all the serialized perks.
			GlobalCoopData::ImportUnlockedPerks(p1);
		}

		auto ui = RE::UI::GetSingleton();
		if (ui && !ui->IsMenuOpen(DebugOverlayMenu::MENU_NAME))
		{
			// Open the ALYSLC overlay if it isn't open already.
			SPDLOG_DEBUG("ALYSLC overlay not open. Opening.");
			DebugOverlayMenu::Load();
		}

		// Stop combat without removing bounties to prevent aggro on load 
		// from previously pacified neutral factions.
		Util::StopCombatOnPlayerAndAllies();

		return firstTimeInit;
	}

	std::vector<std::uint32_t> CoopLib::GetConnectedInputDeviceIDs(RE::StaticFunctionTag*)
	{
		// Setup input device data for all connected devices and return a list of device IDs
		// for all active devices. P1's DID is always first.

		SPDLOG_DEBUG("GetConnectedInputDeviceIDs");
		if (glob.globalDataInit) 
		{
			return glob.cdh->SetupConnectedInputDevices();
		}
		else
		{
			return std::vector<std::uint32_t>();
		}
	}

	bool CoopLib::InitializeCoopPlayers
	(
		RE::StaticFunctionTag*, 
		uint32_t a_numCompanions,
		std::vector<uint32_t> a_deviceIDs, 
		std::vector<RE::Actor*> a_coopActors
	)
	{
		// Preconditions:
		// Player 1 is always determined by the first index's elements in both lists.
		// Device ID and actor lists are contiguously populated (all null elements at the end)
		// and elements at the same indices are linked to one another
		// (Ex. If list index 1 has a device ID of 2 and an actor 'Player', 
		// then the actor 'Player' is controlled by the input device with ID 2.)
		// 
		// Initializes/updates all co-op players with the given data.
		// Returns true if a co-op session was initialized successfully.

		SPDLOG_DEBUG("InitializeCoop");
		// No global co-op data assigned, so we can't start co-op.
		if (!glob.globalDataInit) 
		{
			return false;
		}
		
		// Set P1 essential designation only once before initializing any player managers.
		if (!glob.allPlayersInit)
		{
			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			if (p1)
			{
				glob.p1IsEssential = p1->IsEssential();
				SPDLOG_DEBUG
				(
					"P1 is essential before initializing all players: {}.", p1->IsEssential()
				);
			}
		}

		// Reset living and active players count before constructing/updating co-op players.
		glob.livingPlayers = glob.activePlayers = 0;

		// P1's DID must be set before starting co-op.
		if (glob.player1DID == -1) 
		{
			RE::DebugMessageBox
			(
				"[ALYSLC]\nPlayer 1's device ID has not been assigned "
				"before starting co-op.\n"
				"Please try summoning again or assign Player 1's controller ID "
				"through the Debug Menu before summoning:\n"
				"1. Hold the 'Pause/Journal' bind.\n"
				"2. Press and release the 'Wait' bind.\n"
				"3. Select 'Miscellaneous Options'.\n"
				"4. Select 'Assign Player 1 Controller ID'."
			);
			return false;
		}

		SPDLOG_DEBUG
		(
			"Device IDs vector length: {}, number of companion players: {}.", 
			a_deviceIDs.size(), a_numCompanions
		);
		SPDLOG_DEBUG
		(
			"Device IDs: {}, {}, {}, {}",
			a_deviceIDs.size() > 0 ? a_deviceIDs[0] : -1, 
			a_deviceIDs.size() > 1 ? a_deviceIDs[1] : -1,
			a_deviceIDs.size() > 2 ? a_deviceIDs[2] : -1, 
			a_deviceIDs.size() > 3 ? a_deviceIDs[3] : -1
		);
		SPDLOG_DEBUG
		(
			"Co-op actors: {}, {}, {}, {}",
			(a_coopActors[0]) ? a_coopActors[0]->GetName() : "None",
			(a_coopActors[1]) ? a_coopActors[1]->GetName() : "None",
			(a_coopActors[2]) ? a_coopActors[2]->GetName() : "None",
			(a_coopActors[3]) ? a_coopActors[3]->GetName() : "None"
		);

		// Create 4 co-op players.
		// Subsequent calls to initialize will reuse the co-op player objects
		// by simply updating the co-op actor, device ID,
		// and refreshing data that should be updated on re-summoning.
		// Assign co-op players based on their player IDs [0, 3].
		// NOTE:
		// Player 1 always has a player ID of 0.
		for (uint32_t playerID = 0; playerID < ALYSLC_MAX_PLAYER_COUNT; ++playerID)
		{
			// Instantiate co-op player if their input device is active.
			if (playerID < a_deviceIDs.size() && a_deviceIDs[playerID] != -1)
			{
				if (!a_coopActors[playerID])
				{
					RE::DebugMessageBox
					(
						"[ALYSLC]\nERROR: "
						"Previously active character(s) were likely not fully dismissed yet "
						"and a chosen character is not available.\n"
						"Please wait a little before resummoning, but if the issue persists, "
						"send the mod author a complaint about the absolute state of the mod."
					);
					SPDLOG_ERROR
					(
						"[P{}] should be active at device ID list index {}. Aborting setup.",
						playerID + 1, 
						playerID
					);
					return false;
				}

				SPDLOG_DEBUG
				(
					"[P{}] active at device ID list index {}: {}. Device ID: {}.",
					playerID + 1, 
					playerID, 
					a_coopActors[playerID] ?
					a_coopActors[playerID]->GetName() : 
					"NONE",
					a_deviceIDs[playerID]
				);

				// Update serialization key for this player, which may have changed
				// if the mod load order has been modified since the last save.
				bool succ = GlobalCoopData::UpdatePlayerSerializationIDs
				(
					a_coopActors[playerID]
				);
				// If not successful, we could not get and update this player's serialized data, 
				// so stop initializing co-op.
				if (!succ)
				{
					RE::DebugMessageBox
					(
						fmt::format
						(						
							"[ALYSLC]\nERROR: "
							"Failed to retrieve {}'s saved data.\n"
							"All saved player data has been fully reset prior to starting co-op.\n"
							"Please re-customize and respec all characters.", 
							a_coopActors[playerID] ? 
							a_coopActors[playerID]->GetName() :
							"NONE"
						).c_str()
					);
					Serialization::SetDefaultRetrievedData();
				}

				// Construct a new player or modify the current one at the same index,
				// depending on if all players were already initialized.
				if (glob.allPlayersInit)
				{
					SPDLOG_DEBUG
					(
						"Updating coop player '{}'.",
						a_coopActors[playerID] ?
						a_coopActors[playerID]->GetName() : 
						"NONE"
					);

					// Simply update the current co-op player
					// to reflect the new data received.
					glob.coopPlayers[playerID]->UpdateCoopPlayer
					(
						a_deviceIDs[playerID], 
						playerID,
						a_coopActors[playerID]
					);
				}
				else
				{
					SPDLOG_DEBUG
					(
						"Constructing new coop player '{}'.", 
						a_coopActors[playerID] ?
						a_coopActors[playerID]->GetName() : 
						"NONE"
					);

					// Construct active player at index given by player ID.
					glob.coopPlayers[playerID] = std::make_shared<CoopPlayer>
					(
						a_deviceIDs[playerID],
						playerID,
						a_coopActors[playerID]
					);
				}

				// Increment number of active, living players.
				++glob.activePlayers;
				++glob.livingPlayers;
			}
			else
			{
				SPDLOG_DEBUG("[P{}] inactive", playerID + 1);
				// Construct inactive player to clear out all previous data.
				glob.coopPlayers[playerID] = std::make_shared<CoopPlayer>(-1, -1, nullptr);
			}
		}

		// Initialize all sub-managers after construction.
		for (uint8_t i = 0; i < glob.coopPlayers.size(); ++i)
		{
			const auto& p = glob.coopPlayers[i];
			if (p->isActive) 
			{
				// Since the player manager is itself a member of each sub-manager 
				// for ease of access to all other player sub-managers,
				// initialize all sub-managers after full construction of the player manager.
				// Player manager shared pointer should have a use count of 5 
				// (1 global + 1 per manager X 4 managers) after sub-manager construction.
				p->em->Initialize(p);
				p->mm->Initialize(p);
				p->pam->Initialize(p);
				p->tm->Initialize(p);
			}
		}

		SPDLOG_DEBUG("Players this session: {}", glob.activePlayers);
		// First initialization.
		if (!glob.allPlayersInit)
		{
			// Notify P1 of how they can obtain menu input control with the keyboard + mouse
			// while the co-op camera is inactive and another player is controlling menus.
			// Implemented as a failsafe to prevent getting locked out 
			// of interacting with menus, while also not interrupting
			// the companion player's menu control with keypresses or mouse movement.
			GlobalCoopData::SetMenuPlayerIDs(0);
			RE::DebugMessageBox
			(
				"[ALYSLC]\n"
				"While the co-op camera is inactive and Player 1 is not controlling menus, "
				"keep 'Left Control' held before pressing any other keys or moving the mouse "
				"to enable keyboard and mouse controls in the menu. " 
				"'Right Control' will perform the same action as 'Left Control' "
				"while Player 1 is not controlling menus."
			);
		}

		// All players have now been initialized for the first time.
		glob.allPlayersInit = true;
		return true;
	}

	//=============================================================================================
	// Post-summoning Papyrus functions listed in alphabetical order
	//=============================================================================================

	void CoopLib::ChangeCoopSessionState(RE::StaticFunctionTag*, bool a_shouldStart) 
	{
		// Start or stop a co-op session by starting/pausing all active players' managers 
		// and synchronizing actor values, perks, and items.

		SPDLOG_DEBUG("{} session.",a_shouldStart ? "Starting" : "Ending");
		if (glob.globalDataInit && glob.allPlayersInit) 
		{
			// Enable P1's controls and saving just to be safe.
			SKSE::GetTaskInterface()->AddTask
			(
				[]() 
				{
					auto controlMap = RE::ControlMap::GetSingleton();
					controlMap->lock.Lock();
					controlMap->ToggleControls(RE::ControlMap::UEFlag::kActivate, true);
					controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, true);
					controlMap->ToggleControls(RE::ControlMap::UEFlag::kPOVSwitch, true);
					controlMap->ToggleControls(RE::ControlMap::UEFlag::kMenu, true);
					controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, true);
					controlMap->lock.Unlock();
					// Re-enable saving too if P1 is not dead.
					auto p1 = RE::PlayerCharacter::GetSingleton();
					if (p1 &&
						!p1->IsDead() && 
						*glob.copiedPlayerDataTypes == CopyablePlayerDataTypes::kNone)
					{
						p1->byCharGenFlag = RE::PlayerCharacter::ByCharGenFlag::kNone;
					}
				}
			);

			glob.coopSessionActive = a_shouldStart;
			for (const auto& p : glob.coopPlayers) 
			{
				if (!p || !p->isActive) 
				{
					continue;
				}

				if (a_shouldStart)
				{
					// Make sure the player is not paralyzed either (from being downed).
					p->coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					// Register the player for script events 
					// and then signal all their managers to resume.
					p->RegisterEvents();
					p->RequestStateChange(ManagerState::kRunning);
				}
				else 
				{
					// Signal all player managers to pause and await data refresh
					// before unregistering the player for script events.
					p->RequestStateChange(ManagerState::kAwaitingRefresh);
					p->UnregisterEvents();
				}
			}
			
			// Give all accumulated party-wide shared items to P1.
			GlobalCoopData::GivePartyWideItemsToP1();
			// Modify the level XP gained per skill level up to scale inversely
			// with the number of active players.
			GlobalCoopData::ModifyXPPerSkillLevelMult(a_shouldStart);
			// Turn off god mode for everyone.
			GlobalCoopData::ToggleGodModeForAllPlayers(false, false);
			// Sync shared AVs, perks, Legendary levelings, and scale companion player's skill AVs.
			GlobalCoopData::SyncSharedSkillAVs();
			GlobalCoopData::SyncSharedPerks();
			GlobalCoopData::SyncSharedLegendaryLevelingCounts();
			GlobalCoopData::PerformInitialAVAutoScaling();
			GlobalCoopData::RescaleActivePlayerAVs();
			// Set or restore XP threshold.
			GlobalCoopData::ModifyLevelUpXPThreshold(glob.coopSessionActive);
			// Reset crosshair text.
			GlobalCoopData::SetCrosshairText(true);
			// Load debug overlay menu to show crosshairs/other UI elements.
			DebugOverlayMenu::Load();

			SPDLOG_DEBUG("Co-op session has now {}.", a_shouldStart ? "started" : "ended");
		}
		else
		{ 
			SPDLOG_ERROR
			(
				"Cannot start or stop co-op session. "
				"Global data not initialized: {}, all players not initialized: {}", 
				!glob.globalDataInit, !glob.allPlayersInit
			);
			glob.coopSessionActive = false;
		}

		// Lastly, reset menu DIDs/PIDs.
		if (glob.globalDataInit) 
		{
			glob.lastResolvedMenuPID = 
			glob.menuPID = 
			glob.prevMenuPID = 
			glob.mim->managerMenuDID = -1;
			glob.mim->managerMenuPID = -1;
			glob.mim->pmcPID = 0;
			// Clear all menu opening requests.
			glob.moarm->ClearAllRequests();
		}
	}

	void CoopLib::EnableCoopEntityCollision(RE::StaticFunctionTag*) 
	{
		// Toggle collision on or off for all loaded active players.

		SPDLOG_DEBUG("EnableCoopEntityCollision.");
		if (!glob.globalDataInit) 
		{
			return;
		}

		for (const auto playerActor : glob.coopEntityBlacklist)
		{
			if (!playerActor || playerActor->IsDisabled() || !playerActor->Is3DLoaded())
			{
				continue;
			}
				
			SPDLOG_DEBUG("{}.", playerActor->GetName());
			Util::EnableCollisionForActor(playerActor.get());
		}
	}

	std::vector<RE::TESForm*> CoopLib::GetAllAppearancePresets
	(
		RE::StaticFunctionTag*, RE::TESRace* a_race, bool a_female
	)
	{ 
		// Get all actor base NPC appearance presets, narrowed down by race and sex.

		std::vector<RE::TESForm*> npcList{ };
		auto dataHandler = RE::TESDataHandler::GetSingleton(); 
		if (!dataHandler || !a_race)
		{
			return npcList;
		}

		const auto& npcForms = dataHandler->GetFormArray(RE::FormType::NPC);
		for (const auto npcForm : npcForms)
		{
			auto npc = npcForm ? npcForm->As<RE::TESNPC>() : nullptr;
			if (!npc || !npc->race)
			{
				continue;
			}

			// Of the same race and sex as requested.
			bool sameRaceAndSex = 
			(
				(npc->race == a_race) &&
				(
					(
						a_female && 
						npc->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kFemale)
					) ||
					(
						!a_female &&
						npc->actorData.actorBaseFlags.none(RE::ACTOR_BASE_DATA::Flag::kFemale)
					)
				)
			);
			// Ignore actorbases with the player keyword, ex. P1 and companion player actorbases.
			bool isPlayerBase = npc->HasKeywordByEditorID("PlayerKeyword");
			if (sameRaceAndSex && !isPlayerBase)
			{
				npcList.emplace_back(npcForm);
			}
		}

		SPDLOG_DEBUG
		(
			"{} playable NPC forms with race {} (0x{:X}) and {} sex.", 
			npcList.size(),
			a_race->GetName(),
			a_race->formID,
			a_female ? "female" : "male"
		);
		// Sort by name (A-Z).
		std::sort
		(
			npcList.begin(), npcList.end(), 
			[](const RE::TESForm* a_lhs, const RE::TESForm* a_rhs) 
			{ 
				auto lName = 
				(
					strlen(a_lhs->GetName()) == 0 ? a_lhs->GetFormEditorID() : a_lhs->GetName()
				);
				auto rName = 
				(
					strlen(a_rhs->GetName()) == 0 ? a_rhs->GetFormEditorID() : a_rhs->GetName()
				);
				return strcmp(lName, rName) < 0; 
			}
		);

		return npcList;
	}

	std::vector<RE::TESForm*> CoopLib::GetAllClasses(RE::StaticFunctionTag*)
	{
		// Get all usable player classes.

		std::vector<RE::TESForm*> classList{ };
		auto dataHandler = RE::TESDataHandler::GetSingleton(); 
		if (!dataHandler)
		{
			return classList;
		}

		const auto& classForms = dataHandler->GetFormArray(RE::FormType::Class);
		for (const auto classForm : classForms)
		{
			if (!classForm)
			{
				continue;
			}

			classList.emplace_back(classForm);
		}

		SPDLOG_DEBUG("{} playable class forms.", classList.size());
		// Sort by name (A-Z).
		std::sort
		(
			classList.begin(), classList.end(), 
			[](const RE::TESForm* a_lhs, const RE::TESForm* a_rhs) 
			{
				auto lName = 
				(
					strlen(a_lhs->GetFormEditorID()) == 0 ? 
					a_lhs->GetName() :
					a_lhs->GetFormEditorID()
				);
				auto rName = 
				(
					strlen(a_rhs->GetFormEditorID()) == 0 ?
					a_rhs->GetName() : 
					a_rhs->GetFormEditorID()
				);
				return strcmp(lName, rName) < 0;
			}
		);

		return classList;
	}

	std::vector<RE::BSFixedString> CoopLib::GetAllCyclableEmoteIdleEvents(RE::StaticFunctionTag*)
	{
		// Get all assignable cyclable emote idle event names.

		SPDLOG_DEBUG("GetAllCyclableEmoteIdleEvents.");
		return ALYSLC::Settings::sEmoteIdlesList;
	}

	std::vector<RE::TESForm*> CoopLib::GetAllSelectableRaces
	(
		RE::StaticFunctionTag*, int32_t a_selectableRaceTypeFilter
	)
	{
		// Get all assignable races based on the given filter.

		std::vector<RE::TESForm*> raceList{ };
		SelectableRaceType filter = SelectableRaceType::kAll;
		if (a_selectableRaceTypeFilter >= 0 && 
			a_selectableRaceTypeFilter < !SelectableRaceType::kTotal) 
		{
			filter = static_cast<SelectableRaceType>(a_selectableRaceTypeFilter);
		}
		else
		{
			filter = SelectableRaceType::kPlayable;
		}

		auto dataHandler = RE::TESDataHandler::GetSingleton(); 
		if (!dataHandler) 
		{
			return raceList;
		}
		
		if (filter == SelectableRaceType::kAll) 
		{
			// No restrictions, get 'em all.
			const auto& raceForms = dataHandler->GetFormArray(RE::FormType::Race);
			for (const auto raceForm : raceForms)
			{
				if (!raceForm)
				{
					continue;
				}

				raceList.emplace_back(raceForm);
			}
		}
		else if (filter == SelectableRaceType::kHasNPCKeyword) 
		{
			// Must have the NPC keyword.
			const auto& raceForms = dataHandler->GetFormArray(RE::FormType::Race);
			for (const auto raceForm : raceForms)
			{
				if (!raceForm)
				{
					continue;
				}

				if (raceForm->As<RE::TESRace>()->HasKeyword(glob.npcKeyword))
				{
					raceList.emplace_back(raceForm);
				}
			}
		}
		else if (filter == SelectableRaceType::kPlayable)
		{
			// Must have the playable flag.
			const auto& raceForms = dataHandler->GetFormArray(RE::FormType::Race);
			for (const auto raceForm : raceForms)
			{
				if (!raceForm)
				{
					continue;
				}

				if (raceForm->As<RE::TESRace>()->GetPlayable())
				{
					raceList.emplace_back(raceForm);
				}
			}
		}
		else if (filter == SelectableRaceType::kUsedByAnyActorBase)
		{
			// Must be used as a race for at least one actor base.
			std::set<RE::TESForm*> raceSet{ };
			const auto& npcForms = dataHandler->GetFormArray(RE::FormType::NPC);
			for (const auto npcForm : npcForms)
			{
				if (!npcForm)
				{
					continue;
				}

				auto npc = npcForm->As<RE::TESNPC>();
				if (npc && npc->race)
				{
					raceSet.insert(npc->race);
				}
			}

			for (const auto raceForm : raceSet) 
			{
				if (!raceForm)
				{
					continue;
				}

				raceList.emplace_back(raceForm);
			}
		}
		else
		{
			// Must be used as a race for at least one NPC-keyword actor base.
			std::set<RE::TESForm*> raceSet{};
			const auto& npcForms = dataHandler->GetFormArray(RE::FormType::NPC);
			for (const auto npcForm : npcForms)
			{
				if (!npcForm)
				{
					continue;
				}

				auto npc = npcForm->As<RE::TESNPC>();
				if (npc && npc->race && npc->race->HasKeyword(glob.npcKeyword))
				{
					raceSet.insert(npc->race);
				}
			}

			for (const auto raceForm : raceSet)
			{
				if (!raceForm)
				{
					continue;
				}

				raceList.emplace_back(raceForm);
			}
		}

		SPDLOG_DEBUG("{} playable race forms.", raceList.size());
		// Sort by name (A-Z).
		std::sort
		(
			raceList.begin(), raceList.end(), 
			[](const RE::TESForm* a_lhs, const RE::TESForm* a_rhs)
			{
				auto lName = 
				(
					strlen(a_lhs->GetName()) == 0 ? a_lhs->GetFormEditorID() : a_lhs->GetName()
				);
				auto rName = 
				(
					strlen(a_rhs->GetName()) == 0 ? a_rhs->GetFormEditorID() : a_rhs->GetName()
				);
				return strcmp(lName, rName) < 0;
			}
		);

		return raceList;
	}

	std::vector<RE::TESForm*> CoopLib::GetAllVoiceTypes(RE::StaticFunctionTag*, bool a_female)
	{
		// Get all selectable male/female voice types.

		std::vector<RE::TESForm*> voiceTypeList{ };
		auto dataHandler = RE::TESDataHandler::GetSingleton(); 
		if (!dataHandler)
		{
			return voiceTypeList;
		}

		const auto& voiceTypeForms = dataHandler->GetFormArray(RE::FormType::VoiceType);
		for (const auto voiceTypeForm : voiceTypeForms)
		{
			if (!voiceTypeForm || !voiceTypeForm->As<RE::BGSVoiceType>())
			{
				continue;
			}

			bool isFemaleVoice = 
			(
				voiceTypeForm->As<RE::BGSVoiceType>()->data.flags.all
				(
					RE::VOICE_TYPE_DATA::Flag::kFemale
				)
			);
			// Match with sex.
			if ((a_female && isFemaleVoice) || (!a_female && !isFemaleVoice))
			{
				voiceTypeList.emplace_back(voiceTypeForm);
			}
		}

		SPDLOG_DEBUG
		(
			"{} usable {} voice type forms.", voiceTypeList.size(), a_female ? "female" : "male"
		);
		// Sort by name (A-Z).
		std::sort
		(
			voiceTypeList.begin(), voiceTypeList.end(), 
			[](const RE::TESForm* a_lhs, const RE::TESForm* a_rhs) 
			{
				auto lName = 
				(
					strlen(a_lhs->GetName()) == 0 ? a_lhs->GetFormEditorID() : a_lhs->GetName()
				);
				auto rName = 
				(
					strlen(a_rhs->GetName()) == 0 ? a_rhs->GetFormEditorID() : a_rhs->GetName()
				);
				return strcmp(lName, rName) < 0;
			}
		);

		return voiceTypeList;
	}

	std::vector<RE::Actor*> CoopLib::GetCompanionPlayerCharacters(RE::StaticFunctionTag *)
	{
		// Get a list of all playable companion players' characters.
		// Sent to script because the actor/objectrefr pointers got invalidated 
		// when stored in a FormList/Array property sometimes. I don't even know anymore.

		SPDLOG_DEBUG("");
		auto dataHandler = RE::TESDataHandler::GetSingleton(); 
		if (glob.globalDataInit && !glob.coopEntityBlacklist.empty())
		{
			// Co-op companion player actors.
			// Skip index 0 which is P1.
			return std::vector<RE::Actor*>
			(
				{
					glob.coopEntityBlacklist[1].get(),
					glob.coopEntityBlacklist[2].get(),
					glob.coopEntityBlacklist[3].get(),
					glob.coopEntityBlacklist[4].get(),
					glob.coopEntityBlacklist[5].get(),
					glob.coopEntityBlacklist[6].get(),
					glob.coopEntityBlacklist[7].get(),
					glob.coopEntityBlacklist[8].get(),
					glob.coopEntityBlacklist[9].get()
				}
			);
		}
		else if (dataHandler)
		{
			// Actors that are blacklisted from selection via targeting.
			return std::vector<RE::Actor*>
			(
				{
					dataHandler->LookupForm<RE::Actor>
					(
						GlobalCoopData::PLAYER_CHARACTER_FIDS[1], GlobalCoopData::PLUGIN_NAME
					),
					dataHandler->LookupForm<RE::Actor>
					(
						GlobalCoopData::PLAYER_CHARACTER_FIDS[2], GlobalCoopData::PLUGIN_NAME
					),
					dataHandler->LookupForm<RE::Actor>
					(
						GlobalCoopData::PLAYER_CHARACTER_FIDS[3], GlobalCoopData::PLUGIN_NAME
					),
					dataHandler->LookupForm<RE::Actor>
					(
						GlobalCoopData::PLAYER_CHARACTER_FIDS[4], GlobalCoopData::PLUGIN_NAME
					),
					dataHandler->LookupForm<RE::Actor>
					(
						GlobalCoopData::PLAYER_CHARACTER_FIDS[5], GlobalCoopData::PLUGIN_NAME
					),
					dataHandler->LookupForm<RE::Actor>
					(
						GlobalCoopData::PLAYER_CHARACTER_FIDS[6], GlobalCoopData::PLUGIN_NAME
					),
					dataHandler->LookupForm<RE::Actor>
					(
						GlobalCoopData::PLAYER_CHARACTER_FIDS[7], GlobalCoopData::PLUGIN_NAME
					),
					dataHandler->LookupForm<RE::Actor>
					(
						GlobalCoopData::PLAYER_CHARACTER_FIDS[8], GlobalCoopData::PLUGIN_NAME
					),
					dataHandler->LookupForm<RE::Actor>
					(
						GlobalCoopData::PLAYER_CHARACTER_FIDS[9], GlobalCoopData::PLUGIN_NAME
					)
				}
			);
		}
		else
		{
			SPDLOG_ERROR("ERR: Could not get data handler to look up player characters.");
		}

		return{ };
	}

	std::vector<RE::BSFixedString> CoopLib::GetFavoritedEmoteIdles
	(
		RE::StaticFunctionTag*, int32_t a_playerID
	)
	{
		// Get list of cyclable emote idle event names assigned by the given player.

		SPDLOG_DEBUG("PID {}.", a_playerID);
		std::vector<RE::BSFixedString> favoritedEmoteIdles{ };
		if (glob.allPlayersInit && 
			a_playerID > -1 &&
			a_playerID < ALYSLC_MAX_PLAYER_COUNT && 
			glob.coopPlayers[a_playerID]->isActive)
		{
			const auto& p = glob.coopPlayers[a_playerID];
			for (auto i = 0; i < p->em->favoritedEmoteIdles.size(); ++i)
			{
				favoritedEmoteIdles.emplace_back(p->em->favoritedEmoteIdles[i]);
			}
		}
		else
		{
			// Return the default list if the player PID is invalid.
			for (auto i = 0; i < GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS.size(); ++i)
			{
				favoritedEmoteIdles.emplace_back
				(
					GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS[i]
				);
			}
		}

		return favoritedEmoteIdles;
	}
	
	void CoopLib::RequestMenuControl
	(
		RE::StaticFunctionTag*,
		int32_t a_deviceID,
		int32_t a_playerID, 
		RE::BSFixedString a_menuName
	)
	{
		// Request control of the given menu for the given player.
		// Reset menu PIDs if the given PID is -1.

		if (!glob.globalDataInit ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT) 
		{
			return;
		}

		// Must both be valid assigned IDs.
		if (a_deviceID > -1 && a_playerID > -1) 
		{
			// Send a request to resolve later if during co-op session.
			// Set directly and stop/start menu input manager when out of co-op.
			if (glob.coopSessionActive)
			{
				bool succ = glob.moarm->InsertRequest
				(
					a_playerID,
					InputAction::kNone, 
					SteadyClock::now(), 
					a_menuName, 
					RE::ObjectRefHandle(),
					true
				);
				SPDLOG_DEBUG
				(
					"Req PID {}: menu PID: {}, "
					"last menu PID: {}, menu name: {}, MIM running: {}, MIM player ID: {}. "
					"SUCC: {}",
					a_playerID, 
					glob.menuPID, 
					glob.prevMenuPID, 
					a_menuName, 
					glob.mim->IsRunning(), 
					glob.mim->managerMenuPID, 
					succ
				);
			}
			else
			{
				GlobalCoopData::SetMenuPlayerIDs(a_playerID);
				if (a_playerID != 0 && !glob.mim->IsRunning())
				{
					glob.mim->ToggleCoopPlayerMenuMode(a_deviceID, a_playerID);
				}
			}
		}
		else
		{
			// Reset directly if PID is -1.
			GlobalCoopData::ResetMenuPlayerIDs();
			glob.mim->ToggleCoopPlayerMenuMode(-1, -1);
			SPDLOG_DEBUG
			(
				"After resetting menu PIDs: menu PID: {}, "
				"last menu PID: {}, menu name: {}, MIM running: {}, MIM player ID: {}.",
				glob.menuPID,
				glob.prevMenuPID,
				a_menuName, 
				glob.mim->IsRunning(), 
				glob.mim->managerMenuPID
			);
		}
	}

	void CoopLib::RequestStateChange
	(
		RE::StaticFunctionTag*, int32_t a_playerID, uint32_t a_newState
	)
	{
		// Signal all of the given player's managers to change state to the given state.

		SPDLOG_DEBUG("PID {}'s managers -> state {}.", a_playerID, a_newState);
		if (!glob.allPlayersInit ||
			a_playerID <= -1 ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT || 
			a_newState >= !ManagerState::kTotal)
		{
			return;
		}

		glob.coopPlayers[a_playerID]->RequestStateChange
		(
			static_cast<ManagerState>(a_newState)
		);
	}

	void CoopLib::RescaleAVsOnBaseSkillAVChange
	(
		RE::StaticFunctionTag*, RE::Actor* a_playerActor
	)
	{
		// Rescale the given player's actor values when their base skill AVs change.
		// Usually occurs on class or race change.

		SPDLOG_DEBUG("{}.", a_playerActor ? a_playerActor->GetName() : "NONE");
		if (!a_playerActor)
		{
			return;
		}

		GlobalCoopData::RescaleAVsOnBaseSkillAVChange(a_playerActor);
	}

	void CoopLib::SetCoopPlayerClass
	(
		RE::StaticFunctionTag*, 
		RE::Actor* a_playerActor, 
		RE::TESClass* a_class,
		bool a_rescaleActorValues
	)
	{
		// Set the given player's class to the given class 
		// and optionally update base skill actor values.
		// The player and co-op session do not have to be active.

		SPDLOG_DEBUG
		(
			"Player {} -> class {}.", 
			a_playerActor ? a_playerActor->GetName() : "NONE", 
			a_class ? a_class->GetName() : "NONE"
		);
		if (!glob.globalDataInit || !a_playerActor || !a_class)
		{
			return;
		}

		if (auto actorBase = a_playerActor->GetActorBase(); actorBase)
		{
			actorBase->npcClass = a_class;
		}

		const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
		const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
		if (script)
		{
			script->SetCommand(fmt::format("SetClass {:X}", a_class->formID).c_str());
			script->CompileAndRun(a_playerActor);
			// Cleanup.
			delete script;
		}

		if (a_rescaleActorValues)
		{
			// Rescale skill AVs when done, since their base values have changed.
			GlobalCoopData::RescaleAVsOnBaseSkillAVChange(a_playerActor);
		}
	}

	void CoopLib::SetCoopPlayerRace
	(
		RE::StaticFunctionTag*,
		RE::Actor* a_playerActor, 
		RE::TESRace* a_race,
		bool a_rescaleActorValues
	)
	{
		// Set the given companion player's race to the given race 
		// and optionally update base skill actor values.
		// The player and co-op session do not have to be active.

		SPDLOG_DEBUG
		(
			"Player {} -> race {}.", 
			a_playerActor ? a_playerActor->GetName() : "NONE", 
			a_race ? a_race->GetName() : "NONE"
		);
		if (!glob.globalDataInit || !a_playerActor || !a_playerActor->race || !a_race) 
		{
			return;
		}

		Util::SetActorRace(a_playerActor, a_race);
		if (a_rescaleActorValues)
		{
			// Rescale skill AVs when done, since a race change can modify the base skill levels.
			GlobalCoopData::RescaleAVsOnBaseSkillAVChange(a_playerActor);
		}
	}

	void CoopLib::SetFavoritedEmoteIdles
	(
		RE::StaticFunctionTag*, 
		int32_t a_playerID,
		std::vector<RE::BSFixedString> a_emoteIdlesList
	)
	{
		// Update the given player's list of cyclable emote idle event names to the given list.

		SPDLOG_DEBUG("PID {}.", a_playerID);
		if (!glob.coopSessionActive ||
			a_playerID <= -1 ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT ||
			!glob.coopPlayers[a_playerID]->isActive) 
		{
			return;
		}

		glob.coopPlayers[a_playerID]->em->SetFavoritedEmoteIdles(a_emoteIdlesList);
	}

	void CoopLib::SetGifteePlayerActor(RE::StaticFunctionTag*, RE::Actor* a_playerActor)
	{
		// When opening the Gift Menu, set the given player actor as the recipient.
		// Setting to None/nullptr clears the giftee player.

		SPDLOG_DEBUG("{}.", a_playerActor ? a_playerActor->GetName() : "NONE");
		if (!glob.globalDataInit || !glob.coopSessionActive)
		{
			return;
		}

		glob.mim->gifteePlayerHandle = 
		(
			a_playerActor ? a_playerActor->GetHandle() : RE::ActorHandle()
		);
	}
	
	void CoopLib::SetIsSummoningFlag(RE::StaticFunctionTag*, bool a_set)
	{
		// Set the 'is summoning' flag, which indicates whether players 
		// are summoning their characters for co-op.

		if (!glob.globalDataInit)
		{
			glob.isSummoningPlayers = false;
			return;
		}

		SPDLOG_DEBUG("Set 'is summoning' to {}.", a_set);
		glob.isSummoningPlayers = a_set;
	}

	void CoopLib::SetPartyInvincibility(RE::StaticFunctionTag*, bool a_shouldSet)
	{
		// Enable/disable invincibility for all active players.
		// Play an FX shader while invulnerable.

		SPDLOG_DEBUG("Toggle {} for all players.", a_shouldSet ? "on" : "off");
		if (!glob.allPlayersInit) 
		{
			return;
		}

		for (const auto& p : glob.coopPlayers)
		{
			if (!p || !p->coopActor)
			{
				continue;
			}

			auto actorBase = p->coopActor->GetActorBase(); 
			if (!actorBase)
			{
				continue;
			}

			// Actor base ghost flag sets invincibility.
			auto& baseFlags = actorBase->actorData.actorBaseFlags;
			if (a_shouldSet)
			{
				baseFlags.set(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
				Util::StartEffectShader(p->coopActor.get(), glob.ghostFXShader, -1.0f);
			}
			else
			{
				baseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
				Util::StopAllEffectShaders(p->coopActor.get());
				Util::StopAllHitArtEffects(p->coopActor.get());
			}
		}
	}

	void CoopLib::SignalWaitForUpdate(RE::StaticFunctionTag*, bool a_shouldDismiss)
	{
		// Either dismiss all active players or just request their managers to wait for refresh.
		// Any active co-op session is also flagged as ended.

		SPDLOG_DEBUG("Should dismiss all active players: {}.", a_shouldDismiss);
		if (!glob.globalDataInit || !glob.allPlayersInit)
		{
			return;
		}
		
		// Dismiss P1 last, as the P1 ref alias script performs the final cleanup measures 
		// for the co-op session.
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive || p->isPlayer1)
			{
				continue;
			}

			if (a_shouldDismiss)
			{
				p->DismissPlayer();
			}
			else
			{
				p->RequestStateChange(ManagerState::kAwaitingRefresh);
			}
		}
		
		const auto& coopP1 = glob.coopPlayers[0];
		if (coopP1 && coopP1->isActive) 
		{
			if (a_shouldDismiss)
			{
				coopP1->DismissPlayer();
			}
			else
			{
				coopP1->RequestStateChange(ManagerState::kAwaitingRefresh);
			}
		}

		// Stop menu and camera managers and flag session as ended.
		glob.cam->ToggleCoopCamera(false);
		glob.mim->RequestStateChange(ManagerState::kAwaitingRefresh);
		// Restore XP threshold.
		GlobalCoopData::ModifyLevelUpXPThreshold(false);
		glob.coopSessionActive = false;
	}

	void CoopLib::TeleportToPlayerToActor
	(
		RE::StaticFunctionTag*, const int32_t a_playerID, RE::Actor* a_teleportTarget
	)
	{
		// Teleport the player with the given PID to the given actor.

		SPDLOG_DEBUG
		(
			"PID {} -> {}.",
			a_playerID,
			a_teleportTarget ? a_teleportTarget->GetName() : "NONE"
		);
		if (!glob.globalDataInit || 
			!glob.allPlayersInit || 
			a_playerID <= -1 || 
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}

		const auto& p = glob.coopPlayers[a_playerID]; 
		if (!p || !p->isActive)
		{
			return;
		}

		RE::ActorPtr teleportTargetPtr{ a_teleportTarget };
		p->taskRunner->AddTask
		(
			[&p, teleportTargetPtr]() 
			{ 
				if (!teleportTargetPtr)
				{
					return;
				}

				p->TeleportTask(teleportTargetPtr->GetHandle());
			}
		);
	}

	void CoopLib::ToggleCoopCamera(RE::StaticFunctionTag*, bool a_enable)
	{
		// Toggle the co-op camera on or off.

		SPDLOG_DEBUG("{}.", a_enable ? "ON" : "OFF");
		if (!glob.globalDataInit) 
		{
			return;
		}

		glob.cam->ToggleCoopCamera(a_enable);
	}

	void CoopLib::ToggleSetupMenuControl
	(
		RE::StaticFunctionTag*, int32_t a_deviceID, int32_t a_playerID, bool a_shouldEnter
	)
	{ 
		// Toggle menu control on or off for the given player 
		// when entering/exiting the Co-op Setup/Summoning Menu.

		SPDLOG_DEBUG
		(
			"DID: {}, PID: {}, should enter: {}.", a_deviceID, a_playerID, a_shouldEnter
		);
		if ((glob.globalDataInit && glob.mim) && 
			(a_deviceID > -1) && 
			(a_playerID > -1 && a_playerID < ALYSLC_MAX_PLAYER_COUNT)) 
		{
			// Set the opened menu name and type.
			glob.mim->SetOpenedMenu(GlobalCoopData::SETUP_MENU_NAME, a_shouldEnter);
			if (a_shouldEnter)
			{
				// Reset PMC overlay.
				glob.mim->ResetPlayerMenuControlOverlay();
				auto ui = RE::UI::GetSingleton();
				if (ui && !ui->IsMenuOpen(DebugOverlayMenu::MENU_NAME))
				{
					// Open the ALYSLC overlay if it isn't open already.
					SPDLOG_DEBUG("ALYSLC overlay not open. Opening.");
					DebugOverlayMenu::Load();
				}

				// Set menu PID directly to the requesting player's.
				GlobalCoopData::SetMenuPlayerIDs(a_playerID);
				// Signal MIM to start running.
				glob.mim->ToggleCoopPlayerMenuMode(a_deviceID, a_playerID);
			}
			else
			{
				// Reset menu PIDs.
				GlobalCoopData::ResetMenuPlayerIDs();
				// Signal MIM to pause and reset both DID and PID.
				glob.mim->ToggleCoopPlayerMenuMode(-1, -1);
			}
		}
		else
		{
			SPDLOG_ERROR
			(
				"Global co-op data not initialized: {}, "
				"MIM invalid: {}, DID invalid: {}, PID invalid: {}.",
				!glob.globalDataInit,
				!glob.mim,
				(a_deviceID <= -1),
				(a_playerID <= -1 && a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
			);
		}
	}

	void CoopLib::UpdateAllCompanionPlayerSerializationIDs(RE::StaticFunctionTag*)
	{
		// Update all serialized player FID keys.
		// Used to access each player's serialized data.

		SPDLOG_DEBUG("UpdateAllCompanionPlayerSerializationIDs.");
		if (!glob.globalDataInit)
		{
			return;
		}
			
		GlobalCoopData::UpdateAllCompanionPlayerSerializationIDs();
	}

	void CoopLib::Log(RE::StaticFunctionTag*, RE::BSFixedString a_message)
	{
		// Script request to log a debug message to this mod's log file:
		// 'ALYSLC.log'.

		SPDLOG_DEBUG("{}", a_message.c_str());
	}

	void CoopLib::LogError(RE::StaticFunctionTag*, RE::BSFixedString a_message)
	{
		// Script request to log an error message to this mod's log file:
		// 'ALYSLC.log'.

		SPDLOG_ERROR("{}", a_message.c_str());
	}

	//=============================================================================================
	//[Character Customization Functions]
	//=============================================================================================
	
	void CoopLib::CharacterCustomization::CopyNPCAppearanceToPlayer
	(
		RE::StaticFunctionTag*,
		int32_t a_playerID,
		RE::TESNPC* a_baseToCopy,
		bool a_setOppositeGenderAnims
	)
	{
		// Copy base NPC's appearance to the player. Set opposite gender animations if necessary.

		SPDLOG_DEBUG
		(
			"PID: {}, NPC base: {}, set opposite gender animations: {}.",
			a_playerID,
			a_baseToCopy ? a_baseToCopy->GetName() : "NONE", 
			a_setOppositeGenderAnims
		);
		if (!glob.allPlayersInit || 
			a_playerID <= -1 || 
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT ||
			!a_baseToCopy)
		{
			return;
		}

		glob.coopPlayers[a_playerID]->CopyNPCAppearanceToPlayer
		(
			a_baseToCopy, a_setOppositeGenderAnims
		);
	}
	
	void CoopLib::CharacterCustomization::ExportP1ActorBaseAppearanceData
	(
		RE::StaticFunctionTag*, RE::Actor* a_presetCharacter
	)
	{
		// Either import the given actor's appearance data onto P1's actorbase,
		// or export P1's appearance data to the given actor's actorbase.
		// Makes use of the face swap mannequin as a third actor 
		// to temporarily hold importable appearance data.
		// 
		// TODO: Save P1's appearance on import 
		// and automatically copy the saved appearance back to P1 on export.

		SPDLOG_DEBUG("ExportP1ActorBaseAppearanceData");
		if (!glob.globalDataInit || !a_presetCharacter)
		{
			return;
		}

		RE::ActorHandle actorHandle = a_presetCharacter->GetHandle();
		auto taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface)
		{
			return;
		}

		taskInterface->AddTask
		(
			[actorHandle]()
			{
				auto p1 = RE::PlayerCharacter::GetSingleton();
				if (!p1)
				{
					return;
				}

				auto actorPtr = Util::GetActorPtrFromHandle(actorHandle);
				if (!actorPtr)
				{
					return;
				}

				auto actorBase = actorPtr->GetActorBase();
				if (!actorBase)
				{
					return;
				}
						
				SPDLOG_DEBUG
				(
					"Exporting {}'s appearance to {}.",
					p1->GetName(), Util::GetEditorID(actorBase)
				);
				
				Util::ImportActorBaseAppearanceData(p1, actorPtr.get());
				// Maintain the changes when the game saves.
				actorBase->AddChange(RE::TESNPC::ChangeFlags::kFace);
				actorBase->AddChange(RE::TESNPC::ChangeFlags::kGender);
				actorBase->AddChange(RE::TESNPC::ChangeFlags::kRace);
			}
		);
	}

	bool CoopLib::CharacterCustomization::IsRaceMenuInstalled(RE::StaticFunctionTag*)
	{
		// Return true if RaceMenu by expired6978 is installed.
		
		SPDLOG_DEBUG("IsRaceMenuInstalled");
		return ALYSLC::RaceMenuCompat::g_installed;
	}

	void CoopLib::CharacterCustomization::LoadPlayerCharacterPreset
	(
		RE::StaticFunctionTag*, 
		RE::Actor* a_fromPresetCharacter
	)
	{
		// Load the exported character preset for the given player character.
		
		SPDLOG_DEBUG("LoadPlayerCharacterPreset");
		if (!a_fromPresetCharacter)
		{
			return;
		}
		
		// Update skin color and player model.
		RE::ActorHandle actorHandle = a_fromPresetCharacter->GetHandle();
		auto taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface)
		{
			return;
		}

		taskInterface->AddTask
		(
			[actorHandle]()
			{
				auto actorPtr = Util::GetActorPtrFromHandle(actorHandle);
				if (!actorPtr)
				{
					return;
				}

				auto actorBase = actorPtr->GetActorBase();
				if (!actorBase)
				{
					return;
				}

				Util::LoadOrSaveRaceMenuPreset(actorPtr.get(), true);
			}
		);
	}
	
	void CoopLib::CharacterCustomization::OnPreRaceMenu
	(
		RE::StaticFunctionTag*, RE::TESRace* a_newRace, bool a_setFemale
	)
	{
		// Set P1's gender and race, and save skill levels and perks
		// in preparation for opening the Race Menu.
		// 
		// NOTE: Unused for now until I can figure out how to seamlessly 
		// and automatically re-import P1's character preset 
		// after another player customizes their character.
		
		SPDLOG_DEBUG("OnPreRaceMenu");
		if (!a_newRace)
		{
			return;
		}

		// Update skin color and player model.
		auto taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface)
		{
			return;
		}

		taskInterface->AddTask
		(
			[a_newRace, a_setFemale]()
			{
				auto p1 = RE::PlayerCharacter::GetSingleton();
				if (!p1 || 
					!p1->race || 
					!p1->race->faceRelatedData ||
					!p1->GetActorBase() || 
					!p1->GetActorBase()->race)
				{
					return;
				}

				SPDLOG_DEBUG
				(
					"{}: set female: {}, current race: {}",
					p1->GetName(), a_setFemale, p1->race->GetName()
				);

				auto actorBase = p1->GetActorBase();
				Util::RemoveAllHeadParts(p1);
				Util::SetActorRaceAndGender(p1, a_newRace, a_setFemale);
				// Import the default race-given headparts after.
				Util::RemoveAllHeadParts(p1);
				Util::ImportDefaultRacialHeadParts(a_newRace, a_setFemale, actorBase);
				// Load default preset.
				Util::LoadDefaultBasePreset();
			}
		);
	}

	void CoopLib::CharacterCustomization::SavePlayerCharacterPreset
	(
		RE::StaticFunctionTag*, RE::Actor* a_toPresetCharacter
	)
	{
		// Save P1's name, race, and appearance as the given player character's preset.
		
		SPDLOG_DEBUG("SavePlayerCharacterPreset");
		if (!glob.globalDataInit || !a_toPresetCharacter)
		{
			return;
		}

		RE::ActorHandle actorHandle = a_toPresetCharacter->GetHandle();
		auto taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface)
		{
			return;
		}

		taskInterface->AddTask
		(
			[actorHandle]()
			{
				auto actorPtr = Util::GetActorPtrFromHandle(actorHandle);
				if (!actorPtr)
				{
					return;
				}

				auto actorBase = actorPtr->GetActorBase();
				if (!actorBase)
				{
					return;
				}

				Util::LoadOrSaveRaceMenuPreset(actorPtr.get(), false);
			}
		);
	}
	
	void CoopLib::CharacterCustomization::SetDefaultRacialAppearance
	(
		RE::StaticFunctionTag*,
		int32_t a_playerID,
		bool a_setFemale, 
		bool a_setOppositeGenderAnims
	)
	{
		// Import default racial headparts, update gender, animations, skin tone,
		// and refresh the player actor's 3D model when done.
		// Does not update appearance preset or change the player's race.
		// NOTE:
		// Any race swap must be fully completed first to update properly.

		SPDLOG_DEBUG
		(
			"PID: {}, set female: {}, set opposite gender anims: {}.",
			a_playerID, a_setFemale, a_setOppositeGenderAnims
		);
		if (!glob.allPlayersInit ||
			a_playerID <= -1 ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}

		glob.coopPlayers[a_playerID]->SetDefaultRacialAppearance
		(
			a_setFemale, a_setOppositeGenderAnims
		);
	}

	//=============================================================================================
	//[Debug Functions]
	//=============================================================================================

	void CoopLib::Debug::AssignPlayer1CID(RE::StaticFunctionTag*)
	{
		// Open a prompt which asks P1 to press a certain button on their controller
		// to assign their controller as P1's.

		SPDLOG_DEBUG("AssignPlayer1CID.");
		if (!glob.globalDataInit)
		{
			return;
		}

		glob.taskRunner->AddTask([]() { GlobalCoopData::PromptForPlayer1CIDTask(); });
	}

	void CoopLib::Debug::DisableGodModeForAllCoopPlayers(RE::StaticFunctionTag*)
	{
		// Disable god mode for all players.

		SPDLOG_DEBUG("DisableGodModeForAllCoopPlayers.");
		if (!glob.globalDataInit || !glob.coopSessionActive)
		{
			return;
		}

		glob.ToggleGodModeForAllPlayers(false, true);
	}

	void CoopLib::Debug::DisableGodModeForPlayer(RE::StaticFunctionTag*, int32_t a_playerID)
	{
		// Disable god mode for a specific player.

		SPDLOG_DEBUG("PID: {}.", a_playerID);
		if (!glob.globalDataInit || 
			!glob.coopSessionActive || 
			a_playerID <= -1 || 
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}
			
		glob.ToggleGodModeForPlayer(a_playerID, false, true);
	}

	void CoopLib::Debug::EnableGodModeForAllCoopPlayers(RE::StaticFunctionTag*)
	{
		// Enable god mode for all active players.

		SPDLOG_DEBUG("EnableGodModeForAllCoopPlayers.");
		if (!glob.globalDataInit || !glob.coopSessionActive)
		{
			return;
		}

		glob.ToggleGodModeForAllPlayers(true, true);
	}

	void CoopLib::Debug::EnableGodModeForPlayer(RE::StaticFunctionTag*, int32_t a_playerID)
	{
		// Enable god mode for a specific player.

		SPDLOG_DEBUG("PID: {}.", a_playerID);
		if (!glob.globalDataInit ||
			!glob.coopSessionActive ||
			a_playerID <= -1 ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}
		
		glob.ToggleGodModeForPlayer(a_playerID, true, true);
	}

	void CoopLib::Debug::MoveAllPlayersToPlayer(RE::StaticFunctionTag*, RE::Actor* a_playerActor)
	{
		// Move all other players to the given player.

		SPDLOG_DEBUG("{}", a_playerActor ? a_playerActor->GetName() : "P1");
		if (!glob.globalDataInit || !glob.coopSessionActive) 
		{
			return;
		}

		auto taskInterface = SKSE::GetTaskInterface(); 
		if (!taskInterface)
		{
			return;
		}

		auto moveToActorPtr = RE::ActorPtr(a_playerActor);
		// Move to P1 if the target player actor is invalid.
		if (!moveToActorPtr || !Util::IsValidRefrForTargeting(moveToActorPtr.get()))
		{
			moveToActorPtr = glob.player1Actor;
		}

		for (const auto& otherP : glob.coopPlayers)
		{
			if (!otherP || !otherP->isActive || otherP->coopActor == moveToActorPtr)
			{
				continue;
			}

			taskInterface->AddTask
			(
				[otherP, moveToActorPtr]() 
				{
					if (!moveToActorPtr)
					{
						return;
					}

					Util::TeleportToActor(otherP->coopActor.get(), moveToActorPtr.get());
				}
			);
		}
	}

	void CoopLib::Debug::ReEquipHandForms(RE::StaticFunctionTag*, int32_t a_playerID)
	{
		// Re-equip the player's desired hand forms (weapons/magic/armor).

		SPDLOG_DEBUG("PID: {}.", a_playerID);
		if (!glob.allPlayersInit || 
			!glob.coopSessionActive || 
			a_playerID <= -1 || 
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}

		const auto& p = glob.coopPlayers[a_playerID];
		// Sheathe, re-equip, then unsheathe for best results.
		p->pam->ReadyWeapon(false);
		p->em->ReEquipHandForms();
		p->pam->ReadyWeapon(true);
	}

	void CoopLib::Debug::RefreshAllPlayerManagers(RE::StaticFunctionTag*) 
	{
		// Refresh data for all active players' managers.

		SPDLOG_DEBUG("RefreshAllPlayerManagers.");
		if (!glob.globalDataInit || !glob.coopSessionActive)
		{
			return;
		}

		for (const auto& p : glob.coopPlayers)
		{
			if (!p || !p->isActive)
			{
				continue;
			}

			p->taskRunner->AddTask([&p]() { p->RefreshPlayerManagersTask(); });
		}
	}

	void CoopLib::Debug::RefreshPlayerManagers(RE::StaticFunctionTag*, int32_t a_playerID)
	{
		// Refresh data for all of the given player's managers.

		SPDLOG_DEBUG("PID: {}.", a_playerID);
		if (!glob.globalDataInit ||
			!glob.coopSessionActive ||
			a_playerID <= -1 ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}

		const auto& p = glob.coopPlayers[a_playerID]; 
		if (!p || !p->isActive)
		{
			return;
		}

		p->taskRunner->AddTask([&p]() { p->RefreshPlayerManagersTask(); });
	}

	void CoopLib::Debug::ResetCoopCompanion
	(
		RE::StaticFunctionTag*, int32_t a_playerID, bool a_unequipAll, bool a_reattachHavok
	)
	{
		// Hard reset a companion player:
		// Stop movement, clear movement offset, sheathe weapons/magic,
		// revert transformation, unequip hand forms, resurrect,
		// disable, re-enable, re-equip hand forms, reset I-frames flag, and re-enable movement.
		// Can optionally request to unequip all or re-attach havok.

		SPDLOG_DEBUG
		(
			"PID: {}, unequip all: {}, re-attach havok: {}.",
			a_playerID, a_unequipAll, a_reattachHavok
		);
		if (!glob.globalDataInit || 
			!glob.allPlayersInit || 
			a_playerID <= -1 ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}

		const auto& p = glob.coopPlayers[a_playerID]; 
		if (!p || !p->isActive)
		{
			return;
		}

		p->taskRunner->AddTask
		(
			[&p, a_unequipAll, a_reattachHavok]() 
			{ 
				p->ResetCompanionPlayerStateTask(a_unequipAll, a_reattachHavok); 
			}
		);
	}

	void CoopLib::Debug::ResetPlayer1AndCamera(RE::StaticFunctionTag*) 
	{
		// Stop P1's managers, disable the co-op camera, stop the menu input manager,
		// and re-enable movement for P1.

		SPDLOG_DEBUG("ResetPlayer1AndCamera.");
		if (!glob.globalDataInit) 
		{
			return;
		}

		if (glob.coopSessionActive) 
		{
			// Stop P1 managers, cam manager, and menu input manager.
			glob.taskRunner->AddTask([]() { GlobalCoopData::ResetPlayer1AndCameraTask(); });
		}

		// Ensure P1 is not animation driven or synced as a result of co-op.
		if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1) 
		{
			p1->SetGraphVariableBool("bAnimationDriven", false);
			p1->SetGraphVariableBool("bIsSynced", false);
		}
		
		// Also ensure P1 is not AI driven anymore.
		Util::SetPlayerAIDriven(false);
	}

	void CoopLib::Debug::ResetPlayer1State(RE::StaticFunctionTag*)
	{
		// Hard reset for P1:
		// Resurrect P1, re-attach havok, remove paralysis and fix ragdoll, sheathe weapons/magic,
		// revert any active transformation, re-equip hand forms, and reset I-frames flag.

		SPDLOG_DEBUG("ResetPlayer1State.");
		if (!glob.globalDataInit || !glob.allPlayersInit) 
		{
			return;
		}

		glob.coopPlayers[0]->ResetPlayer1();
	}

	void CoopLib::Debug::RespecPlayer
	(
		RE::StaticFunctionTag*, int32_t a_playerID
	)
	{
		// NOTE:
		// Not for Enderal.
		// Reset HMS AVs to default.
		// Remove all perks and refund all allotted perk points for the given player,
		// allowing them to completely respec their character.
		// Also remove all shared perks from all active players.
		// Since all shared perks are removed, all other active players are also
		// refunded any shared perk points and can re-use them as they see fit.

		if (!glob.globalDataInit || 
			!glob.allPlayersInit ||
			!glob.coopSessionActive ||
			ALYSLC::EnderalCompat::g_installed ||
			a_playerID <= -1 ||
			a_playerID >= ALYSLC_MAX_CONTROLLER_COUNT) 
		{
			return;
		}

		const auto& p = glob.coopPlayers[a_playerID]; 
		if (!p || !p->isActive)
		{
			return;
		}
		
		SPDLOG_DEBUG("{}.", p->coopActor->GetName());
		glob.taskRunner->AddTask
		(
			[a_playerID]() 
			{
				GlobalCoopData::RespecPlayerTask(a_playerID);
			}
		);
	}

	void CoopLib::Debug::RestartCoopCamera(RE::StaticFunctionTag*)
	{
		// Toggle the co-op camera off and then on again.

		SPDLOG_DEBUG("RestartCoopCamera.");
		if (!glob.globalDataInit || !glob.coopSessionActive)
		{
			return;
		}

		glob.taskRunner->AddTask([]() { GlobalCoopData::RestartCoopCameraTask(); });
	}

	void CoopLib::Debug::StopAllCombatOnCoopPlayers(RE::StaticFunctionTag*, bool a_clearBounties) 
	{
		// Stop combat on all active players,
		// optionally clearing all bounties to get off scot-free.
		
		SPDLOG_DEBUG("Clear bounties too: {}", a_clearBounties);
		if (!glob.globalDataInit)
		{
			return;
		}

		// Stops combat for all actors from each process level (low, mid low, mid high, and high).
		GlobalCoopData::StopAllCombatOnCoopPlayers(false, std::move(a_clearBounties));
		// Yes, I know, some redundancy. 
		// Will ensure players and allies are not in combat if the above call fails.
		// Only checks the high process actors.
		// Iterating through magic effects for all process' actors is too demanding.
		Util::StopCombatOnPlayerAndAllies();
	}

	void CoopLib::Debug::StopMenuInputManager(RE::StaticFunctionTag*)
	{
		// Signal the menu input manager to stop running, returning menu control to P1.

		if (!glob.globalDataInit)
		{
			return;
		}

		SPDLOG_DEBUG
		(
			"Current menu-related PIDs: menu: {}, last menu: {}, manager: {}.",
			glob.menuPID, glob.prevMenuPID, glob.mim->managerMenuPID
		);
		GlobalCoopData::StopMenuInputManager();
	}
	
	//=============================================================================================
	//[MCM Settings Import]
	//=============================================================================================
	
	void CoopLib::Settings::OnConfigClose(RE::TESQuest*)
	{
		// Import all settings when this mod's MCM closes.

		ALYSLC::Settings::ImportAllSettings();
	}
	
	//=============================================================================================
	//[Papyrus API Functions]
	//=============================================================================================
	
	//=============================================================================================
	// [V1]
	//=============================================================================================
	RE::Actor* CoopLib::API::GetALYSLCPlayerByDID
	(
		RE::StaticFunctionTag*, int32_t a_deviceID
	)
	{
		// Return the player character corresponding to the player with the given device ID.

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		auto playerHandle = modAPI->GetALYSLCPlayerByDID(a_deviceID);
		return
		(
			playerHandle && playerHandle.get() ? 
			playerHandle.get().get() : 
			nullptr
		);
	}

	RE::Actor* CoopLib::API::GetALYSLCPlayerByPID
	(
		RE::StaticFunctionTag*, int32_t a_playerID
	)
	{
		// Return the player character corresponding to the player with the given player ID.

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		auto playerHandle = modAPI->GetALYSLCPlayerByPID(a_playerID);
		return
		(
			playerHandle && playerHandle.get() ? 
			playerHandle.get().get() : 
			nullptr
		);
	}

	int32_t CoopLib::API::GetALYSLCPlayerDID(RE::StaticFunctionTag*, RE::Actor* a_actor)
	{
		// Return the device ID for the input device controlling the given player character.

		if (!a_actor)
		{
			return -1;
		}

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->GetALYSLCPlayerDID(a_actor->GetHandle());
	}

	int32_t CoopLib::API::GetALYSLCPlayerPID(RE::StaticFunctionTag*, RE::Actor* a_actor)
	{
		// Return the player ID for the player controlling the given player character.

		if (!a_actor)
		{
			return -1;
		}

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->GetALYSLCPlayerPID(a_actor->GetHandle());
	}

	bool CoopLib::API::IsALYSLCCharacter(RE::StaticFunctionTag*, RE::Actor* a_actor)
	{
		// Return true if the given actor is a player character (P1 or companion player).
		// Can be called even when a co-op session is not active.

		if (!a_actor)
		{
			return false;
		}

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->IsALYSLCCharacter(a_actor->GetHandle());
	}

	bool CoopLib::API::IsALYSLCPlayer(RE::StaticFunctionTag*, RE::Actor* a_actor)
	{
		// Return true if the given actor is an active player character (P1 or companion player).
		// Only returns true if a co-op session is active 
		// and the given actor is being controlled by a player.

		if (!a_actor)
		{
			return false;
		}

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->IsALYSLCPlayer(a_actor->GetHandle());
	}
	
	bool CoopLib::API::IsSessionActive(RE::StaticFunctionTag*)
	{
		// Return true if a co-op session is active.

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->IsSessionActive();
	}
	
	//=============================================================================================
	// [V2]
	//=============================================================================================

	bool CoopLib::API::IsPlayerPerformingAction
	(
		RE::StaticFunctionTag*, RE::Actor* a_playerActor, uint32_t a_playerActionIndex
	)
	{
		// Return true if the player controlling the given actor 
		// is performing the player action corresponding to the given player action index.
		// See the 'ALYSLC::InputAction' enum in the 'Enums.h' file
		// for the supported action indices.

		if (!a_playerActor)
		{
			return false;
		}

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->IsPerformingAction(a_playerActor->GetHandle(), a_playerActionIndex);
	}

	bool CoopLib::API::IsPlayerPressingInput
	(
		RE::StaticFunctionTag*, RE::Actor* a_playerActor, uint32_t a_inputIndex
	)
	{
		// Return true if the player controlling the given character
		// is pressing the input that corresponds to the given index.
		// See the 'ALYSLC::InputAction' enum in the 'Enums.h' file
		// for the supported input indices.

		if (!a_playerActor)
		{
			return false;
		}
		
		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->IsPressingInput(a_playerActor->GetHandle(), a_inputIndex);
	}

	//=============================================================================================
	// [V3]
	//=============================================================================================

	void CoopLib::API::AddSkillXP
	(
		RE::StaticFunctionTag*, RE::Actor* a_playerActor, int32_t a_skillAVIndex, float a_baseXP
	)
	{
		// Increment the given player's serialized XP total for the given skill.
		// Factors in the player's specific XP modifier.
		// Shared skills are leveled up directly through P1 
		// and nothing is saved to the serialized data.

		if (!a_playerActor || 
			a_skillAVIndex <= !RE::ActorValue::kNone ||
			a_skillAVIndex >= !RE::ActorValue::kTotal)
		{
			return;
		}

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->AddSkillXP
		(
			a_playerActor->GetHandle(), static_cast<RE::ActorValue>(a_skillAVIndex), a_baseXP
		);
	}

	int32_t CoopLib::API::GetMenuControlPID(RE::StaticFunctionTag*)
	{
		// Return the player ID for the player currently controlling menus.
		// NOTE:
		// Works even before a co-op session starts.
		
		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->GetMenuControlPID();
	}

	RE::Actor* CoopLib::API::GetMenuControlPlayer(RE::StaticFunctionTag*)
	{
		// Return the actor handle for the player currently controlling menus.
		// NOTE:
		// If the player currently controlling menus does not have an active character,
		// such as before the co-op session starts, this call will return an empty handle.
		
		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		auto playerHandle = modAPI->GetMenuControlPlayer();
		return
		(
			playerHandle && playerHandle.get() ? 
			playerHandle.get().get() : 
			nullptr
		);
	}

	void CoopLib::API::RequestMenuControl
	(
		RE::StaticFunctionTag*, 
		int32_t a_playerID,
		RE::BSFixedString a_menuName,
		RE::TESObjectREFR* a_assocRefr
	)
	{
		// Insert a request from the given player to control the given menu.
		// NOTE: 
		// Call before the desired menu opens and while a co-op session is active.

		const auto& modAPI = ALYSLC_API::ALYSLCInterface::GetSingleton();
		return modAPI->RequestMenuControl
		(
			a_playerID, a_menuName, a_assocRefr ? a_assocRefr->GetHandle() : RE::ObjectRefHandle()
		);
	}

	//=============================================================================================
	// Register papyrus functions
	//=============================================================================================
	bool CoopLib::RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm)
	{
		// Registered functions for ALYSLC's scripts.
		a_vm->RegisterFunction("ChangeCoopSessionState"s, "ALYSLC"s, ChangeCoopSessionState);
		a_vm->RegisterFunction("EnableCoopEntityCollision"s, "ALYSLC"s, EnableCoopEntityCollision);
		a_vm->RegisterFunction("GetAllAppearancePresets"s, "ALYSLC"s, GetAllAppearancePresets);
		a_vm->RegisterFunction("GetAllClasses"s, "ALYSLC"s, GetAllClasses);
		a_vm->RegisterFunction
		(
			"GetAllCyclableEmoteIdleEvents"s, "ALYSLC"s, GetAllCyclableEmoteIdleEvents
		);
		a_vm->RegisterFunction("GetAllSelectableRaces"s, "ALYSLC"s, GetAllSelectableRaces);
		a_vm->RegisterFunction("GetAllVoiceTypes"s, "ALYSLC"s, GetAllVoiceTypes);
		a_vm->RegisterFunction
		(
			"GetCompanionPlayerCharacters"s, "ALYSLC"s, GetCompanionPlayerCharacters
		);
		a_vm->RegisterFunction
		(
			"GetConnectedInputDeviceIDs"s, "ALYSLC"s, GetConnectedInputDeviceIDs
		);
		a_vm->RegisterFunction("GetFavoritedEmoteIdles"s, "ALYSLC"s, GetFavoritedEmoteIdles);
		a_vm->RegisterFunction("InitializeCoopPlayers"s, "ALYSLC"s, InitializeCoopPlayers);
		a_vm->RegisterFunction("InitializeGlobalData"s, "ALYSLC"s, InitializeGlobalData);
		a_vm->RegisterFunction("RequestMenuControl"s, "ALYSLC"s, RequestMenuControl);
		a_vm->RegisterFunction("RequestStateChange"s, "ALYSLC"s, RequestStateChange);
		a_vm->RegisterFunction
		(
			"RescaleAVsOnBaseSkillAVChange"s, "ALYSLC"s, RescaleAVsOnBaseSkillAVChange
		);
		a_vm->RegisterFunction("SetCoopPlayerClass"s, "ALYSLC"s, SetCoopPlayerClass);
		a_vm->RegisterFunction("SetCoopPlayerRace"s, "ALYSLC"s, SetCoopPlayerRace);
		a_vm->RegisterFunction("SetFavoritedEmoteIdles"s, "ALYSLC"s, SetFavoritedEmoteIdles);
		a_vm->RegisterFunction("SetGifteePlayerActor"s, "ALYSLC"s, SetGifteePlayerActor);
		a_vm->RegisterFunction("SetIsSummoningFlag"s, "ALYSLC"s, SetIsSummoningFlag);
		a_vm->RegisterFunction("SetPartyInvincibility"s, "ALYSLC"s, SetPartyInvincibility);
		a_vm->RegisterFunction("SignalWaitForUpdate"s, "ALYSLC"s, SignalWaitForUpdate);
		a_vm->RegisterFunction("TeleportToPlayerToActor"s, "ALYSLC"s, TeleportToPlayerToActor);
		a_vm->RegisterFunction("ToggleCoopCamera"s, "ALYSLC"s, ToggleCoopCamera);
		a_vm->RegisterFunction("ToggleSetupMenuControl"s, "ALYSLC"s, ToggleSetupMenuControl);
		a_vm->RegisterFunction
		(
			"UpdateAllCompanionPlayerSerializationIDs"s,
			"ALYSLC"s, 
			UpdateAllCompanionPlayerSerializationIDs
		);

		// Logging
		a_vm->RegisterFunction("Log"s, "ALYSLC"s, Log);
		a_vm->RegisterFunction("LogError"s, "ALYSLC"s, LogError);

		// Character customization functions.
		a_vm->RegisterFunction
		(
			"CopyNPCAppearanceToPlayer"s,
			"ALYSLC"s,
			CharacterCustomization::CopyNPCAppearanceToPlayer
		);
		a_vm->RegisterFunction
		(
			"ExportP1ActorBaseAppearanceData",
			"ALYSLC"s, 
			CharacterCustomization::ExportP1ActorBaseAppearanceData
		);
		a_vm->RegisterFunction
		(
			"IsRaceMenuInstalled"s,
			"ALYSLC"s,
			CharacterCustomization::IsRaceMenuInstalled
		);
		a_vm->RegisterFunction
		(
			"LoadPlayerCharacterPreset",
			"ALYSLC"s, 
			CharacterCustomization::LoadPlayerCharacterPreset
		);
		a_vm->RegisterFunction
		(
			"OnPreRaceMenu"s,
			"ALYSLC"s,
			CharacterCustomization::OnPreRaceMenu
		);
		a_vm->RegisterFunction
		(
			"SavePlayerCharacterPreset",
			"ALYSLC"s, 
			CharacterCustomization::SavePlayerCharacterPreset
		);
		a_vm->RegisterFunction
		(
			"SetDefaultRacialAppearance"s,
			"ALYSLC"s, 
			CharacterCustomization::SetDefaultRacialAppearance
		);

		// Debug menu functions.
		a_vm->RegisterFunction("AssignPlayer1CID"s, "ALYSLC"s, Debug::AssignPlayer1CID);
		a_vm->RegisterFunction
		(
			"EnableGodModeForAllCoopPlayers"s, "ALYSLC"s, Debug::EnableGodModeForAllCoopPlayers
		);
		a_vm->RegisterFunction
		(
			"EnableGodModeForPlayer"s, "ALYSLC"s, Debug::EnableGodModeForPlayer
		);
		a_vm->RegisterFunction
		(
			"DisableGodModeForAllCoopPlayers"s, "ALYSLC"s, Debug::DisableGodModeForAllCoopPlayers
		);
		a_vm->RegisterFunction
		(
			"DisableGodModeForPlayer"s, "ALYSLC"s, Debug::DisableGodModeForPlayer
		);
		a_vm->RegisterFunction
		(
			"MoveAllPlayersToPlayer"s, "ALYSLC"s, Debug::MoveAllPlayersToPlayer
		);
		a_vm->RegisterFunction("ReEquipHandForms"s, "ALYSLC"s, Debug::ReEquipHandForms);
		a_vm->RegisterFunction
		(
			"RefreshAllPlayerManagers"s, "ALYSLC"s, Debug::RefreshAllPlayerManagers
		);
		a_vm->RegisterFunction("RefreshPlayerManagers"s, "ALYSLC"s, Debug::RefreshPlayerManagers);
		a_vm->RegisterFunction("ResetCoopCompanion"s, "ALYSLC"s, Debug::ResetCoopCompanion);
		a_vm->RegisterFunction
		(
			"RespecPlayer"s, "ALYSLC"s, Debug::RespecPlayer
		);
		a_vm->RegisterFunction("ResetPlayer1AndCamera"s, "ALYSLC"s, Debug::ResetPlayer1AndCamera);
		a_vm->RegisterFunction("ResetPlayer1State"s, "ALYSLC"s, Debug::ResetPlayer1State);
		a_vm->RegisterFunction("RestartCoopCamera"s, "ALYSLC"s, Debug::RestartCoopCamera);
		a_vm->RegisterFunction
		(
			"StopAllCombatOnCoopPlayers"s, "ALYSLC"s, Debug::StopAllCombatOnCoopPlayers
		);
		a_vm->RegisterFunction("StopMenuInputManager"s, "ALYSLC"s, Debug::StopMenuInputManager);

		// MCM settings.
		a_vm->RegisterFunction("OnConfigClose"s, "__ALYSLC_ConfigMenu"s, Settings::OnConfigClose);

		// Papyrus API functions.
		// TODO:
		// More framework functions for any scripts wishing to access/modify ALYSLC data.

		// [V1]
		a_vm->RegisterFunction("GetALYSLCPlayerByDID"s, "ALYSLC_API"s, API::GetALYSLCPlayerByDID);
		a_vm->RegisterFunction("GetALYSLCPlayerByPID"s, "ALYSLC_API"s, API::GetALYSLCPlayerByPID);
		a_vm->RegisterFunction("GetALYSLCPlayerDID"s, "ALYSLC_API"s, API::GetALYSLCPlayerDID);
		a_vm->RegisterFunction("GetALYSLCPlayerPID"s, "ALYSLC_API"s, API::GetALYSLCPlayerPID);
		a_vm->RegisterFunction("IsALYSLCCharacter"s, "ALYSLC_API"s, API::IsALYSLCCharacter);
		a_vm->RegisterFunction("IsALYSLCPlayer"s, "ALYSLC_API"s, API::IsALYSLCPlayer);
		a_vm->RegisterFunction("IsSessionActive"s, "ALYSLC_API"s, API::IsSessionActive);

		// [V2]
		a_vm->RegisterFunction
		(
			"IsPlayerPerformingAction"s, "ALYSLC_API"s, API::IsPlayerPerformingAction
		);
		a_vm->RegisterFunction
		(
			"IsPlayerPressingInput"s, "ALYSLC_API"s, API::IsPlayerPressingInput
		);

		// [V3]
		a_vm->RegisterFunction("AddSkillXP"s, "ALYSLC_API"s, API::AddSkillXP);
		a_vm->RegisterFunction("GetMenuControlPID"s, "ALYSLC_API"s, API::GetMenuControlPID);
		a_vm->RegisterFunction("GetMenuControlPlayer"s, "ALYSLC_API"s, API::GetMenuControlPlayer);
		a_vm->RegisterFunction("RequestMenuControl"s, "ALYSLC_API"s, API::RequestMenuControl);

		return true;
	}
}

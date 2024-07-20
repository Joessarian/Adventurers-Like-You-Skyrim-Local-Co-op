#include "Proxy.h"
#include <Controller.h>
#include <Enums.h>
#include <Events.h>
#include <GlobalCoopData.h>
#include <MenuInputManager.h>
#include <Player.h>
#include <Serialization.h>

namespace ALYSLC
{
	// Global co-op data used to help the proxy delegate papyrus function calls
	// to the corresponding plugin functions.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	//========================================================================================================================================================================
	// Initialization functions defined in proper order of execution.
	//========================================================================================================================================================================
 
	void CoopLib::InitializeGlobalData(RE::StaticFunctionTag*, RE::BGSRefAlias* a_player1Ref)
	{
		// Initialize or re-assign global co-op data.
		// Called each time a save is loaded.

		// REMOVE when done debugging.
		logger::debug("[Proxy] InitializeGlobalData.");

		if (!glob.globalDataInit) 
		{
			// Only set all global data once per play session (on save load).
			GlobalCoopData::InitializeGlobalCoopData(a_player1Ref);
		}
		else 
		{
			// Player 1 data may change on loading a save (if another player's save is loaded).
			// Must also ensure the co-op camera manager is not running on save load.

			// Reset player 1's CID.
			// Will be automatically re-assigned on the first summoning after save load.
			glob.player1CID = -1;
			// Reset controller ID in control of dialogue.
			glob.moarm->reqDialoguePlayerCID = -1;
			// Set player ref alias, which may have changed.
			glob.player1RefAlias = a_player1Ref;
			// Get player 1, which may be a different character.
			glob.player1Actor.reset();
			glob.player1Actor = RE::ActorPtr(RE::PlayerCharacter::GetSingleton());
			// Set living and active players to 0 when not in co-op.
			glob.livingPlayers = glob.activePlayers = 0;
			// Reset QuickLoot menu-opening data.
			glob.quickLootReqCID = -1;
			glob.reqQuickLootContainerHandle = RE::ObjectRefHandle();
			// Co-op camera paused and not waiting for toggle.
			glob.cam->waitForToggle = false;
			glob.cam->ToggleCoopCamera(false);
		}

		// Import all settings.
		ALYSLC::Settings::ImportAllSettings();
		// Re-register for script events.
		GlobalCoopData::UnregisterEvents();
		GlobalCoopData::RegisterEvents();
		// Reset supported menu open state because it won't reset
		// properly if the previous co-op session ended while a supported menu was open.
		Events::ResetMenuState();
		// Re-enable any controls for P1 that might have been disabled.
		Util::ToggleAllControls(true);
		// Reset the default third person camera orientation, 
		// just in case the game was saved while the co-op cam was active.
		Util::ResetTPCamOrientation();
		// Reset essential flag for P1, which may have been set if using the revive system.
		if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1 && p1->GetActorBase()) 
		{
			auto actorBase = p1->GetActorBase();
			Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kEssential, false);
			p1->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
		}

		logger::debug("[Proxy] InitializeGlobalData: Done initializing global co-op data");
	}

	std::vector<std::uint32_t> CoopLib::GetConnectedCoopControllerIDs(RE::StaticFunctionTag*)
	{
		// Setup controller data for all connected controllers and return a list of controller IDs
		// for all active controllers. Player 1's CID is always first.

		logger::debug("[Proxy] GetConnectedCoopControllerIDs.");
		if (glob.globalDataInit) 
		{
			return glob.cdh->SetupConnectedCoopControllers();
		}
		else
		{
			return std::vector<std::uint32_t>();
		}
	}

	bool CoopLib::InitializeCoop(RE::StaticFunctionTag*, uint32_t a_numCompanions, std::vector<uint32_t> a_controllerIDs, std::vector<RE::Actor*> a_coopActors,  std::vector<uint32_t> a_packageFormListIndicesList)
	{
		// Initializes/updates all co-op players with the given data.
		// Returns true if co-op session was initialized successfully.

		logger::debug("[Proxy] InitializeCoop.");
		if (glob.globalDataInit) 
		{
			// Set P1's controller ID.
			// Is always the first index in the controller IDs list.
			// Player 1 CID must be set before starting co-op.
			if (glob.player1CID == -1) 
			{
				logger::critical("[Proxy] InitializeCoop: P1 CID not assigned. Stopping co-op session.");
				RE::DebugMessageBox(fmt::format("[ALYSLC] Error: Player 1's controller ID was not assigned.\nPlease open the debug menu with player 1's controller or manually assign a controller ID to player 1 through the debug menu option before summoning other players.").c_str());
				glob.activePlayers = glob.livingPlayers = 0;
				return false;
			}

			glob.player1CID = a_controllerIDs[0];
			// Reset living and active players count before constructing/updating co-op players.
			glob.livingPlayers = glob.activePlayers = 0;

			// Attempting to account for discontinuity in which controller ports are active.
			// e.g. port 1 and port 3 are active, so port 2 must remain inactive with no co-op player assigned.
			std::array<bool, ALYSLC::Settings::fMaxNumControllers> isActiveControllerIDList = { false, false, false, false };

			// REMOVE when done debugging.
			logger::debug("[Proxy] InitializeCoop: Controller IDs vector length: {}, number of companion players: {}.", 
				a_controllerIDs.size(), a_numCompanions);
			logger::debug("[Proxy] InitializeCoop: Controller IDs: {}, {}, {}, {}",
				a_controllerIDs.size() > 0 ? a_controllerIDs[0] : -1, 
				a_controllerIDs.size() > 1 ? a_controllerIDs[1] : -1,
				a_controllerIDs.size() > 2 ? a_controllerIDs[2] : -1, 
				a_controllerIDs.size() > 3 ? a_controllerIDs[3] : -1);
			logger::debug("[Proxy] InitializeCoop: Co-op actors: {}, {}, {}, {}",
				(a_coopActors[0]) ? a_coopActors[0]->GetName() : "None",
				(a_coopActors[1]) ? a_coopActors[1]->GetName() : "None",
				(a_coopActors[2]) ? a_coopActors[2]->GetName() : "None",
				(a_coopActors[3]) ? a_coopActors[3]->GetName() : "None");

			// Create 4 co-op players.
			// Subsequent calls to initialize will reuse the co-op player objects
			// by simply updating the co-op actor, controller ID,
			// and refreshing data that should be updated on re-summoning.
			for (auto i = 0; i < a_numCompanions + 1; ++i)
			{
				// Invalid player at index given by an active controller ID, so stop initializing co-op.
				if (!a_coopActors[a_controllerIDs[i]]) 
				{
					logger::error("[Proxy] ERR: InitializeCoop: Player with controller ID {} is invalid. Stopping co-op session.", a_controllerIDs[i]);
					RE::DebugMessageBox
					(
						fmt::format
						(
							"[ALYSLC] Critical error: player with controller ID {} is invalid.\nPlease contact the mod author about his incompetence.", 
							a_controllerIDs[i]
						).c_str()
					);
					glob.activePlayers = glob.livingPlayers = 0;
					return false;
				}

				// This controller is active.
				isActiveControllerIDList[a_controllerIDs[i]] = true;
			}

			for (uint32_t i = 0; i < isActiveControllerIDList.size(); ++i)
			{
				// Instantiate co-op player with co-op actor assigned.
				if (isActiveControllerIDList[i])
				{
					// Get index for the active controller ID from the controller ID list.
					auto inputIndex = std::find(a_controllerIDs.begin(), a_controllerIDs.end(), i) - a_controllerIDs.begin();

					// REMOVE when done debugging.
					logger::debug("[Proxy] InitializeCoop: [P{}] active at controller ID list index {}: {}.",
						i + 1, 
						inputIndex, 
						a_coopActors[inputIndex] ? a_coopActors[inputIndex]->GetName() : "NONE");

					// Update serialization data and import saved unlocked perks.
					bool succ = GlobalCoopData::UpdateSerializedCompanionPlayerFIDKey(a_coopActors[inputIndex]);
					// If not successful, could not get and update this player's serialized data, so stop initializing co-op.
					if (!succ)
					{
						RE::DebugMessageBox
						(
							fmt::format
							(
								"[ALYSLC] Failed to retrieve {}'s serialized data.\nPlease save the game and reload before summoning again.", 
								a_coopActors[inputIndex] ? a_coopActors[inputIndex]->GetName() : "NONE"
							).c_str()
						);
						glob.activePlayers = glob.livingPlayers = 0;
						return false;
					}

					if (!glob.allPlayersInit)
					{
						// REMOVE when done debugging.
						logger::debug("[Proxy] InitializeCoop: Constructing new coop player '{}' with package start index {}.", 
							a_coopActors[inputIndex]->GetName(), a_packageFormListIndicesList[inputIndex]);

						// Construct active player at index given by controller ID.
						glob.coopPlayers[i] = std::make_shared<CoopPlayer>
						(
							a_controllerIDs[inputIndex], 
							a_coopActors[inputIndex], 
							a_packageFormListIndicesList[inputIndex]
						);
					}
					else
					{
						// REMOVE when done debugging.
						logger::debug("[Proxy] InitializeCoop: Updating coop player '{}' with package start index {}.",
							a_coopActors[inputIndex]->GetName(), a_packageFormListIndicesList[inputIndex]);

						// Simply update the current co-op player objects to reflect the new data received.
						glob.coopPlayers[i]->UpdateCoopPlayer
						(
							a_controllerIDs[inputIndex], 
							a_coopActors[inputIndex], 
							a_packageFormListIndicesList[inputIndex]
						);
					}

					// Increment number of active, living players.
					++glob.activePlayers;
					++glob.livingPlayers;
				}
				else
				{
					logger::debug("[Proxy] InitializeCoop: [P{}] inactive", i + 1);
					// Construct inactive player to clear out all previous data.
					glob.coopPlayers[i] = std::make_shared<CoopPlayer>(-1, nullptr, -1);
				}
			}

			// Set player IDs and initialize all other data after construction.
			uint8_t currentID = 1;
			for (uint8_t i = 0; i < glob.coopPlayers.size(); ++i)
			{
				const auto& p = glob.coopPlayers[i];
				if (p->isActive) 
				{
					if (p->isPlayer1) 
					{
						// Player 1 is always at index 0.
						p->playerID = 0;
					}
					else
					{
						// After P1, companion players are ordered based on their controller IDs.
						p->playerID = currentID;
						++currentID;
					}

					// Since the player manager is itself a member of each sub manager, 
					// for ease of access to all other player sub-managers,
					// initialize all sub-managers after full construction of the player manager.
					// Player manager shared pointer should have a use count of 5 (1 global + 1 per manager X 4 managers)
					p->em->Initialize(p);
					p->mm->Initialize(p);
					p->pam->Initialize(p);
					p->tm->Initialize(p);

					// REMOVE when done debugging.
					logger::debug("[Proxy] InitializeCoop: {}: CID {} -> player ID {}, use count: {}",
						p->coopActor->GetName(),
						p->controllerID,
						p->playerID,
						p.use_count());
				}
			}

			// All players have now been initialized for the first time.
			glob.allPlayersInit = true;

			// REMOVE when done debugging.
			logger::debug("[Proxy] InitializeCoop: Number of active players for co-op session: {}",
				glob.activePlayers);

			return true;
		}

		// No global co-op data assigned, so we can't start co-op.
		return false;
	}

	//========================================================================================================================================================================
	// Post-summoning Papyrus functions listed in alphabetical order
	//========================================================================================================================================================================

	void CoopLib::ChangeCoopSessionState(RE::StaticFunctionTag*, bool a_shouldStart) 
	{
		// Start or stop a co-op session by starting/pausing all active players' managers 
		// and synchronizing actor values, perks, and items.

		logger::debug("[Proxy] ChangeCoopSessionState. {} session.", a_shouldStart ? "Starting" : "Ending");
		if (glob.globalDataInit && glob.allPlayersInit) 
		{
			// Enable P1's controls just to be safe.
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
				}
			);
			
			// Set global co-op session flag.
			glob.coopSessionActive = a_shouldStart;
			for (const auto& p : glob.coopPlayers) 
			{
				if (p && p->isActive) 
				{
					if (a_shouldStart)
					{
						// Make sure P1 and companion players are not set to essential before starting.
						if (p->isPlayer1)
						{
							glob.player1RefAlias->flags.reset(RE::BGSBaseAlias::FLAGS::kEssential);
						}

						Util::NativeFunctions::SetActorBaseDataFlag(p->coopActor->GetActorBase(), RE::ACTOR_BASE_DATA::Flag::kEssential, false);
						p->coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
						// Make sure the player is not paralyzed either (from being downed).
						p->coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);

						// Register the player for script events and then start all their managers.
						p->RegisterEvents();
						p->RequestStateChange(ManagerState::kRunning);
					}
					else 
					{
						// Signal all player managers to pause await data refresh
						// before unregistering the player for script events.
						p->RequestStateChange(ManagerState::kAwaitingRefresh);
						p->UnregisterEvents();
					}
				}
			}
			
			// Give all accumulated party-wide shared items to P1.
			GlobalCoopData::GivePartyWideItemsToP1();
			// Modify the level XP gained per skill level up to scale inversely with the number of active players.
			GlobalCoopData::ModifyXPPerSkillLevelMult(a_shouldStart);
			// Turn off god mode.
			GlobalCoopData::ToggleGodModeForAllPlayers(false);
			// Sync shared AVs, perks, and scale companion player's skill AVs.
			logger::debug("[Proxy] Syncing perks and shared AVs.", a_shouldStart);
			GlobalCoopData::SyncSharedSkillAVs();
			GlobalCoopData::SyncSharedPerks();
			GlobalCoopData::PerformInitialAVAutoScaling();
			GlobalCoopData::RescaleActivePlayerAVs();

			logger::debug("[Proxy] ChangeCoopSessionState: Co-op session has now {}.", a_shouldStart ? "started" : "ended");
		}
		else
		{ 
			logger::critical("[Proxy] ERR: ChangeCoopSessionState: Cannot start co-op session. Global data not initialized: {}, all players not initialized: {}", 
				!glob.globalDataInit, !glob.allPlayersInit);
			glob.coopSessionActive = false;
		}

		// Lastly, reset menu CIDs/PIDs.
		if (glob.globalDataInit) 
		{
			glob.menuCID = glob.prevMenuCID = glob.mim->managerMenuCID = -1;
			glob.mim->managerMenuPlayerID = glob.mim->pmcPlayerID = 0;
		}
	}

	void CoopLib::CopyNPCAppearanceToPlayer(RE::StaticFunctionTag*, int32_t a_controllerID, RE::TESNPC* a_baseToCopy, bool a_setOppositeGenderAnims)
	{
		// Copy base NPC's appearance to the player. Set opposite gender animations if necessary.

		logger::debug("[Proxy] CopyNPCAppearanceToPlayer: CID: {}, NPC base: {}, set opposite gender animations: {}.",
			a_controllerID, a_baseToCopy ? a_baseToCopy->GetName() : "NONE", a_setOppositeGenderAnims);
		if (glob.allPlayersInit && a_controllerID < ALYSLC_MAX_PLAYER_COUNT && a_baseToCopy)
		{
			glob.coopPlayers[a_controllerID]->CopyNPCAppearanceToPlayer(a_baseToCopy, a_setOppositeGenderAnims);
		}
	}

	std::vector<RE::TESForm*> CoopLib::GetAllAppearancePresets(RE::StaticFunctionTag*, RE::TESRace* a_race, bool a_female)
	{ 
		// Get all actor base NPC appearance presets, narrowed down by race and sex.

		std::vector<RE::TESForm*> npcList;
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			const auto& npcForms = dataHandler->GetFormArray(RE::FormType::NPC);
			for (const auto npcForm : npcForms)
			{
				auto npc = npcForm->As<RE::TESNPC>();
				// Of the same race and sex as requested.
				if (npc && npc->race == a_race &&
					((a_female && npc->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kFemale)) ||
					(!a_female && npc->actorData.actorBaseFlags.none(RE::ACTOR_BASE_DATA::Flag::kFemale))))
				{
					npcList.emplace_back(npcForm);
				}
			}
		}

		logger::debug("[Proxy] GetAllAppearancePresets: {} playable NPC forms.", npcList.size());
		// Sort by name (A-Z).
		std::sort(npcList.begin(), npcList.end(), [](const RE::TESForm* a_lhs, const RE::TESForm* a_rhs) { 
			auto lName = strlen(a_lhs->GetName()) == 0 ? a_lhs->GetFormEditorID() : a_lhs->GetName();
			auto rName = strlen(a_rhs->GetName()) == 0 ? a_rhs->GetFormEditorID() : a_rhs->GetName();
			return strcmp(lName, rName) < 0; 
		});

		return npcList;
	}

	std::vector<RE::TESForm*> CoopLib::GetAllClasses(RE::StaticFunctionTag*)
	{
		// Get all usable player classes.

		std::vector<RE::TESForm*> classList{};
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			const auto& classForms = dataHandler->GetFormArray(RE::FormType::Class);
			for (const auto classForm : classForms)
			{
				classList.emplace_back(classForm);
			}
		}

		logger::debug("[Proxy] GetAllClasses: {} playable class forms.", classList.size());
		// Sort by name (A-Z).
		std::sort(classList.begin(), classList.end(), [](const RE::TESForm* a_lhs, const RE::TESForm* a_rhs) {
			auto lName = strlen(a_lhs->GetFormEditorID()) == 0 ? a_lhs->GetName() : a_lhs->GetFormEditorID();
			auto rName = strlen(a_rhs->GetFormEditorID()) == 0 ? a_rhs->GetName() : a_rhs->GetFormEditorID();
			return strcmp(lName, rName) < 0;
		});

		return classList;
	}

	std::vector<RE::BSFixedString> CoopLib::GetAllCyclableEmoteIdleEvents(RE::StaticFunctionTag*)
	{
		// Get all assignable cyclable emote idle event names.

		logger::debug("[Proxy] GetAllCyclableEmoteIdleEvents.");
		return ALYSLC::Settings::sEmoteIdlesList;
	}

	std::vector<RE::TESForm*> CoopLib::GetAllSelectableRaces(RE::StaticFunctionTag*, int32_t a_selectableRaceTypeFilter)
	{
		// Get all assignable races based on the given filter.

		std::vector<RE::TESForm*> raceList{};
		SelectableRaceType filter = SelectableRaceType::kAll;
		if (a_selectableRaceTypeFilter >= 0 && a_selectableRaceTypeFilter < !SelectableRaceType::kTotal) 
		{
			filter = static_cast<SelectableRaceType>(a_selectableRaceTypeFilter);
		}
		else
		{
			logger::error("[Proxy] ERR: GetAllSelectableRaces: Type filter {} is invalid. Using playable type filter instead.", a_selectableRaceTypeFilter);
			filter = SelectableRaceType::kPlayable;
		}

		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) 
		{
			if (filter == SelectableRaceType::kAll) 
			{
				// No restrictions, get 'em all.
				const auto& raceForms = dataHandler->GetFormArray(RE::FormType::Race);
				for (const auto raceForm : raceForms)
				{
					raceList.emplace_back(raceForm);
				}
			}
			else if (filter == SelectableRaceType::kHasNPCKeyword) 
			{
				// Must have the NPC keyword.
				const auto& raceForms = dataHandler->GetFormArray(RE::FormType::Race);
				for (const auto raceForm : raceForms)
				{
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
					if (raceForm->As<RE::TESRace>()->GetPlayable())
					{
						raceList.emplace_back(raceForm);
					}
				}
			}
			else if (filter == SelectableRaceType::kUsedByAnyActorBase)
			{
				// Must be used as a race for at least one actor base.
				std::set<RE::TESForm*> raceSet{};
				const auto& npcForms = dataHandler->GetFormArray(RE::FormType::NPC);
				for (const auto npcForm : npcForms)
				{
					auto npc = npcForm->As<RE::TESNPC>();
					if (npc && npc->race)
					{
						raceSet.insert(npc->race);
					}
				}

				for (const auto race : raceSet) 
				{
					raceList.emplace_back(race);
				}
			}
			else
			{
				// Must be used as a race for at least one NPC-keyword actor base.
				std::set<RE::TESForm*> raceSet{};
				const auto& npcForms = dataHandler->GetFormArray(RE::FormType::NPC);
				for (const auto npcForm : npcForms)
				{
					auto npc = npcForm->As<RE::TESNPC>();
					if (npc && npc->race && npc->race->HasKeyword(glob.npcKeyword))
					{
						raceSet.insert(npc->race);
					}
				}

				for (const auto race : raceSet)
				{
					raceList.emplace_back(race);
				}
			}
		}

		logger::debug("[Proxy] GetAllSelectableRaces: {} playable race forms.", raceList.size());
		// Sort by name (A-Z).
		std::sort(raceList.begin(), raceList.end(), [](const RE::TESForm* a_lhs, const RE::TESForm* a_rhs) {
			auto lName = strlen(a_lhs->GetName()) == 0 ? a_lhs->GetFormEditorID() : a_lhs->GetName();
			auto rName = strlen(a_rhs->GetName()) == 0 ? a_rhs->GetFormEditorID() : a_rhs->GetName();
			return strcmp(lName, rName) < 0;
		});

		return raceList;
	}

	std::vector<RE::TESForm*> CoopLib::GetAllVoiceTypes(RE::StaticFunctionTag*, bool a_female)
	{
		std::vector<RE::TESForm*> voiceTypeList{};
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			const auto& voiceTypeForms = dataHandler->GetFormArray(RE::FormType::VoiceType);
			for (const auto voiceTypeForm : voiceTypeForms)
			{
				bool isFemaleVoice = voiceTypeForm->As<RE::BGSVoiceType>()->data.flags.all(RE::VOICE_TYPE_DATA::Flag::kFemale);
				// Match with sex.
				if ((a_female && isFemaleVoice) || (!a_female && !isFemaleVoice))
				{
					voiceTypeList.emplace_back(voiceTypeForm);
				}
			}
		}

		logger::debug("[Proxy] GetAllVoiceTypes: {} usable {} voice type forms.", 
			voiceTypeList.size(),
			a_female ? "female" : "male");
		// Sort by name (A-Z).
		std::sort(voiceTypeList.begin(), voiceTypeList.end(), [](const RE::TESForm* a_lhs, const RE::TESForm* a_rhs) {
			auto lName = strlen(a_lhs->GetName()) == 0 ? a_lhs->GetFormEditorID() : a_lhs->GetName();
			auto rName = strlen(a_rhs->GetName()) == 0 ? a_rhs->GetFormEditorID() : a_rhs->GetName();
			return strcmp(lName, rName) < 0;
		});

		return voiceTypeList;
	}

	std::vector<RE::BSFixedString> CoopLib::GetFavoritedEmoteIdles(RE::StaticFunctionTag*, int32_t a_controllerID)
	{
		// Get list of cyclable emote idle event names for the given player.

		logger::debug("[Proxy] GetFavoritedEmoteIdles: CID {}.", a_controllerID);
		std::vector<RE::BSFixedString> favoritedEmoteIdles;
		if (glob.allPlayersInit && a_controllerID < ALYSLC_MAX_PLAYER_COUNT && glob.coopPlayers[a_controllerID]->isActive)
		{
			const auto& p = glob.coopPlayers[a_controllerID];
			for (auto i = 0; i < p->em->favoritedEmoteIdles.size(); ++i)
			{
				favoritedEmoteIdles.emplace_back(p->em->favoritedEmoteIdles[i]);
			}
		}
		else
		{
			// Return default list if the player CID is invalid.
			for (auto i = 0; i < GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS.size(); ++i)
			{
				favoritedEmoteIdles.emplace_back(GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS[i]);
			}
		}

		return favoritedEmoteIdles;
	}
	
	void CoopLib::RequestMenuControl(RE::StaticFunctionTag*, int32_t a_controllerID, RE::BSFixedString a_menuName)
	{
		// Request control of the given menu for the given player.
		// Reset menu CIDs if the given CID is -1.

		if (glob.globalDataInit && a_controllerID < ALYSLC_MAX_PLAYER_COUNT) 
		{
			if (a_controllerID > -1) 
			{
				bool succ = glob.moarm->InsertRequest(a_controllerID, InputAction::kNone, SteadyClock::now(), a_menuName, RE::ObjectRefHandle(), true);
				logger::debug("[Proxy] RequestMenuControl: req CID {}: menu CID: {}, last menu CID: {}, menu name: {}, MIM running: {}, MIM controller ID: {}. SUCC: {}",
					a_controllerID, glob.menuCID, glob.prevMenuCID, a_menuName, glob.mim->IsRunning(), glob.mim->managerMenuCID, succ);
			}
			else
			{
				// Reset directly if CID is -1.
				GlobalCoopData::ResetMenuCIDs();
				logger::debug("[Proxy] RequestMenuControl: After resetting menu CIDs: menu CID: {}, last menu CID: {}, menu name: {}, MIM running: {}, MIM controller ID: {}.",
					glob.menuCID, glob.prevMenuCID, a_menuName, glob.mim->IsRunning(), glob.mim->managerMenuCID);
			}
		}
	}

	void CoopLib::RequestStateChange(RE::StaticFunctionTag*, int32_t a_controllerID, uint32_t a_newState)
	{
		// Signal all player managers to change state to the given state for the given player.

		logger::debug("[Proxy] RequestStateChange: CID {}'s managers -> state {}.", a_controllerID, a_newState);
		if (glob.allPlayersInit && a_controllerID < ALYSLC_MAX_PLAYER_COUNT && 
			a_newState >= 0 && a_newState < static_cast<uint32_t>(ManagerState::kTotal))
		{
			glob.coopPlayers[a_controllerID]->RequestStateChange(static_cast<ManagerState>(a_newState));
		}
	}

	void CoopLib::RescaleAVsOnBaseSkillAVChange(RE::StaticFunctionTag*, RE::Actor* a_playerActor)
	{
		// Rescale the given player's actor values when their base skill AVs change.
		// Usually occurs on class or race change.

		logger::debug("[Proxy] RescaleAVsOnBaseSkillAVChange: {}.", a_playerActor ? a_playerActor->GetName() : "NONE");
		if (a_playerActor)
		{
			GlobalCoopData::RescaleAVsOnBaseSkillAVChange(a_playerActor);
		}
	}

	void CoopLib::SetCoopPlayerClass(RE::StaticFunctionTag*, RE::Actor* a_playerActor, RE::TESClass* a_class)
	{
		// Set the given player's class to the given class and update base skill actor values.
		// Player and co-op session do not have to be active.

		logger::debug("[Proxy] SetCoopPlayerClass: player {} -> class {}.", 
			a_playerActor ? a_playerActor->GetName() : "NONE", 
			a_class ? a_class->GetName() : "NONE");
		if (glob.globalDataInit && a_playerActor && a_class)
		{
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
				delete script;
			}

			logger::debug("[Proxy] SetCoopPlayerClass: {}'s class is now {}.",
				a_playerActor->GetName(), a_class->GetName());

			// Rescale base skill AVs when done.
			GlobalCoopData::RescaleAVsOnBaseSkillAVChange(a_playerActor);
		}
	}

	void CoopLib::SetCoopPlayerRace(RE::StaticFunctionTag*, RE::Actor* a_playerActor, RE::TESRace* a_race)
	{
		// Set the given player's race to the given race and update base skill actor values.
		// Player and co-op session do not have to be active.

		logger::debug("[Proxy] SetCoopPlayerRace: player {} -> race {}.", 
			a_playerActor ? a_playerActor->GetName() : "NONE", a_race ? a_race->GetName() : "NONE");
		if (glob.globalDataInit && a_playerActor && a_playerActor->race && a_race) 
		{
			a_playerActor->SwitchRace(a_race, false);
			a_playerActor->race = a_race;
			if (auto actorBase = a_playerActor->GetActorBase(); actorBase) 
			{
				actorBase->originalRace = a_race;
				actorBase->race = a_race;
			}

			logger::debug("[Proxy] SetCoopPlayerRace: {}'s race is now {}.",
				a_playerActor->GetName(), a_race->GetName());

			// Rescale base skill AVs when done.
			GlobalCoopData::RescaleAVsOnBaseSkillAVChange(a_playerActor);
		}
	}

	void CoopLib::SetFavoritedEmoteIdles(RE::StaticFunctionTag*, int32_t a_controllerID, std::vector<RE::BSFixedString> a_emoteIdlesList)
	{
		// Update the given player's list of cyclable emote idle event names to the given list.

		logger::debug("[Proxy] SetFavoritedEmoteIdles: CID {}.", a_controllerID);
		if (glob.coopSessionActive && a_controllerID < ALYSLC_MAX_PLAYER_COUNT && glob.coopPlayers[a_controllerID]->isActive) 
		{
			glob.coopPlayers[a_controllerID]->em->SetFavoritedEmoteIdles(a_emoteIdlesList);
		}
	}

	void CoopLib::SetGifteePlayerActor(RE::StaticFunctionTag*, RE::Actor* a_playerActor)
	{
		// When opening the Gift Menu, set the given player actor as the recipient.
		// Setting to None/nullptr clears the giftee player.

		logger::debug("[Proxy] SetGifteePlayerActor: {}.", a_playerActor ? a_playerActor->GetName() : "NONE");
		if ((glob.globalDataInit && glob.coopSessionActive) && (!a_playerActor || GlobalCoopData::IsCoopPlayer(a_playerActor))) 
		{
			glob.mim->gifteePlayerHandle = a_playerActor->GetHandle();
		}
	}

	void CoopLib::SetPartyInvincibility(RE::StaticFunctionTag*, bool a_shouldSet)
	{
		// Enable/disable invincibility for all active players.
		// Play an FX shader while a player is invulnerable.

		logger::debug("[Proxy] SetPartyInvincibility: Toggle {} for all players.", a_shouldSet ? "on" : "off");
		if (glob.allPlayersInit) 
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (p && p->coopActor)
				{
					if (auto actorBase = p->coopActor->GetActorBase(); actorBase)
					{
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
						}
					}
				}
			}
		}
	}

	void CoopLib::SignalWaitForUpdate(RE::StaticFunctionTag*, bool a_shouldDismiss)
	{
		// Either dismiss all active players or just request their managers to wait for refresh.
		// Any active co-op session is tagged as ended.

		logger::debug("[Proxy] SignalWaitForUpdate: Should dismiss all active players: {}.", a_shouldDismiss);
		if (glob.globalDataInit && glob.allPlayersInit)
		{
			for (auto& coopPlayer : glob.coopPlayers)
			{
				if (coopPlayer->isActive)
				{
					if (a_shouldDismiss)
					{
						coopPlayer->DismissPlayer();
					}
					else
					{
						coopPlayer->RequestStateChange(ManagerState::kAwaitingRefresh);
					}
				}
			}

			// Stop co-op camera manager and flag session as ended.
			glob.cam->ToggleCoopCamera(false);
			glob.coopSessionActive = false;
		}
	}

	void CoopLib::ToggleCoopCamera(RE::StaticFunctionTag*, bool a_enable)
	{
		// Toggle the co-op camera on or off.

		logger::debug("[Proxy] ToggleCoopCamera: {}.", a_enable ? "ON" : "OFF");
		if (glob.globalDataInit) 
		{
			glob.cam->ToggleCoopCamera(a_enable);
		}
	}

	void CoopLib::ToggleCoopEntityCollision(RE::StaticFunctionTag*, bool a_enable) 
	{
		// Toggle collision on or off for all loaded active players.

		logger::debug("[Proxy] ToggleCoopEntityCollision: {}.", a_enable ? "ON" : "OFF");
		if (glob.globalDataInit) 
		{
			for (const auto& p : glob.coopPlayers)
			{
				// Enable/disable collision for all active players as well.
				if (p->isActive && p->coopActor)
				{
					logger::debug("[Proxy] ToggleCoopEntityCollision: Toggling to {} for {} (0x{:X})",
						a_enable, p->coopActor->GetName(), p->coopActor->GetFormID());
					p->coopActor->SetCollision(a_enable);
				}
			}
		}
	}

	void CoopLib::ToggleSetupMenuControl(RE::StaticFunctionTag*, int32_t a_controllerID, int32_t a_playerID, bool a_shouldEnter)
	{ 
		// Toggle menu control on or off for the given player when entering/exiting the Co-op Setup/Summoning Menu.

		logger::debug("[Proxy] ToggleSetupMenuControl: CID: {}, PID: {}, should enter: {}.", a_controllerID, a_playerID, a_shouldEnter);
		if ((glob.globalDataInit && glob.mim) && 
			(a_controllerID > -1 && a_controllerID < ALYSLC_MAX_PLAYER_COUNT) && 
			(a_playerID > -1 && a_playerID < ALYSLC_MAX_PLAYER_COUNT)) 
		{
			// Set the opened menu name and type.
			glob.mim->SetOpenedMenu(GlobalCoopData::SETUP_MENU_NAME, a_shouldEnter);
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[Proxy] ToggleCoopPlayerMenuMode: P{}: Try to lock: 0x{:X}.", a_playerID + 1, hash);
			{
				if (a_shouldEnter)
				{
					// Set menu CID directly to the requesting player's.
					GlobalCoopData::SetMenuCIDs(a_controllerID);
					// Signal MIM to start running.
					glob.mim->ToggleCoopPlayerMenuMode(a_controllerID, a_playerID);
				}
				else
				{
					// Reset menu CIDs.
					GlobalCoopData::ResetMenuCIDs();
					// Signal MIM to pause and reset both CID and PID.
					glob.mim->ToggleCoopPlayerMenuMode(-1, -1);
				}
			}
		}
		else
		{
			logger::error("[Proxy] ERR: ToggleSetupMenuControl: Global co-op data not initialized: {}, MIM invalid: {}, CID invalid: {}, player ID invalid: {}.",
				!glob.globalDataInit, !glob.mim,
				(a_controllerID <= -1 && a_controllerID >= ALYSLC_MAX_PLAYER_COUNT),
				(a_playerID <= -1 && a_playerID >= ALYSLC_MAX_PLAYER_COUNT));
		}
	}

	void CoopLib::UpdateAllSerializedCompanionPlayerFIDKeys(RE::StaticFunctionTag*)
	{
		// Update all serialized player FID keys.

		logger::debug("[Proxy] UpdateAllSerializedCompanionPlayerFIDKeys.");
		if (glob.globalDataInit)
		{
			GlobalCoopData::UpdateAllSerializedCompanionPlayerFIDKeys();
		}
	}

	void CoopLib::UpdateGenderAndBody(RE::StaticFunctionTag*, int32_t a_controllerID, bool a_setFemale, bool a_setOppositeGenderAnims)
	{
		// Update the given player's sex and gendered animations.
		// NOTE: Precondition to update properly: any race swap must be fully completed first.

		logger::debug("[Proxy] UpdateGenderAndBody: CID: {}, set female: {}, set opposite gender anims: {}.",
			a_controllerID, a_setFemale, a_setOppositeGenderAnims);
		if (glob.allPlayersInit && a_controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			glob.coopPlayers[a_controllerID]->UpdateGenderAndBody(a_setFemale, a_setOppositeGenderAnims);
		}
	}

	void CoopLib::Log(RE::StaticFunctionTag*, RE::BSFixedString a_message)
	{
		// Script request to log a message to this mod's log file 'ALYSLC.log'.

		logger::debug("{}", a_message.c_str());
	}

	//=================================================================================================================================================================================================
	//[Debug Functions]
	//=================================================================================================================================================================================================

	void CoopLib::Debug::AssignPlayer1CID(RE::StaticFunctionTag*)
	{
		// Open a prompt which asks P1 to press a certain button on their controller
		// to assign their controller as P1's.

		logger::debug("[Proxy] AssignPlayer1CID.");
		if (glob.globalDataInit)
		{
			glob.taskRunner->AddTask([]() { GlobalCoopData::PromptForPlayer1CIDTask(); });
		}
	}

	void CoopLib::Debug::EnableGodModeForAllCoopPlayers(RE::StaticFunctionTag*)
	{
		// Enable god mode for all active players.

		logger::debug("[Proxy] EnableGodModeForAllCoopPlayers.");
		if (glob.globalDataInit && glob.coopSessionActive)
		{
			glob.ToggleGodModeForAllPlayers(true);
		}
	}

	void CoopLib::Debug::EnableGodModeForPlayer(RE::StaticFunctionTag*, int32_t a_controllerID)
	{
		// Enable god mode for a specific player.

		logger::debug("[Proxy] EnableGodModeForPlayer: CID: {}.", a_controllerID);
		if (glob.globalDataInit && glob.coopSessionActive && a_controllerID != -1 && a_controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			glob.ToggleGodModeForPlayer(a_controllerID, true);
		}
	}

	void CoopLib::Debug::DisableGodModeForAllCoopPlayers(RE::StaticFunctionTag*)
	{
		// Disable god mode for all players.

		logger::debug("[Proxy] DisableGodModeForAllCoopPlayers.");
		if (glob.globalDataInit && glob.coopSessionActive)
		{
			glob.ToggleGodModeForAllPlayers(false);
		}
	}

	void CoopLib::Debug::DisableGodModeForPlayer(RE::StaticFunctionTag*, int32_t a_controllerID)
	{
		// Disable god mode for a specific player.

		logger::debug("[Proxy] DisableGodModeForPlayer: CID: {}.", a_controllerID);
		if (glob.globalDataInit && glob.coopSessionActive && a_controllerID != -1 && a_controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			glob.ToggleGodModeForPlayer(a_controllerID, false);
		}
	}

	void CoopLib::Debug::MoveAllCoopActorsToP1(RE::StaticFunctionTag*)
	{
		// Move all other players to P1.

		logger::debug("[Proxy] MoveAllCoopActorsToP1");
		if (glob.globalDataInit && glob.coopSessionActive) 
		{
			if (auto taskInterface = SKSE::GetTaskInterface(); taskInterface)
			{
				for (const auto& p : glob.coopPlayers)
				{
					if (p && p->isActive && !p->isPlayer1)
					{
						taskInterface->AddTask
						(
							[&p]() 
							{
								p->coopActor->MoveTo(glob.player1Actor.get());
							}
						);
					}
				}
			}
		}
	}

	void CoopLib::Debug::ReEquipHandForms(RE::StaticFunctionTag*, int32_t a_controllerID)
	{
		// Re-equip the player's desired hand forms (weapons/magic/armor).

		logger::debug("[Proxy] RefreshAllPlayerManagers: CID: {}.", a_controllerID);
		if (glob.allPlayersInit && glob.coopSessionActive && a_controllerID != -1 && a_controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			glob.coopPlayers[a_controllerID]->em->ReEquipHandForms();
		}
	}

	void CoopLib::Debug::RefreshAllPlayerManagers(RE::StaticFunctionTag*) 
	{
		// Refresh data for all active players' managers.
		logger::debug("[Proxy] RefreshAllPlayerManagers.");

		if (glob.globalDataInit && glob.coopSessionActive)
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (p && p->isActive)
				{
					p->taskRunner->AddTask([&p]() { p->RefreshPlayerManagersTask(); });
				}
			}
		}
	}

	void CoopLib::Debug::RefreshPlayerManagers(RE::StaticFunctionTag*, int32_t a_controllerID)
	{
		// Refresh data for all of the given player's managers.

		logger::debug("[Proxy] RefreshPlayerManagers: CID: {}.", a_controllerID);
		if (glob.globalDataInit && glob.coopSessionActive && a_controllerID != -1 && a_controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			if (const auto& p = glob.coopPlayers[a_controllerID]; p && p->isActive)
			{
				p->taskRunner->AddTask([&p]() { p->RefreshPlayerManagersTask(); });
			}
		}
	}

	void CoopLib::Debug::ResetCoopCompanion(RE::StaticFunctionTag*, int32_t a_controllerID, bool a_unequipAll, bool a_reattachHavok)
	{
		// Hard reset a companion player:
		// Stop movement, clear movement offset, sheathe weapons/magic,
		// revert transformation, unequip hand forms, resurrect,
		// disable, re-enable, re-equip hand forms, reset I-frames flag, and re-enable movement.
		// Can optionally request to unequip all or re-attach havok.

		logger::debug("[Proxy] ResetCoopCompanion: CID: {}, unequip all: {}, re-attach havok: {}.",
			a_controllerID, a_unequipAll, a_reattachHavok);
		if (glob.globalDataInit && glob.allPlayersInit && a_controllerID != -1 && a_controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			if (const auto& p = glob.coopPlayers[a_controllerID]; p && p->isActive)
			{
				p->taskRunner->AddTask
				(
					[&p, a_unequipAll, a_reattachHavok]() 
					{ 
						p->ResetCompanionPlayerStateTask(a_unequipAll, a_reattachHavok); 
					}
				);
			}
		}
	}

	void CoopLib::Debug::ResetPlayer1AndCamera(RE::StaticFunctionTag*) 
	{
		// Stop P1's managers, disable the co-op camera, and stop the menu input manager.

		logger::debug("[Proxy] ResetPlayer1AndCamera.");
		if (glob.globalDataInit) 
		{
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
	}

	void CoopLib::Debug::ResetPlayer1State(RE::StaticFunctionTag*)
	{
		// Hard reset for P1:
		// Resurrect P1, re-attach havok, remove paralysis and fix ragdoll, sheathe weapons/magic,
		// revert any active transformation, re-equip hand forms, and reset I-frames flag.

		logger::debug("[Proxy] ResetPlayer1State.");
		if (glob.globalDataInit && glob.allPlayersInit) 
		{
			const auto& p = glob.coopPlayers[glob.player1CID];
			p->ResetPlayer1();
		}
	}

	void CoopLib::Debug::RestartCoopCamera(RE::StaticFunctionTag*)
	{
		// Toggle the co-op camera off and then on again.

		logger::debug("[Proxy] RestartCoopCamera.");
		if (glob.globalDataInit && glob.coopSessionActive)
		{
			glob.taskRunner->AddTask([]() { GlobalCoopData::RestartCoopCameraTask(); });
		}
	}

	void CoopLib::Debug::StopAllCombatOnCoopPlayers(RE::StaticFunctionTag*, bool a_clearBounties) 
	{
		// Stop combat on all active players, optionally clearing all bounties to get off scot-free.

		logger::debug("[Proxy] StopAllCombatOnCoopPlayers: Clear bounties too: {}", a_clearBounties);
		GlobalCoopData::StopAllCombatOnCoopPlayers(false, std::move(a_clearBounties));
	}

	void CoopLib::Debug::StopMenuInputManager(RE::StaticFunctionTag*)
	{
		// Signal the menu input manager to stop running, returning menu control to P1.

		logger::debug("[Proxy] StopMenuInputManager. Current menu-related CIDs: menu: {}, last menu: {}, manager: {}.",
			glob.menuCID, glob.prevMenuCID, glob.mim->managerMenuCID);
		GlobalCoopData::StopMenuInputManager();
	}

	

	void CoopLib::Settings::OnConfigClose(RE::TESQuest*)
	{
		// Import all settings when this mod's MCM closes.

		logger::debug("[Proxy] OnConfigClose.");
		ALYSLC::Settings::ImportAllSettings();
	}

	//=================================================================================================================================================================================================
	// Register papyrus functions
	//=================================================================================================================================================================================================
	bool CoopLib::RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm)
	{
		// Registered functions for ALYSLC's scripts.
		a_vm->RegisterFunction("ChangeCoopSessionState"s, "ALYSLC"s, ChangeCoopSessionState);
		a_vm->RegisterFunction("CopyNPCAppearanceToPlayer"s, "ALYSLC"s, CopyNPCAppearanceToPlayer);
		a_vm->RegisterFunction("GetAllAppearancePresets"s, "ALYSLC"s, GetAllAppearancePresets);
		a_vm->RegisterFunction("GetAllClasses"s, "ALYSLC"s, GetAllClasses);
		a_vm->RegisterFunction("GetAllCyclableEmoteIdleEvents"s, "ALYSLC"s, GetAllCyclableEmoteIdleEvents);
		a_vm->RegisterFunction("GetAllSelectableRaces"s, "ALYSLC"s, GetAllSelectableRaces);
		a_vm->RegisterFunction("GetAllVoiceTypes"s, "ALYSLC"s, GetAllVoiceTypes);
		a_vm->RegisterFunction("GetConnectedCoopControllerIDs"s, "ALYSLC"s, GetConnectedCoopControllerIDs);
		a_vm->RegisterFunction("GetFavoritedEmoteIdles"s, "ALYSLC"s, GetFavoritedEmoteIdles);
		a_vm->RegisterFunction("InitializeCoop"s, "ALYSLC"s, InitializeCoop);
		a_vm->RegisterFunction("InitializeGlobalData"s, "ALYSLC"s, InitializeGlobalData);
		a_vm->RegisterFunction("RequestMenuControl"s, "ALYSLC"s, RequestMenuControl);
		a_vm->RegisterFunction("RequestStateChange"s, "ALYSLC"s, RequestStateChange);
		a_vm->RegisterFunction("RescaleAVsOnBaseSkillAVChange"s, "ALYSLC"s, RescaleAVsOnBaseSkillAVChange);
		a_vm->RegisterFunction("SetCoopPlayerClass"s, "ALYSLC"s, SetCoopPlayerClass);
		a_vm->RegisterFunction("SetCoopPlayerRace"s, "ALYSLC"s, SetCoopPlayerRace);
		a_vm->RegisterFunction("SetFavoritedEmoteIdles"s, "ALYSLC"s, SetFavoritedEmoteIdles);
		a_vm->RegisterFunction("SetGifteePlayerActor"s, "ALYSLC"s, SetGifteePlayerActor);
		a_vm->RegisterFunction("SetPartyInvincibility"s, "ALYSLC"s, SetPartyInvincibility);
		a_vm->RegisterFunction("SignalWaitForUpdate"s, "ALYSLC"s, SignalWaitForUpdate);
		a_vm->RegisterFunction("ToggleCoopCamera"s, "ALYSLC"s, ToggleCoopCamera);
		a_vm->RegisterFunction("ToggleCoopEntityCollision"s, "ALYSLC"s, ToggleCoopEntityCollision);
		a_vm->RegisterFunction("ToggleSetupMenuControl"s, "ALYSLC"s, ToggleSetupMenuControl);
		a_vm->RegisterFunction("UpdateAllSerializedCompanionPlayerFIDKeys"s, "ALYSLC"s, UpdateAllSerializedCompanionPlayerFIDKeys);
		a_vm->RegisterFunction("UpdateGenderAndBody"s, "ALYSLC"s, UpdateGenderAndBody);
		a_vm->RegisterFunction("Log"s, "ALYSLC"s, Log);

		// Debug menu functions.
		a_vm->RegisterFunction("AssignPlayer1CID"s, "ALYSLC"s, Debug::AssignPlayer1CID);
		a_vm->RegisterFunction("EnableGodModeForAllCoopPlayers"s, "ALYSLC"s, Debug::EnableGodModeForAllCoopPlayers);
		a_vm->RegisterFunction("EnableGodModeForPlayer"s, "ALYSLC"s, Debug::EnableGodModeForPlayer);
		a_vm->RegisterFunction("DisableGodModeForAllCoopPlayers"s, "ALYSLC"s, Debug::DisableGodModeForAllCoopPlayers);
		a_vm->RegisterFunction("DisableGodModeForPlayer"s, "ALYSLC"s, Debug::DisableGodModeForPlayer);
		a_vm->RegisterFunction("MoveAllCoopActorsToP1"s, "ALYSLC"s, Debug::MoveAllCoopActorsToP1);
		a_vm->RegisterFunction("ReEquipHandForms"s, "ALYSLC"s, Debug::ReEquipHandForms);
		a_vm->RegisterFunction("RefreshAllPlayerManagers"s, "ALYSLC"s, Debug::RefreshAllPlayerManagers);
		a_vm->RegisterFunction("RefreshPlayerManagers"s, "ALYSLC"s, Debug::RefreshPlayerManagers);
		a_vm->RegisterFunction("ResetCoopCompanion"s, "ALYSLC"s, Debug::ResetCoopCompanion);
		a_vm->RegisterFunction("ResetPlayer1AndCamera"s, "ALYSLC"s, Debug::ResetPlayer1AndCamera);
		a_vm->RegisterFunction("ResetPlayer1State"s, "ALYSLC"s, Debug::ResetPlayer1State);
		a_vm->RegisterFunction("RestartCoopCamera"s, "ALYSLC"s, Debug::RestartCoopCamera);
		a_vm->RegisterFunction("StopAllCombatOnCoopPlayers"s, "ALYSLC"s, Debug::StopAllCombatOnCoopPlayers);
		a_vm->RegisterFunction("StopMenuInputManager"s, "ALYSLC"s, Debug::StopMenuInputManager);

		// MCM settings.
		a_vm->RegisterFunction("OnConfigClose"s, "__ALYSLC_ConfigMenu"s, Settings::OnConfigClose);

		// TODO: Framework functions for any scripts wishing to access/modify ALYSLC data.
		return true;
	}
}

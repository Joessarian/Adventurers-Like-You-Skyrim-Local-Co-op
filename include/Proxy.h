#pragma once

#include <mutex>
#include <thread>
#include <unordered_map>
//#include <PCH.cpp>
#include <Windows.h>

#include <Player.h>
#include <GlobalCoopData.h>

namespace ALYSLC
{
	// Interfaces with Papyrus to call the corresponding functions within
	// the plugin's codebase.
	namespace CoopLib
	{
		//========================================================================================================================================================================
		// Papyrus functions
		//========================================================================================================================================================================
		
		//========================================================================================================================================================================
		// Initialization functions defined in proper order of execution.
		//========================================================================================================================================================================

		// Set global co-op data after the player loads a save.
		// Return true if global data was initialized for the first time.
		bool InitializeGlobalData(RE::StaticFunctionTag*, RE::BGSRefAlias* a_player1Ref);

		// Setup controller data for all connected controllers and return a list of controller IDs
		// for all active controllers. Player 1's CID is always first.
		std::vector<std::uint32_t> GetConnectedCoopControllerIDs(RE::StaticFunctionTag*);

		// Initializes/updates all co-op players with the given data.
		// Returns true if the co-op session was initialized successfully.
		bool InitializeCoop(RE::StaticFunctionTag*, uint32_t a_numCompanions, std::vector<uint32_t> a_controllerIDs, std::vector<RE::Actor*> a_coopActors, std::vector<uint32_t> a_packageFormListIndicesList);

		//========================================================================================================================================================================
		// Post-summoning Papyrus functions listed in alphabetical order
		//========================================================================================================================================================================

		// Start a new co-op session or stop an active co-op session.
		void ChangeCoopSessionState(RE::StaticFunctionTag*, bool a_start);
		
		// Copy base NPC's appearance to the player. Set opposite gender animations if necessary.
		void CopyNPCAppearanceToPlayer(RE::StaticFunctionTag*, int32_t a_controllerID, RE::TESNPC* a_baseToCopy, bool a_setOppositeGenderAnims);

		// Get all actor base NPC appearance presets, narrowed down by race and sex.
		std::vector<RE::TESForm*> GetAllAppearancePresets(RE::StaticFunctionTag*, RE::TESRace* a_race, bool a_female);

		// Get all usable classes.
		std::vector<RE::TESForm*> GetAllClasses(RE::StaticFunctionTag*);

		// Get all assignable emote idle animation event names.
		std::vector<RE::BSFixedString> GetAllCyclableEmoteIdleEvents(RE::StaticFunctionTag*);

		// Get all races narrowed down by the given filter.
		std::vector<RE::TESForm*> GetAllSelectableRaces(RE::StaticFunctionTag*, int32_t a_selectableRaceTypeFilter);

		// Get all voice types narrowed down by sex.
		std::vector<RE::TESForm*> GetAllVoiceTypes(RE::StaticFunctionTag*, bool a_female);
		
		// Get the given player's assigned list of cyclable emote idle event names.
		std::vector<RE::BSFixedString> GetFavoritedEmoteIdles(RE::StaticFunctionTag*, int32_t a_controllerID);

		// Request control of the given menu for the given player.
		void RequestMenuControl(RE::StaticFunctionTag*, int32_t a_controllerID, RE::BSFixedString a_menuName);

		// Signal all player managers for the given player to change state to the given state.
		void RequestStateChange(RE::StaticFunctionTag*, int32_t a_controllerID, uint32_t a_newState);

		// Rescale the given player's actor values when their base skill actor values change:
		// for example, when the player changes their race or their class.
		// Will update their saved base skill AVs and re-apply their skill progression increments
		// on top of each skill.
		void RescaleAVsOnBaseSkillAVChange(RE::StaticFunctionTag*, RE::Actor* a_playerActor);
		
		// Set the given player's class to the given class and update base skill actor values.
		// Player and co-op session do not have to be active.
		void SetCoopPlayerClass(RE::StaticFunctionTag*, RE::Actor* a_playerActor, RE::TESClass* a_class);

		// Set the given player's race to the given race and update base skill actor values.
		// Player and co-op session do not have to be active.
		void SetCoopPlayerRace(RE::StaticFunctionTag*, RE::Actor* a_playerActor, RE::TESRace* a_race);
		
		// Update the given player's list of cyclable emote idle event names to the given list.
		void SetFavoritedEmoteIdles(RE::StaticFunctionTag*, int32_t a_controllerID, std::vector<RE::BSFixedString> a_emoteIdlesList);

		// When opening the Gift Menu, set the given player actor as the recipient.
		// Setting to None/nullptr clears the giftee player.
		void SetGifteePlayerActor(RE::StaticFunctionTag*, RE::Actor* a_playerActor);

		// Enable/disable invincibility for all active players.
		void SetPartyInvincibility(RE::StaticFunctionTag*, bool a_shouldSet);

		// Either dismiss all active players or just request their managers to wait for refresh.
		// Any active co-op session is flagged as ended.
		void SignalWaitForUpdate(RE::StaticFunctionTag*, bool a_shouldDismiss);

		// Toggle the co-op camera on or off.
		void ToggleCoopCamera(RE::StaticFunctionTag*, bool a_enable);

		// Toggle collision on or off for all active players.
		void ToggleCoopEntityCollision(RE::StaticFunctionTag*, bool a_enable);

		// Toggle menu control on or off for the given player when entering/exiting the Summoning Menu.
		void ToggleSetupMenuControl(RE::StaticFunctionTag*, int32_t a_controllerID, int32_t a_playerID, bool a_shouldEnter);

		// Update all serialized player FID keys.
		// NOTE: Serialized data should contain all players' FID keys before the Summoning Menu is opened.
		void UpdateAllSerializedCompanionPlayerFIDKeys(RE::StaticFunctionTag*);

		// Update the given player's sex and gendered animations.
		void UpdateGenderAndBody(RE::StaticFunctionTag*, int32_t a_controllerID, bool a_setFemale, bool a_setOppositeGenderAnims);


		//========================================================================================================================

		// Debug function for scripts to write log messages to ALYSLC_(SE/AE).log.
		void Log(RE::StaticFunctionTag*, RE::BSFixedString a_message);

		// Register all papyrus functions.
		bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm);

		// Debug-related functions called from a UIExtensions menu.
		namespace Debug
		{
			// Open a prompt which asks P1 to press a certain button on their controller
			// to assign their controller as P1's.
			void AssignPlayer1CID(RE::StaticFunctionTag*);

			// Disable god mode for all players.
			void DisableGodModeForAllCoopPlayers(RE::StaticFunctionTag*);

			// Disable god mode for a specific player.
			void DisableGodModeForPlayer(RE::StaticFunctionTag*, int32_t a_controllerID);

			// Enable god mode for all active players.
			void EnableGodModeForAllCoopPlayers(RE::StaticFunctionTag*);

			// Enable god mode for a specific player.
			void EnableGodModeForPlayer(RE::StaticFunctionTag*, int32_t a_controllerID);

			// Move all other players to P1.
			void MoveAllCoopActorsToP1(RE::StaticFunctionTag*);

			// Re-equip the player's desired hand forms (weapons/magic/armor).
			void ReEquipHandForms(RE::StaticFunctionTag*, int32_t a_controllerID);

			// Refresh data for all active players' managers.
			void RefreshAllPlayerManagers(RE::StaticFunctionTag*);

			// Refresh data for all of the given player's managers.
			void RefreshPlayerManagers(RE::StaticFunctionTag*, int32_t a_controllerID);

			// Hard reset a companion player:
			// Stop movement, clear movement offset, sheathe weapons/magic,
			// revert transformation, unequip hand forms, resurrect,
			// disable, re-enable, re-equip hand forms, reset dash dodge I-frames flag, and re-enable movement.
			// Can optionally request to unequip all or re-attach havok.
			void ResetCoopCompanion(RE::StaticFunctionTag*, int32_t a_controllerID, bool a_unequipAll, bool a_reattachHavok);

			// Stop P1's managers, disable the co-op camera, and stop the menu input manager.
			void ResetPlayer1AndCamera(RE::StaticFunctionTag*);

			// Hard reset for P1:
			// Resurrect P1, re-attach havok, remove paralysis and fix ragdoll, sheathe weapons/magic,
			// revert any active transformation, re-equip hand forms, and reset dash dodge I-frames flag.
			void ResetPlayer1State(RE::StaticFunctionTag*);

			// Toggle the co-op camera off and then on again.
			void RestartCoopCamera(RE::StaticFunctionTag*);

			// Stop combat on all active players, optionally clearing all bounties to get off scot-free.
			void StopAllCombatOnCoopPlayers(RE::StaticFunctionTag*, bool a_clearBounties);

			// Signal the menu input manager to stop running, returning menu control to P1.
			void StopMenuInputManager(RE::StaticFunctionTag*);
		}

		// MCM functions.
		namespace Settings 
		{
			// Import all settings when this mod's MCM closes.
			void OnConfigClose(RE::TESQuest*);
		}
	};
}



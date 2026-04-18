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
		//=========================================================================================
		// Papyrus functions																	  
		//=========================================================================================
																								  
		//=========================================================================================
		// Initialization functions defined in proper order of execution.						  
		//=========================================================================================

		// Set global co-op data after the player loads a save.
		// Return true if global data was initialized for the first time.
		bool InitializeGlobalData(RE::StaticFunctionTag*, RE::BGSRefAlias* a_player1Ref);
		
		// Setup input device data for all connected devices and return a list of device IDs
		// for all active devices. P1's DID is always first.
		std::vector<std::uint32_t> GetConnectedInputDeviceIDs(RE::StaticFunctionTag*);

		// Initializes/updates all co-op players with the given data.
		// Returns true if the co-op session was initialized successfully.
		bool InitializeCoopPlayers
		(
			RE::StaticFunctionTag*, 
			uint32_t a_numCompanions, 
			std::vector<uint32_t> a_deviceIDs, 
			std::vector<RE::Actor*> a_coopActors
		);

		//=========================================================================================
		// Post-summoning Papyrus functions listed in alphabetical order
		//=========================================================================================

		// Start a new co-op session or stop an active co-op session.
		void ChangeCoopSessionState(RE::StaticFunctionTag*, bool a_start);
		
		// Toggle collisions on for all active players.
		void EnableCoopEntityCollision(RE::StaticFunctionTag*);

		// Get all actor base NPC appearance presets, narrowed down by race and sex.
		std::vector<RE::TESForm*> GetAllAppearancePresets
		(
			RE::StaticFunctionTag*, RE::TESRace* a_race, bool a_female
		);

		// Get all usable classes.
		std::vector<RE::TESForm*> GetAllClasses(RE::StaticFunctionTag*);

		// Get all assignable emote idle animation event names.
		std::vector<RE::BSFixedString> GetAllCyclableEmoteIdleEvents(RE::StaticFunctionTag*);

		// Get all races narrowed down by the given filter.
		std::vector<RE::TESForm*> GetAllSelectableRaces
		(
			RE::StaticFunctionTag*, int32_t a_selectableRaceTypeFilter
		);

		// Get all voice types narrowed down by sex.
		std::vector<RE::TESForm*> GetAllVoiceTypes(RE::StaticFunctionTag*, bool a_female);

		// Get a list of all playable companion players' characters.
		std::vector<RE::Actor*> GetCompanionPlayerCharacters(RE::StaticFunctionTag*);
		
		// Get the given race's default male/female voice type.
		RE::BGSVoiceType* GetDefaultRacialVoiceType
		(
			RE::StaticFunctionTag*, RE::TESRace* a_race, bool a_female
		);
		
		// Get a list of all exported RaceMenu presets' names.
		// Exported presets are located in the "\Data\SKSE\Plugins\CharGen\Exported' folder.
		std::vector<RE::BSFixedString> GetExportedRaceMenuPresetFileNames(RE::StaticFunctionTag*);

		// Get the given player's assigned list of cyclable emote idle event names.
		std::vector<RE::BSFixedString> GetFavoritedEmoteIdles
		(
			RE::StaticFunctionTag*, int32_t a_playerID
		);

		// Request control of the given menu for the given player.
		void RequestMenuControl
		(
			RE::StaticFunctionTag*,
			int32_t a_deviceID, 
			int32_t a_playerID, 
			RE::BSFixedString a_menuName
		);

		// Signal all player managers for the given player to change state to the given state.
		void RequestStateChange
		(
			RE::StaticFunctionTag*, int32_t a_playerID, uint32_t a_newState
		);

		// Rescale the given player's actor values when their base skill actor values change:
		// for example, when the player changes their race or their class.
		// Will update their saved base skill AVs and re-apply their skill progression increments
		// on top of each skill.
		void RescaleAVsOnBaseSkillAVChange(RE::StaticFunctionTag*, RE::Actor* a_playerActor);
		
		// Set the given player's class to the given class and update base skill actor values.
		// Player and co-op session do not have to be active.
		void SetCoopPlayerClass
		(
			RE::StaticFunctionTag*,
			RE::Actor* a_playerActor, 
			RE::TESClass* a_class,
			bool a_rescaleActorValues
		);

		// Set the given player's race to the given race and update base skill actor values.
		// Player and co-op session do not have to be active.
		void SetCoopPlayerRace
		(
			RE::StaticFunctionTag*,
			RE::Actor* a_playerActor,
			RE::TESRace* a_race,
			bool a_rescaleActorValues
		);

		// Update the given player's list of cyclable emote idle event names to the given list.
		void SetFavoritedEmoteIdles
		(
			RE::StaticFunctionTag*,
			int32_t a_playerID,
			std::vector<RE::BSFixedString> a_emoteIdlesList
		);

		// When opening the Gift Menu, set the given player actor as the recipient.
		// Setting to None/nullptr clears the giftee player.
		void SetGifteePlayerActor(RE::StaticFunctionTag*, RE::Actor* a_playerActor);

		// Set the 'is summoning' flag, which indicates whether players 
		// are summoning their characters for co-op.
		void SetIsSummoningFlag(RE::StaticFunctionTag*, bool a_set);

		// Enable/disable invincibility for all active players.
		void SetPartyInvincibility(RE::StaticFunctionTag*, bool a_shouldSet);

		// Either dismiss all active players or just request their managers to wait for refresh.
		// Any active co-op session is flagged as ended.
		void SignalWaitForUpdate(RE::StaticFunctionTag*, bool a_shouldDismiss);
		
		// Teleport the given player actor to P1 or their editor location.
		void TeleportToP1OrAway(RE::StaticFunctionTag*, RE::Actor* a_playerActor, bool a_toP1);

		// Teleport the player with the given PID to an actor.
		void TeleportToPlayerToActor
		(
			RE::StaticFunctionTag*, const int32_t a_playerID, RE::Actor* a_teleportTarget
		);

		// Toggle the co-op camera on or off.
		void ToggleCoopCamera(RE::StaticFunctionTag*, bool a_enable);

		// Toggle menu control on or off for the given player 
		// when entering/exiting the Summoning Menu.
		void ToggleSetupMenuControl
		(
			RE::StaticFunctionTag*, int32_t a_deviceID, int32_t a_playerID, bool a_shouldEnter
		);

		// Update all serialized player FID keys.
		// NOTE:
		// Serialized data should contain all players' FID keys 
		// before the Summoning Menu is opened.
		void UpdateAllCompanionPlayerSerializationIDs(RE::StaticFunctionTag*);
		
		//=========================================================================================

		// Debug functions for scripts to write debug/error log messages to ALYSLC.log.
		void Log(RE::StaticFunctionTag*, RE::BSFixedString a_message);
		void LogError(RE::StaticFunctionTag*, RE::BSFixedString a_message);

		// Register all papyrus functions.
		bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm);

		// Functions for importing/exporting player appearances/presets 
		// created directly from the RaceMenu.
		namespace CharacterCustomization
		{
			// Copy base NPC's appearance to the player. 
			// Set opposite gender animations if necessary.
			void CopyNPCAppearanceToPlayer
			(
				RE::StaticFunctionTag*, 
				int32_t a_playerID,
				RE::TESNPC* a_baseToCopy, 
				bool a_setOppositeGenderAnims
			);

			// Export P1's appearance data to the given actor's actorbase.
			void ExportP1ActorBaseAppearanceData
			(
				RE::StaticFunctionTag*, RE::Actor* a_presetCharacter
			);

			// Return true if RaceMenu mod by expired6978 is installed.
			bool IsRaceMenuInstalled(RE::StaticFunctionTag*);

			// Load the exported character preset for the given player character.
			void LoadPlayerCharacterPreset
			(
				RE::StaticFunctionTag*, RE::Actor* a_fromPresetCharacter
			);

			// Load the preset with the given name onto the given player character.
			void LoadPlayerCharacterPresetWithName
			(
				RE::StaticFunctionTag*, RE::Actor* a_toCharacter, RE::BSFixedString a_presetName
			);
			
			// NOTE: Unused for now until I can figure out how to seamlessly 
			// and automatically re-import P1's character preset 
			// after another player customizes their character.
			// Set P1's gender and race, and save skill levels and perks
			// in preparation for opening the Race Menu.
			void OnPreRaceMenu(RE::StaticFunctionTag*, RE::TESRace* a_newRace, bool a_setFemale);
			
			// Save P1's name, race, and appearance as the given player's preset.
			void SavePlayerCharacterPreset(RE::StaticFunctionTag*, RE::Actor* a_toPresetCharacter);

			// Import default racial headparts, update gender, animations, skin tone,
			// and refresh the player actor's 3D model when done.
			void SetDefaultRacialAppearance
			(
				RE::StaticFunctionTag*,
				int32_t a_playerID,
				bool a_setFemale,
				bool a_setOppositeGenderAnims
			);
		};

		// Debug-related functions called from a UIExtensions menu.
		namespace Debug
		{
			// Open a prompt which asks P1 to press a certain button on their controller
			// to assign their controller as P1's.
			void AssignPlayer1CID(RE::StaticFunctionTag*);

			// Disable god mode for all players.
			void DisableGodModeForAllCoopPlayers(RE::StaticFunctionTag*);

			// Disable god mode for a specific player.
			void DisableGodModeForPlayer(RE::StaticFunctionTag*, int32_t a_playerID);

			// Enable god mode for all active players.
			void EnableGodModeForAllCoopPlayers(RE::StaticFunctionTag*);

			// Enable god mode for a specific player.
			void EnableGodModeForPlayer(RE::StaticFunctionTag*, int32_t a_playerID);

			// Move all other players to the given player.
			void MoveAllPlayersToPlayer(RE::StaticFunctionTag*, RE::Actor* a_playerActor);

			// Re-equip the player's desired hand forms (weapons/magic/armor).
			void ReEquipHandForms(RE::StaticFunctionTag*, int32_t a_playerID);

			// Refresh data for all active players' managers.
			void RefreshAllPlayerManagers(RE::StaticFunctionTag*);

			// Refresh data for all of the given player's managers.
			void RefreshPlayerManagers(RE::StaticFunctionTag*, int32_t a_playerID);

			// Hard reset a companion player:
			// Stop movement,
			// clear movement offset, 
			// sheathe weapons/magic,
			// revert transformation,
			// unequip hand forms, 
			// resurrect,
			// disable, 
			// re-enable, 
			// re-equip hand forms, 
			// reset dash dodge I-frames flag, 
			// and re-enable movement.
			// Can optionally request to unequip all or re-attach havok.
			void ResetCoopCompanion
			(
				RE::StaticFunctionTag*,
				int32_t a_playerID, 
				bool a_unequipAll,
				bool a_reattachHavok
			);

			// Stop P1's managers, disable the co-op camera, and stop the menu input manager.
			void ResetPlayer1AndCamera(RE::StaticFunctionTag*);

			// Hard reset for P1:
			// Resurrect P1,
			// re-attach havok, 
			// remove paralysis and fix ragdoll, 
			// sheathe weapons/magic,
			// revert any active transformation, 
			// re-equip hand forms, 
			// and reset dash dodge I-frames flag.
			void ResetPlayer1State(RE::StaticFunctionTag*);

			// First, reset HMS actor values to their base values for the provided player.
			// Then, remove all perks and refund all allotted perk points for the given player,
			// allowing them to completely respec their character once they re-enter the StatsMenu.
			// Also remove all shared perks from all active players.
			// Since all shared perks are removed, all other active players are also
			// refunded any shared perk points and can re-use them as they see fit.
			// Skill and HMS levels remain unchanged.
			void RespecPlayer(RE::StaticFunctionTag*, int32_t a_playerID);

			// Toggle the co-op camera off and then on again.
			void RestartCoopCamera(RE::StaticFunctionTag*);

			// Stop combat on all active players,
			// optionally clearing all bounties to get off scot-free.
			void StopAllCombatOnCoopPlayers(RE::StaticFunctionTag*, bool a_clearBounties);

			// Signal the menu input manager to stop running, returning menu control to P1.
			void StopMenuInputManager(RE::StaticFunctionTag*);

			// Unfreeze time (via the Main singleton) 
			// if everyone and everything is still stuck in place.
			void UnfreezeTime(RE::StaticFunctionTag*);
		}

		// MCM functions.
		namespace Settings 
		{
			// Import all settings when this mod's MCM closes.
			void OnConfigClose(RE::TESQuest*);
		}

		// Papyrus API functions.
		namespace API
		{
			//
			// [V1]
			//

			// Get the actor for the player with the given device ID.
			// Controller IDs fall in the range [0, 3] and keyboard + mouse IDs are >= 4.
			// If the device given by the ID is controlling a player, 
			// return a pointer to that player actor.
			// Otherwise, return nullptr.
			RE::Actor* GetALYSLCPlayerByDID(RE::StaticFunctionTag*, int32_t a_deviceID);
			
			// Get the actor for the player with the given player ID.
			// Player 1 always has a player ID of 0, and all active companion players' IDs 
			// are assigned sequentially in the order of their device IDs (DIDs).
			// The player ID is used to index active players,
			// keep track of player-specific settings,
			// and retrieve information on a specific player.
			// It ignores gaps in assigned DIDs.
			// 0 -> Player 1
			// 1 -> Player 2
			// 2 -> Player 3
			// 3 -> Player 4
			// If the player ID is in the range [0, 3] and the corresponding player is active,
			// return that player's actor.
			// Otherwise, return nullptr.
			RE::Actor* GetALYSLCPlayerByPID(RE::StaticFunctionTag*, int32_t a_playerID);

			// Get the ID for the device controlling the given player actor.
			// Controller IDs fall in the range [0, 3] and keyboard + mouse IDs are >= 4.
			// If the given actor handle corresponds to an active (co-op session started) player, 
			// return the ID of the device controlling the actor.
			// Otherwise, return -1.
			int32_t GetALYSLCPlayerDID(RE::StaticFunctionTag*, RE::Actor* a_actor);

			// Get the ID for the player controlling the given player actor.
			// If the given actor corresponds to an active (co-op session started) player, 
			// return the ID [0, 3] of the player controlling the actor.
			// Otherwise, return -1.
			int32_t GetALYSLCPlayerPID(RE::StaticFunctionTag*, RE::Actor* a_actor);

			// Check if the given actor corresponds to a actor 
			// that is controllable by a co-op player.
			// A co-op session does not have to be active.
			// True if a co-op character (P1 or companion player), false otherwise.
			bool IsALYSLCCharacter(RE::StaticFunctionTag*, RE::Actor* a_actor);

			// Check if the given actor corresponds to an active co-op player.
			// True if an active co-op player actor (P1 or companion player), 
			// false otherwise.
			bool IsALYSLCPlayer(RE::StaticFunctionTag*, RE::Actor* a_actor);

			// Check if there is an active local co-op session.
			// True if companion players have been summoned.
			// False if no players have been summoned yet or all players were dismissed.
			bool IsSessionActive(RE::StaticFunctionTag*);
			
			// 
			// [V2]
			//

			// Check if the player controlling the given actor
			// is performing the action that corresponds to the given index.
			// See the 'ALYSLC::InputAction' enum in the 'Enums.h' file
			// for the supported action indices.
			// True if the player is performing the action,
			// false otherwise.
			bool IsPlayerPerformingAction
			(
				RE::StaticFunctionTag*, 
				RE::Actor* a_playerActor,
				uint32_t a_playerActionIndex
			);
			
			// Check if the player controlling the given character
			// is pressing the button/key or moving the analog stick/mouse 
			// that corresponds to the given input action index.
			// See the 'ALYSLC::InputAction' enum in the 'Enums.h' file
			// for the supported input indices.
			// True if the player is pressing the input.
			// False if the player is not pressing the input.
			bool IsPlayerPressingInput
			(
				RE::StaticFunctionTag*, 
				RE::Actor* a_playerActor,
				uint32_t a_inputIndex
			);


			//
			// [V3]
			//

			/// Add experience points to the given skill for the given player.
			/// Shared skills (Alchemy, Enchanting, Lockpicking, Pickpocket, Speech, Smithing)
			/// progress directly through player 1.
			/// The new skill XP amount for the skill is serialized and saved.
			void AddSkillXP
			(
				RE::StaticFunctionTag*, 
				RE::Actor* a_playerActor,
				int32_t a_skillAVIndex, 
				float a_baseXP
			);
		
			/// Gets the player ID for the player currently controlling any open menus.
			/// Returns the player ID of the player currently controlling menus,
			/// or -1 if no controllable menu is open.
			int32_t GetMenuControlPID(RE::StaticFunctionTag*);

			/// Gets the character for the player currently controlling any open menus.
			/// Returns the player character for the player currently controlling menus,
			/// or an None if no controllable menu is open.
			RE::Actor* GetMenuControlPlayer(RE::StaticFunctionTag*);

			/// IMPORTANT: Call before the desired menu opens and while a co-op session is active.
			/// While a co-op session is active, request menu control for the given menu
			/// be granted to the player corresponding to the given player ID.
			/// Specify '-1' as the player ID to relinquish control.
			/// Can optionally specify an object that triggered the menu 
			/// to better link the request to the player.
			/// (ex. a 'pull chain' as the object that triggers a message box)
			void RequestMenuControl
			(
				RE::StaticFunctionTag*,
				int32_t a_playerID,
				RE::BSFixedString a_menuName,
				RE::TESObjectREFR* a_assocRefr
			);
		}
	};
}



#pragma once
#include <CameraManager.h>
#include <Controller.h>
#include <Enums.h>
#include <Player.h>
#include <PlayerActionInfoHolder.h>
#include <PlayerActionFunctionsHolder.h>
#include <MenuInputManager.h>
#include <Settings.h>
   
namespace ALYSLC 
{
	using Skill = RE::PlayerCharacter::PlayerSkills::Data::Skill;
	using SkillList = 
	std::array<float, (size_t)RE::PlayerCharacter::PlayerSkills::Data::Skill::kTotal>;
	// Singleton holding data pertaining to and functions that affect all active co-op players.
	class GlobalCoopData
	{
	public:

		// Collision data from thrown objects.
		struct ContactEventData
		{
			ContactEventData() :
				bodyARefHandle(RE::ObjectRefHandle()),
				bodyBRefHandle(RE::ObjectRefHandle()),
				contactPoint(RE::NiPoint3())
			{ }

			ContactEventData
			(
				const RE::ObjectRefHandle& a_bodyARef, 
				const RE::ObjectRefHandle& a_bodyBRef, 
				const RE::NiPoint3& a_contactPoint, 
				const float& a_separatingVelocity
			) :
				bodyARefHandle(a_bodyARef),
				bodyBRefHandle(a_bodyBRef),
				contactPoint(a_contactPoint)
			{ }

			ContactEventData(const ContactEventData& a_other) :
				bodyARefHandle(a_other.bodyARefHandle),
				bodyBRefHandle(a_other.bodyBRefHandle),
				contactPoint(a_other.contactPoint)
			{ }

			ContactEventData(ContactEventData&& a_other) :
				bodyARefHandle(std::move(a_other.bodyARefHandle)),
				bodyBRefHandle(std::move(a_other.bodyBRefHandle)),
				contactPoint(std::move(a_other.contactPoint))
			{ }

			ContactEventData& operator=(const ContactEventData& a_other)
			{
				bodyARefHandle = a_other.bodyARefHandle;
				bodyBRefHandle = a_other.bodyBRefHandle;
				contactPoint = a_other.contactPoint;

				return *this;
			}

			ContactEventData& operator=(ContactEventData&& a_other)
			{
				bodyARefHandle = std::move(a_other.bodyARefHandle);
				bodyBRefHandle = std::move(a_other.bodyBRefHandle);
				contactPoint = std::move(a_other.contactPoint);

				return *this;
			}

			// Colliding bodies' handles.
			RE::ObjectRefHandle bodyARefHandle;
			RE::ObjectRefHandle bodyBRefHandle;
			// Collision position between the two bodies in world coordinates.
			RE::NiPoint3 contactPoint;
		};

		// Listener to handle thrown object collisions.
		struct ContactListener : public RE::hkpContactListener
		{
			ContactListener() :
				world(nullptr)
			{ }

			ContactListener(const int32_t& a_playerCID) :
				world(nullptr)
			{ }

			ContactListener& operator=(const ContactListener& a_other)
			{
				world = a_other.world;
				return *this;
			}

			ContactListener& operator=(ContactListener&& a_other)
			{
				world = std::move(a_other.world);
				return *this;
			}

			// Add listener to the given havok world to catch contact events.
			// Cache the world.
			inline void AddContactListener(RE::ahkpWorld* a_world)
			{
				if (!a_world)
				{
					return;
				}

				bool alreadyAdded = std::any_of
				(
					world->contactListeners.begin(), 
					world->contactListeners.end(), 
					[this](RE::hkpContactListener* a_listener) 
					{ 
						return a_listener == this; 
					}
				);
				if (alreadyAdded)
				{
					return;
				}

				world = a_world;
				Util::NativeFunctions::hkpCollisionCallbackUtil_requireCollisionCallbackUtil
				(
					world
				);
				Util::NativeFunctions::hkpWorld_addContactListener(world, this);
			}

			// Remove listener from the cached havok world.
			inline void RemoveContactListener()
			{
				if (!world)
				{
					return;
				}

				for (auto listener : world->contactListeners)
				{
					if (listener != this)
					{
						continue;
					}

					Util::NativeFunctions::hkpCollisionCallbackUtil_releaseCollisionCallbackUtil
					(
						world
					);
					Util::NativeFunctions::hkpWorld_removeContactListener(world, listener);
					listener = nullptr;
				}
			}

			// Contact listener for thrown refr collisions.
			void ContactPointCallback(const RE::hkpContactPointEvent& a_event) override;

			//
			// Members
			//

			// Havok world the listener was placed in.
			RE::ahkpWorld* world;
		};

		// Holds info about player data that should be copied 
		// to P1 before a menu opens or closes.
		struct CopyPlayerDataRequestInfo
		{
			CopyPlayerDataRequestInfo() :
				shouldImport(false),
				menuName(""sv),
				requestingPlayerHandle(),
				assocForm(nullptr)
			{ }

			CopyPlayerDataRequestInfo
			(
				bool a_shouldImport, 
				const RE::BSFixedString a_menuName,
				RE::ActorHandle a_requestingPlayerHandle,
				RE::TESForm* a_assocForm = nullptr
			) :
				shouldImport(a_shouldImport),
				menuName(a_menuName),
				requestingPlayerHandle(a_requestingPlayerHandle),
				assocForm(a_assocForm)
			{ }

			// Handle for player requesting to import data to P1.
			RE::ActorHandle requestingPlayerHandle;
			// Menu linked with the player data exchange.
			RE::BSFixedString menuName;
			// Associated form for the exchange.
			RE::TESForm* assocForm;
			// Should copy data to P1 (true) or restore P1's original data (false).
			bool shouldImport;
		};

		// Exchangeable player data with P1 when a co-op companion player is controlling menus. 
		struct ExchangeablePlayerData
		{
			ExchangeablePlayerData() :
				name(""), raceName("")
			{
				carryWeightAVData.fill(0.0f);
				hmsAVMods.fill({ 0.0f, 0.0f, 0.0f });
				hmsAVs.fill(0.0f);
				hmsBaseAVs.fill(0.0f);
				skillAVs.fill(0.0f);
				skillAVMods.fill(SkillList());
				unlockedPerks.clear();
			}

			ExchangeablePlayerData(
				const RE::BSFixedString& a_name,
				const RE::BSFixedString& a_raceName,
				std::array<float, 5> a_carryWeightAVData,
				std::array<std::array<float, 3>, 3> a_hmsAVMods,
				std::array<float, 3> a_hmsAVs,
				std::array<float, 3> a_hmsBaseAVs,
				SkillList a_skillAVs,
				std::array<SkillList, 3> a_skillAVMods,
				std::vector<RE::BGSPerk*> a_unlockedPerks,
				float a_carryWeight,
				int32_t a_gold
				) :
				name(a_name),
				raceName(a_raceName),
				carryWeightAVData(a_carryWeightAVData),
				hmsAVMods(a_hmsAVMods),
				hmsAVs(a_hmsAVs),
				hmsBaseAVs(a_hmsBaseAVs),
				skillAVs(a_skillAVs),
				skillAVMods(a_skillAVMods),
				unlockedPerks(a_unlockedPerks)
			{ }

			//
			// Members
			//

			// Player name.
			RE::BSFixedString name;
			// Player race name.
			RE::BSFixedString raceName;

			// Current AV, base AV, damage AV mod, permanent AV mod, and temp AV mod.
			std::array<float, 5> carryWeightAVData;
			// Damage AV mod, permanent AV mod, temp AV mod for Health/Magicka/Stamina.
			std::array<std::array<float, 3>, 3> hmsAVMods;
			// Health/Magicka/Stamina AVs.
			std::array<float, 3> hmsAVs;
			// Base Health/Magicka/Stamina AVs.
			std::array<float, 3> hmsBaseAVs;
			// Skill AVs, shared and otherwise.
			SkillList skillAVs;
			// Damage AV mod, permanent AV mod, temp AV mod for all skill AVs.
			std::array<SkillList, 3> skillAVMods;
			// List of unlocked perks for this player.
			std::vector<RE::BGSPerk*> unlockedPerks;
		};

		// Holds the arguments from the P1's last 'UseSkill()' call for a melee related skill:
		// OneHanded, Twohanded, Archery, Block, HeavyArmor, LightArmor.
		// We keep track of these calls to delay execution of the UseSkill() function until
		// AFTER the melee hit event fires, since we have to check if the attacked/attacker actor
		// is friendly to P1, and if so, skip awarding XP to P1 when friendly fire is off.
		// For other skills, such as Destruction, UseSkill() seems to execute after damage 
		// is calculated/associated hit events fire, so caching this data is unnecessary.
		// NOTE: For now, will only work if each melee UseSkill() call is followed by ProcessHit(). 
		// Otherwise, only the last UseSkill() call before ProcessHit() will get saved.
		// Could eventually use a queue, 
		// which would involve more costly comparisons (3 members per queued item).
		struct LastP1MeleeUseSkillCallArgs
		{
			LastP1MeleeUseSkillCallArgs() :
				skill(RE::ActorValue::kNone),
				assocForm(nullptr),
				points(0.0f)
			{ }

			LastP1MeleeUseSkillCallArgs
			(
				RE::ActorValue a_skill, float a_points, RE::TESForm* a_assocForm
			) :
				skill(a_skill),
				assocForm(a_assocForm),
				points(a_points)
			{ }

			// Skill to increment from cached call.
			RE::ActorValue skill;
			// Associated form from cached call (eg. a weapon dealing damage).
			RE::TESForm* assocForm;
			// Points used to increment skill AV from cached call.
			float points;
		};

		// Player data to write to the SKSE co-save on creation of a save file 
		// and read back on loading of a save file.
		class SerializablePlayerData
		{
		public:
			SerializablePlayerData() :
				availablePerkPoints(0),
				firstSavedLevel(0),
				level(1),
				levelXP(0.0f),
				sharedPerksUnlocked(0),
				usedPerkPoints(0),
				extraPerkPoints(0),
				prevTotalUnlockedPerks(0)
			{
				equippedForms.clear();
				equippedForms = std::vector<RE::TESForm*>(!EquipIndex::kTotal, nullptr);
				favoritedMagForms.clear();
				unlockedPerksList.clear();
				unlockedPerksSet.clear();
				copiedMagic.fill(nullptr);
				cyclableEmoteIdleEvents = GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS;
				hmsBaseAVsOnMenuEntry.fill(0.0f);
				hmsBasePointsList.fill(100.0f);
				hmsPointIncreasesList.fill(0.0f);
				hotkeyedForms.fill(nullptr);
				skillBaseLevelsList.fill(0.0f);
				skillLevelIncreasesList.fill(0.0f);
				skillLevelsOnMenuEntry.fill(15.0f);
				skillLevelThresholdsOnMenuEntry.fill(0.0f);
				skillXPList.fill(0.0f);
			}

			SerializablePlayerData
			(
				std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal> a_copiedMagic,
				std::array<RE::BSFixedString, 8> a_cyclableEmoteIdleEvents,
				std::vector<RE::TESForm*> a_equippedForms,
				std::vector<RE::TESForm*> a_favoritedMagForms,
				float a_levelXP,
				uint32_t a_availablePerkPoints, 
				uint32_t a_firstSavedLevel,
				uint32_t a_level, 
				uint32_t a_sharedPerksUnlocked, 
				uint32_t a_usedPerkPoints, 
				uint32_t a_extraPerkPoints,
				std::array<float, 3> a_hmsBasePointsList,
				std::array<float, 3> a_hmsPointIncreasesList,
				std::array<RE::TESForm*, 8> a_hotkeyedForms,
				SkillList a_skillBaseLevelsList,
				SkillList a_skillLevelIncreasesList,
				SkillList a_skillXPList,
				std::vector<RE::BGSPerk*> a_unlockedPerksList
			) :
				copiedMagic(a_copiedMagic),
				cyclableEmoteIdleEvents(a_cyclableEmoteIdleEvents),
				equippedForms(a_equippedForms),
				favoritedMagForms(a_favoritedMagForms),
				levelXP(a_levelXP),
				availablePerkPoints(a_availablePerkPoints),
				firstSavedLevel(a_firstSavedLevel),
				level(a_level),
				sharedPerksUnlocked(a_sharedPerksUnlocked),
				usedPerkPoints(a_usedPerkPoints),
				extraPerkPoints(a_extraPerkPoints),
				hmsBasePointsList(a_hmsBasePointsList),
				hmsPointIncreasesList(a_hmsPointIncreasesList),
				hotkeyedForms(a_hotkeyedForms),
				skillBaseLevelsList(a_skillBaseLevelsList),
				skillLevelIncreasesList(a_skillLevelIncreasesList),
				skillXPList(a_skillXPList),
				unlockedPerksList(a_unlockedPerksList)
			{
				hmsBaseAVsOnMenuEntry.fill(0.0f);
				skillLevelsOnMenuEntry.fill(15.0f);
				skillLevelThresholdsOnMenuEntry.fill(0.0f);
				unlockedPerksSet = std::set<RE::BGSPerk*>
				(
					unlockedPerksList.begin(), unlockedPerksList.end()
				);
				prevTotalUnlockedPerks = 0;
			}

			// Clear cached unlocked perks list, set, and count.
			inline void ClearUnlockedPerks()
			{
				unlockedPerksList.clear();
				unlockedPerksSet.clear();
				prevTotalUnlockedPerks = 0;
			}

			// Get list of all unlocked perks.
			inline const std::vector<RE::BGSPerk*>& GetUnlockedPerksList() 
			{ 
				return unlockedPerksList; 
			}

			// Get set of all unlocked perks.
			inline const std::set<RE::BGSPerk*>& GetUnlockedPerksSet() { return unlockedPerksSet; }

			// Check if the player has unlocked the given perk.
			inline bool HasUnlockedPerk(RE::BGSPerk* a_perk) 
			{
				return !unlockedPerksSet.empty() && unlockedPerksSet.contains(a_perk);
			}

			// Prevent insertion/serialization of duplicate perks.
			// Return true if successfully inserted, false if duplicate.
			bool InsertUnlockedPerk(RE::BGSPerk* a_perk) noexcept
			{
				if (unlockedPerksSet.empty() || !unlockedPerksSet.contains(a_perk))
				{
					unlockedPerksList.emplace_back(a_perk);
					unlockedPerksSet.emplace(a_perk);

					return true;
				}

				return false;
			}

			// Set unlocked perks list and set directly.
			inline void SetUnlockedPerks(const std::vector<RE::BGSPerk*> a_perksList) noexcept
			{
				unlockedPerksList = a_perksList;
				unlockedPerksSet.clear();
				unlockedPerksSet = std::set<RE::BGSPerk*>(a_perksList.begin(), a_perksList.end());
			}

			// [Serialized]:
			// Spells and shout forms copied into placeholder forms.
			std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal> copiedMagic;
			// Cyclable emote idle event names.
			std::array<RE::BSFixedString, 8> cyclableEmoteIdleEvents;
			// Base health, magicka, and stamina actor values.
			std::array<float, 3> hmsBasePointsList;
			// Change in base health, magicka, and stamina actor values.
			std::array<float, 3> hmsPointIncreasesList;
			// Up to 8 hotkeyed favorited forms.
			// Nullptr if the slot has no hotkeyed form.
			std::array<RE::TESForm*, 8> hotkeyedForms;
			// All forms that were saved as equipped for this player.
			std::vector<RE::TESForm*> equippedForms;
			// Favorited magical (spells/shouts) forms for this player.
			// NOTE: Having the game store physical and magical favorites separately is a PITA.
			// Since the physical favorited forms are already saved through serialized extra data 
			// and can be saved on NPCs, we only serialize the magical favorited forms, 
			// which are normally saved as a single, P1-only list in MagicFavorites.
			std::vector<RE::TESForm*> favoritedMagForms;
			// Base skill levels for this player.
			SkillList skillBaseLevelsList;
			// Increases on the base skill levels for this player.
			SkillList skillLevelIncreasesList;
			// Skill XP totals.
			SkillList skillXPList;
			// XP towards the current player level.
			float levelXP;
			// Available perk points.
			uint32_t availablePerkPoints;
			// Points received from non-levelup sources, ex. console commands.
			uint32_t extraPerkPoints;
			// First player level serialized.
			uint32_t firstSavedLevel;
			// Player level.
			uint32_t level;
			// Shared perks unlocked by this player.
			uint32_t sharedPerksUnlocked;
			// Used levelup perk points.
			uint32_t usedPerkPoints;

			// [Helper data: NOT Serialized]:
			// Previous total unlocked perks count.
			uint32_t prevTotalUnlockedPerks;
			// Previous HMS base actor values when opening the Stats Menu.
			// NOTE: The difference in HMS between opening and closing the menu
			// is used to calculate how many times the player leveled up.
			std::array<float, 3> hmsBaseAVsOnMenuEntry;
			// P1 skill levels and skill level thresholds saved when a companion player 
			// enters the Stats Menu and restored when the player exits the menu.
			SkillList skillLevelsOnMenuEntry;
			SkillList skillLevelThresholdsOnMenuEntry;

		private:
			// List of perks that the player has unlocked.
			std::vector<RE::BGSPerk*> unlockedPerksList;
			// Set of perks that the player has unlocked.
			std::set<RE::BGSPerk*> unlockedPerksSet;
		};

		// Get the global data singleton.
		static GlobalCoopData& GetSingleton();

		// Clean up global and player task runner threads by signalling them to stop.
		~GlobalCoopData() 
		{
			// Request that detached threads stop.
			taskRunner->runnerThread.request_stop();
			for (const auto& p : coopPlayers) 
			{
				if (p->isActive) 
				{
					p->taskRunner->runnerThread.request_stop();
				}
			}
		}

		// Increment the given skill for the co-op actor, 
		// provided the base XP amount for that skill.
		static void AddSkillXP
		(
			const int32_t& a_cid, RE::ActorValue a_skillAV, const float& a_baseXP
		);

		// Adjust perk point counts for all active players 
		// based on their unlocked perks and HMS increases.
		static void AdjustAllPlayerPerkCounts();

		// Save the player's base HMS actor values on menu entry 
		// to check for any changes when exiting the menu.
		// Then adjust the player's base HMS actor value increases 
		// when exiting the menu to keep track of how many
		// times the player leveled up.
		static void AdjustBaseHMSData(RE::Actor* a_playerActor, const bool& a_shouldImport);

		// Adjust the amount of perk points for P1 
		// when the given co-op player enters the Stats Menu.
		// Returns true if P1's level was decreased (dipped) to allow for HMS levelup(s).
		static bool AdjustInitialPlayer1PerkPoints(RE::Actor* a_playerActor);

		// Adjust the number of available perk points for the given actor when they enter/exit
		// the Stats Menu. For co-op companion players.
		static void AdjustPerkDataForCompanionPlayer
		(
			RE::Actor* a_playerActor, const bool& a_enteringMenu
		);

		// Same idea as above but specifically for P1.
		static void AdjustPerkDataForPlayer1(const bool& a_enteringMenu);
		
		// Assign generic killmoves by getting killmove idles 
		// that aren't explicitly linked to a skeleton type
		// and grouping them based on weapon type.
		static void AssignGenericKillmoves();

		// Assign killmoves linked with target skeleton type by categorizing killmove idles 
		// based on skeleton name and then weapon type.
		static void AssignSkeletonSpecificKillmoves();
		
		// Checks if this controller can control menus.
		static bool CanControlMenus(const int32_t& a_controllerID);

		// Enable collisions among the biped, biped no char controller,
		// dead biped, and char controller layers for all actors in the current cell.
		// Allows the havok contact listener to respond to collisions between ragdolling bodies.
		// NOTE:
		// Currently haven't figured out how to enable ragdoll-to-P1 collisions,
		// so all ragdolling actors pass through P1 without colliding.
		static void EnableRagdollToActorCollisions();

		// If the given argument is a co-op player, get the player's index 
		// in the co-op companions list (controller ID).
		// Return -1 otherwise.
		static int8_t GetCoopPlayerIndex(const RE::ActorPtr& a_actorPtr);
		static int8_t GetCoopPlayerIndex(RE::TESObjectREFR* a_refr);
		static int8_t GetCoopPlayerIndex(const RE::TESObjectREFRPtr& a_refrPtr);
		static int8_t GetCoopPlayerIndex(const RE::ObjectRefHandle& a_refrHandle);
		static int8_t GetCoopPlayerIndex(const RE::FormID& a_formID);

		// Get highest AV amount for the shared AV among all active players.
		// Return -1 if there is no active co-op session.
		static float GetHighestSharedAVLevel(const RE::ActorValue& a_av);
		
		// Returns the total number of unlocked perks in all shared skill trees.
		static uint32_t GetUnlockedSharedPerksCount();
		
		// Transfer party-wide items (usable by any player through P1, 
		// or trigger quests) from all players to P1.
		// Items include: gold, lockpicks, keys, non-skill/spell teaching books, and notes.
		static void GivePartyWideItemsToP1();

		// Check for arm node collisions with raycasts along each player's moving arms.
		static void HandlePlayerArmCollisions();

		// NOTE: Enderal only.
		// Check for changes to P1 level-ups, and changes to crafting/learning/memory points.
		// Have to poll for changes periodically in the absence of events 
		// that signal changes to these quantities.
		static void HandleEnderalProgressionChanges();

		// Havok pre-physics callback queued and run by Precision.
		// Cache player arm and torso node rotations to restore later 
		// in an NiNode hook, which will overwrite the game's changes 
		// to all handled player arm/torso nodes.
		static void HavokPrePhysicsStep(RE::bhkWorld* a_world);

		// Remove all perks for this player and then add back all serialized unlocked perks.
		static void ImportUnlockedPerks(RE::Actor* a_coopActor);

		// Set global data after loading a save.
		// Ref alias passed in through script.
		static void InitializeGlobalCoopData(RE::BGSRefAlias* a_player1RefAlias);

		// Checks if this controller is controlling menus.
		static bool IsControllingMenus(const int32_t& a_controllerID);

		// Is the given argument a co-op player?
		static bool IsCoopPlayer(const RE::ActorPtr& a_actorPtr);
		static bool IsCoopPlayer(RE::TESObjectREFR* a_refr);
		static bool IsCoopPlayer(const RE::TESObjectREFRPtr& a_refrPtr);
		static bool IsCoopPlayer(const RE::ObjectRefHandle& a_refrHandle);
		static bool IsCoopPlayer(const RE::FormID& a_formID);
		
		// Checks if this controller is not controlling menus.
		static bool IsNotControllingMenus(const int32_t& a_controllerID);

		// Is a co-op player-controllable menu open?
		static bool IsSupportedMenuOpen();

		// Modify the XP threshold for leveling up based on the imported setting.
		// Restore original when not setting for co-op.
		static void ModifyLevelUpXPThreshold(const bool& a_setForCoop);

		// Modify the character XP granted per skill level multiplier 
		// to account for the additional companion players.
		// Restore default when not setting for co-op.
		static void ModifyXPPerSkillLevelMult(const bool& a_setForCoop);

		// Check if any player has not recorded a co-op level up 
		// (never summoned or first saved level is 0) 
		// and thus needs their AVs auto-scaled by the game.
		// Done to set an AV baseline for players introduced for the first time mid-playthrough.
		static void PerformInitialAVAutoScaling();

		// Reset the player's health/magicka/stamina AVs to their initial values,
		// remove all unlocked perks, and also remove all unlocked shared perks 
		// for all active players.
		static void PerformPlayerRespec(RE::Actor* a_playerActor);

		// Register for global script events to send data to the player ref alias script.
		static void RegisterEvents();

		// Rescale all active player skill and HMS AVs.
		// Reset to saved base AVs and then add increases.
		// Done to override the game's auto-scaling of NPC AVs.
		static void RescaleActivePlayerAVs();

		// Rescale on summoning, player level up, and on class change, or
		// in any other scenario where the game's NPC AV auto-scaling kicks in.
		static void RescaleAVsOnBaseSkillAVChange(RE::Actor* a_playerActor);

		// Set last menu controller ID to the current one and then
		// reset the current menu controller ID.
		static void ResetMenuCIDs();

		// Save current unlocked perks for all active players to their serializable data sets.
		static void SaveUnlockedPerksForAllPlayers();

		// Save current unlocked perks for the given player to their serializable data set.
		static void SaveUnlockedPerksForPlayer(RE::Actor* a_coopActor);

		// Concatenate all individual player crosshair text entries,
		// and then set the crosshair's info text to the result.
		static void SetCrosshairText();

		// Set the default game settings values to revert to for player XP calculations.
		static void SetDefaultXPBaseAndMultFromGameSettings();

		// Set menu controller IDs (current and previous).
		// -1 to reset.
		static void SetMenuCIDs(const int32_t a_controllerID);

		// DEBUG OPTION: Stop combat between all actors and players.
		// Only among party means that only combat between player teammates and players is stopped.
		// Remove crime gold means the players "get off scot-free" without paying a fine.
		static void StopAllCombatOnCoopPlayers(bool&& a_onlyAmongParty, bool&& a_removeCrimeGold);

		// DEBUG OPTION: Force stop the menu input manager, 
		// resetting menu CID/overlay data as well.
		static void StopMenuInputManager();

		// Sync co-op companions' shared perks to P1's so that all players have the same 
		// set of perks from the shared skill trees.
		static void SyncSharedPerks();

		// Sync co-op companions' shared AVs to P1's so that all players have the same
		// skill AVs from the group of shared skills.
		static void SyncSharedSkillAVs();

		// End co-op session, optionally dismissing all co-op companions.
		static void TeardownCoopSession(bool a_shouldDismiss);

		// DEBUG OPTION: Toggle god mode for all players.
		static void ToggleGodModeForAllPlayers(const bool& a_enable);

		// DEBUG OPTION: Toggle god mode for the given player.
		static void ToggleGodModeForPlayer(const int32_t& a_controllerID, const bool& a_enable);

		// Unregister global script event registrations (with player ref alias).
		static void UnregisterEvents();

		// Update serlializable data form ID keys for all players.
		// NOTE: Serialized data should contain all players' FID keys 
		// before the Summoning Menu is opened.
		static void UpdateAllSerializedCompanionPlayerFIDKeys();

		// Update player actor's serialized form ID.
		// Form IDs for co-op companion players may change 
		// if this mod's spot in the load order is modified.
		static bool UpdateSerializedCompanionPlayerFIDKey(RE::Actor* a_playerActor);


		// Try again? The entire party gets wiped.
		static void YouDied(RE::Actor* a_deadPlayer = nullptr);

		// Co-op player data copying on menu open/close events.
		static void CopyPlayerData(const std::unique_ptr<CopyPlayerDataRequestInfo>& a_info);

		// Base data such as player name, race name, gold, and carryweight.
		static void CopyOverActorBaseData
		(
			RE::Actor* a_coopActor,
			const bool& a_shouldImport,
			bool&& a_name, 
			bool&& a_raceName, 
			bool&& a_carryWeight
		);

		// Exchanges/restores HMS and skill AVs. 
		// Can also keep changes made while the linked menu was open and
		// export these AV changes to the requesting player when the menu closes.
		static void CopyOverAVs
		(
			RE::Actor* a_coopActor,
			const bool& a_shouldImport, 
			const bool& a_shouldCopyChanges = false
		);

		// WIP: Needs more testing for long term side effects, and may need a rework if a better 
		// solution is found that doesn't involve brute-force copying all inventory items to P1.
		// Exchange the given player's inventory with P1's or restore P1's.
		// Allows companion players to sell their own items, but obviously has limitations
		// and can cause major issues if the game saves in this state.
		// P1 keeps their equipped items, which cannot be sold by another player when bartering.
		static void CopyOverInventories(RE::Actor* a_coopActor, const bool& a_shouldImport);

		// Exchanges/restores unlocked perks. Only copies the player's perks over to P1,
		// and these perks are not added or removed 
		// to match their ordering in their respective skill trees,
		// so this should not be called when opening/closing the Stats Menu.
		// No skill XP totals are modified.
		static void CopyOverPerkLists(RE::Actor* a_coopActor, const bool& a_shouldImport);

		// Exchanges/restores perk trees by removing all P1's perks and adding the given player's
		// unlocked perks in order, one node at a time for each skill tree. 
		// Also imports skill XP totals.
		// Changes are copied back over to the co-op player upon exiting the Stats Menu and 
		// all of P1's perks trees and skill XP totals are restored.
		static void CopyOverPerkTrees(RE::Actor* a_coopActor, const bool& a_shouldImport);

		//
		// Tasks to run on task runner thread.
		//
		
		// Run as a task or in the main hook.
		// Copy co-op companion player's data over to P1 before the given menu opens, 
		// or restore P1's data before closing the given menu.
		// Associated form is a linked form which provides additional info.
		// For example, if the Training Menu is open, the associated form is set to the trainer NPC
		// to link them with the menu.
		static void CopyOverCoopPlayerData
		(
			const bool a_shouldImport, 
			const RE::BSFixedString a_menuName,
			RE::ActorHandle a_requestingPlayerHandle,
			RE::TESForm* a_assocForm = nullptr
		);

		// Assign linked controller ID for P1 via a prompt to press a certain button.
		// Workaround until finding direct way of accessing P1's controller's XInput index. 
		static void PromptForPlayer1CIDTask();

		// Stop P1's managers, disable the co-op camera, and close the menu input manager.
		static void ResetPlayer1AndCameraTask();

		// Prompt the player given by the CID to press the 'Start' button on their controller
		// to confirm their intentions to respec their character.
		// Then, reset their HMS AVs and perk data 
		// and remove all shared perks from all active players.
		static void RespecPlayerTask(const int32_t a_controllerID);

		// Pause and then restart the co-op camera manager.
		static void RestartCoopCameraTask();

		//
		// Const Members
		//

		// Max number of composing input actions assignable to a bind.
		static constexpr uint8_t MAX_ACTIONS_PER_BIND = 4;

		// ALYSLC plugin name to load data from (Skyrim SE or Enderal SE).
		// Defaults to skyrim plugin name.
		static inline std::string_view PLUGIN_NAME = "ALYSLC.esp"sv;

		// Set of selectable perks through the Stats Menu.
		// Populated with all perk tree node perks on global data initialization.
		static inline std::set<RE::BGSPerk*> SELECTABLE_PERKS;

		// Custom Menu name.
		static constexpr inline std::string_view CUSTOM_MENU = "CustomMenu"sv;

		// Enhanced Hero Menu name.
		static constexpr inline std::string_view ENHANCED_HERO_MENU = "00E_heromenu"sv;

		// Loot Menu name.
		static constexpr inline std::string_view LOOT_MENU = "LootMenu"sv;

		// Summoning menu name.
		static constexpr inline std::string_view SETUP_MENU_NAME = "ALYSLC Setup Menu"sv;

		// Maps actor values to their corresponding player skills.
		static inline const std::unordered_map<RE::ActorValue, Skill> AV_TO_SKILL_MAP = 
		{
			{ RE::ActorValue::kOneHanded, Skill::kOneHanded },
			{ RE::ActorValue::kTwoHanded, Skill::kTwoHanded },
			{ RE::ActorValue::kArchery, Skill::kArchery },
			{ RE::ActorValue::kBlock, Skill::kBlock },
			{ RE::ActorValue::kSmithing, Skill::kSmithing },
			{ RE::ActorValue::kHeavyArmor, Skill::kHeavyArmor },
			{ RE::ActorValue::kLightArmor, Skill::kLightArmor },
			{ RE::ActorValue::kPickpocket, Skill::kPickpocket },
			{ RE::ActorValue::kLockpicking, Skill::kLockpicking },
			{ RE::ActorValue::kSneak, Skill::kSneak },
			{ RE::ActorValue::kAlchemy, Skill::kAlchemy },
			{ RE::ActorValue::kSpeech, Skill::kSpeech },
			{ RE::ActorValue::kAlteration, Skill::kAlteration },
			{ RE::ActorValue::kConjuration, Skill::kConjuration },
			{ RE::ActorValue::kDestruction, Skill::kDestruction },
			{ RE::ActorValue::kIllusion, Skill::kIllusion },
			{ RE::ActorValue::kRestoration, Skill::kRestoration },
			{ RE::ActorValue::kEnchanting, Skill::kEnchanting }
		};

		// Maps player skills to their corresponding actor values.
		static inline const std::unordered_map<Skill, RE::ActorValue> SKILL_TO_AV_MAP =
		{
			{ Skill::kOneHanded, RE::ActorValue::kOneHanded },
			{ Skill::kTwoHanded, RE::ActorValue::kTwoHanded },
			{ Skill::kArchery, RE::ActorValue::kArchery },
			{ Skill::kBlock, RE::ActorValue::kBlock },
			{ Skill::kSmithing, RE::ActorValue::kSmithing },
			{ Skill::kHeavyArmor, RE::ActorValue::kHeavyArmor },
			{ Skill::kLightArmor, RE::ActorValue::kLightArmor },
			{ Skill::kPickpocket, RE::ActorValue::kPickpocket },
			{ Skill::kLockpicking, RE::ActorValue::kLockpicking },
			{ Skill::kSneak, RE::ActorValue::kSneak },
			{ Skill::kAlchemy, RE::ActorValue::kAlchemy },
			{ Skill::kSpeech, RE::ActorValue::kSpeech },
			{ Skill::kAlteration, RE::ActorValue::kAlteration },
			{ Skill::kConjuration, RE::ActorValue::kConjuration },
			{ Skill::kDestruction, RE::ActorValue::kDestruction },
			{ Skill::kIllusion, RE::ActorValue::kIllusion },
			{ Skill::kRestoration, RE::ActorValue::kRestoration },
			{ Skill::kEnchanting, RE::ActorValue::kEnchanting }
		};

		// Maps actor values to their corresponding text name.
		static inline const std::unordered_map<RE::ActorValue, std::string> AV_TO_SKILL_NAME_MAP =
		{
			{ RE::ActorValue::kOneHanded, "Onehanded" },
			{ RE::ActorValue::kTwoHanded, "Twohanded" },
			{ RE::ActorValue::kArchery, "Marksman" },
			{ RE::ActorValue::kBlock, "Block" },
			{ RE::ActorValue::kSmithing, "Smithing" },
			{ RE::ActorValue::kHeavyArmor, "HeavyArmor" },
			{ RE::ActorValue::kLightArmor, "LightArmor" },
			{ RE::ActorValue::kPickpocket, "Pickpocket" },
			{ RE::ActorValue::kLockpicking, "Lockpicking" },
			{ RE::ActorValue::kSneak, "Sneak" },
			{ RE::ActorValue::kAlchemy, "Alchemy" },
			{ RE::ActorValue::kSpeech, "Speechcraft" },
			{ RE::ActorValue::kAlteration, "Alteration" },
			{ RE::ActorValue::kConjuration, "Conjuration" },
			{ RE::ActorValue::kDestruction, "Destruction" },
			{ RE::ActorValue::kIllusion, "Illusion" },
			{ RE::ActorValue::kRestoration, "Restoration" },
			{ RE::ActorValue::kEnchanting, "Enchanting" }
		};

		// Skill names to their corresponding actor values.
		static inline const std::unordered_map<std::string, RE::ActorValue> SKILL_NAME_TO_AV_MAP = 
		{
			{ "Onehanded", RE::ActorValue::kOneHanded },
			{ "Twohanded", RE::ActorValue::kTwoHanded },
			{ "Marksman", RE::ActorValue::kArchery },
			{ "Block", RE::ActorValue::kBlock },
			{ "Smithing", RE::ActorValue::kSmithing },
			{ "HeavyArmor", RE::ActorValue::kHeavyArmor },
			{ "LightArmor", RE::ActorValue::kLightArmor },
			{ "Pickpocket", RE::ActorValue::kPickpocket },
			{ "Lockpicking", RE::ActorValue::kLockpicking },
			{ "Sneak", RE::ActorValue::kSneak },
			{ "Alchemy", RE::ActorValue::kAlchemy },
			{ "Speechcraft", RE::ActorValue::kSpeech },
			{ "Alteration" , RE::ActorValue::kAlteration },
			{ "Conjuration", RE::ActorValue::kConjuration },
			{ "Destruction", RE::ActorValue::kDestruction },
			{ "Illusion", RE::ActorValue::kIllusion },
			{ "Restoration", RE::ActorValue::kRestoration },
			{ "Enchanting", RE::ActorValue::kEnchanting }
		};

		// Enderal skill names mapped to their corresponding Skyrim actor values.
		static inline const std::unordered_map<std::string, RE::ActorValue> 
		ENDERAL_SKILL_NAMES_TO_SKYRIM_AVS_MAP = 
		{
			{ "One-handed", RE::ActorValue::kOneHanded },
			{ "Two-handed", RE::ActorValue::kTwoHanded },
			{ "Marksman", RE::ActorValue::kArchery },
			{ "Block", RE::ActorValue::kBlock },
			{ "Handicraft", RE::ActorValue::kSmithing },
			{ "Heavy Armor", RE::ActorValue::kHeavyArmor },
			{ "Light Armor", RE::ActorValue::kLightArmor },
			{ "Sleight of Hand", RE::ActorValue::kPickpocket },
			{ "Lockpicking", RE::ActorValue::kLockpicking },
			{ "Sneak", RE::ActorValue::kSneak },
			{ "Alchemy", RE::ActorValue::kAlchemy },
			{ "Rhetoric", RE::ActorValue::kSpeech },
			{ "Mentalism", RE::ActorValue::kAlteration },
			{ "Entropy", RE::ActorValue::kConjuration },
			{ "Elementalism", RE::ActorValue::kDestruction },
			{ "Psionics", RE::ActorValue::kIllusion },
			{ "Light magic", RE::ActorValue::kRestoration },
			{ "Enchantment", RE::ActorValue::kEnchanting }
		};

		// Skyrim actor values mapped to their corresponding Enderal skill names.
		static inline const std::unordered_map<RE::ActorValue, std::string> 
		SKYRIM_AVS_TO_ENDERAL_SKILL_NAMES_MAP = 
		{
			{ RE::ActorValue::kOneHanded, "One-handed" },
			{ RE::ActorValue::kTwoHanded, "Two-handed" },
			{ RE::ActorValue::kArchery, "Marksman" },
			{ RE::ActorValue::kBlock, "Block" },
			{ RE::ActorValue::kSmithing, "Handicraft" },
			{ RE::ActorValue::kHeavyArmor, "Heavy Armor" },
			{ RE::ActorValue::kLightArmor, "Light Armor" },
			{ RE::ActorValue::kPickpocket, "Sleight of Hand" },
			{ RE::ActorValue::kLockpicking, "Lockpicking" },
			{ RE::ActorValue::kSneak, "Sneak" },
			{ RE::ActorValue::kAlchemy, "Alchemy" },
			{ RE::ActorValue::kSpeech, "Rhetoric" },
			{ RE::ActorValue::kAlteration, "Mentalism" },
			{ RE::ActorValue::kConjuration, "Entropy" },
			{ RE::ActorValue::kDestruction, "Elementalism" },
			{ RE::ActorValue::kIllusion, "Psionics" },
			{ RE::ActorValue::kRestoration, "Light magic" },
			{ RE::ActorValue::kEnchanting, "Enchantment" }
		};

		// Set of menus requiring import/export of co-op player data when entering/exiting.
		static inline const std::set<std::string_view> COPY_PLAYER_DATA_MENU_NAMES = 
		{
			RE::BarterMenu::MENU_NAME, 
			RE::BookMenu::MENU_NAME,
			RE::ContainerMenu::MENU_NAME,
			RE::CraftingMenu::MENU_NAME,
			RE::StatsMenu::MENU_NAME,
			RE::TrainingMenu::MENU_NAME,
			ENHANCED_HERO_MENU
		};

		// Default cyclable emote idle animation event names.
		static inline const std::array<RE::BSFixedString, 8> DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS = 
		{
			"IdleApplaud2",
			"IdleSilentBow",
			"IdleCiceroDance1",
			"IdleGetAttention",
			"pa_HugA",
			"IdleLaugh",
			"IdlePointClose",
			"IdleWave"
		};

		// Skills that are shared among the party.
		// All active players' levels in these skills are synced to create shared progression.
		static inline const std::set<RE::ActorValue> SHARED_SKILL_AVS_SET =
		{
			RE::ActorValue::kSmithing,
			RE::ActorValue::kPickpocket,
			RE::ActorValue::kLockpicking,
			RE::ActorValue::kAlchemy,
			RE::ActorValue::kSpeech,
			RE::ActorValue::kEnchanting
		};

		// Names of all skill actor values that have shared progression.
		static inline const std::set<std::string> SHARED_SKILL_NAMES_SET =
		{
			{ "Smithing" },
			{ "Pickpocket" },
			{ "Lockpicking" },
			{ "Alchemy" },
			{ "Speechcraft" },
			{ "Enchanting" }
		};

		// List of progressable skill actor values.
		static inline const std::vector<RE::ActorValue> SKILL_ACTOR_VALUES_LIST = 
		{
			RE::ActorValue::kOneHanded,
			RE::ActorValue::kTwoHanded,
			RE::ActorValue::kArchery,
			RE::ActorValue::kBlock,
			RE::ActorValue::kSmithing,
			RE::ActorValue::kHeavyArmor,
			RE::ActorValue::kLightArmor,
			RE::ActorValue::kPickpocket,
			RE::ActorValue::kLockpicking,
			RE::ActorValue::kSneak,
			RE::ActorValue::kAlchemy,
			RE::ActorValue::kSpeech,
			RE::ActorValue::kAlteration,
			RE::ActorValue::kConjuration,
			RE::ActorValue::kDestruction,
			RE::ActorValue::kIllusion,
			RE::ActorValue::kRestoration,
			RE::ActorValue::kEnchanting
		};

		// Killmove type categories assigned to each possible weapon type.
		// Killmove type is ordered from most generic to specific, left to right.
		static inline const std::unordered_map<RE::WEAPON_TYPE, std::vector<KillmoveType>> 
		KILLMOVE_TYPES_FOR_WEAP_TYPE = 
		{
			{ 
				RE::WEAPON_TYPE::kBow, 
				{ KillmoveType::kShield } 
			},
			{ 
				RE::WEAPON_TYPE::kCrossbow, 
				{ KillmoveType::kShield } 
			},
			{ 
				RE::WEAPON_TYPE::kHandToHandMelee, 
				{ KillmoveType::kGeneral, KillmoveType::kH2H } 
			},
			{ 
				RE::WEAPON_TYPE::kOneHandAxe, 
				{ KillmoveType::kGeneral, KillmoveType::k1H, KillmoveType::k1HAxe } 
			},
			{ 
				RE::WEAPON_TYPE::kOneHandDagger, 
				{ KillmoveType::kGeneral, KillmoveType::k1H, KillmoveType::k1HDagger } 
			},
			{ 
				RE::WEAPON_TYPE::kOneHandMace, 
				{ KillmoveType::kGeneral, KillmoveType::k1H, KillmoveType::k1HMace } 
			},
			{ 
				RE::WEAPON_TYPE::kOneHandSword, 
				{ KillmoveType::kGeneral, KillmoveType::k1H, KillmoveType::k1HSword } 
			},
			{ 
				RE::WEAPON_TYPE::kTwoHandAxe, 
				{ KillmoveType::kGeneral, KillmoveType::k2H, KillmoveType::k2HAxe } 
			},
			{ 
				RE::WEAPON_TYPE::kTwoHandSword, 
				{ KillmoveType::kGeneral, KillmoveType::k2H, KillmoveType::k2HSword } 
			},
			{ 
				RE::WEAPON_TYPE::kStaff, 
				{ KillmoveType::k2H, KillmoveType::k2HAxe } 
			}
		};

		// Event names for killmoves best triggered when the player is behind the target.
		static inline const std::set<uint32_t> BACKSTAB_KILLMOVES_HASHES_SET =
		{
			Hash("KillMove2HMStabFromBehind"),
			Hash("KillMove2HWHackFromBehind"),
			Hash("KillMoveBackStab"),
			Hash("KillMoveSneak1HMThroatSlit"),
			Hash("KillMoveSneakH2HNeckBreak"),
			Hash("KillMoveSneakH2HSleeper"),
			Hash("pa_1HMKillMoveBackStab"),
			Hash("VampireLordRightPowerAttackFeedBack")
		};

		// Event names for decapitation killmoves.
		static inline const std::set<uint32_t> DECAP_KILLMOVES_HASHES_SET = 
		{
			Hash("KillMove2HMDecapBleedOut"),
			Hash("KillMove2HMDecapSlash"),
			Hash("KillMove2HWDecap"),
			Hash("KillMove2HWDecapBleedOut"),
			Hash("KillMoveDecapSlash"),
			Hash("KillMoveDecapSlashAxe"),
			Hash("KillMoveDualWieldBleedOutDecap"),
			Hash("KillMoveDualWieldDecap"),
			Hash("pa_1HMKillMoveDecapBleedOut"),
			Hash("pa_1HMKillMoveDecapKnife"),
			Hash("pa_1HMKillMoveDecapSlash"),
			Hash("pa_2HMKillMoveDecapBleedOut"),
			Hash("pa_2HMKillMoveDecapSlash"),
			Hash("pa_2HWKillMoveDecapA"),
			Hash("pa_KillMoveDualWieldDecap")
		};

		// Enderal skillbook form IDs matched with their skillbook tiers.
		static inline const std::unordered_map<EnderalSkillbookTier, std::vector<RE::FormID>> 
		ENDERAL_TIERED_SKILLBOOKS_MAP = 
		{
			{ 
				EnderalSkillbookTier::kApprentice,
				{ 
					0x31ACC,
					0x39936,
					0x39941,
					0x3F86A,
					0x85614,
					0x85618,
					0x8561C,
					0x85621,
					0x85624,
					0x85641,
					0x8591F,
					0xE75F3,
					0xE7622,
					0xE7626,
					0xE762A,
					0xE762E,
					0xE7632,
					0xE7636 
				} 
			},
			{ 
				EnderalSkillbookTier::kAdept,
				{ 
					0x31ACE,
					0x39937,
					0x39942,
					0x3F86B,
					0x85615,
					0x85619,
					0x8561D,
					0x85622,
					0x85625,
					0x85643,
					0x8591E,
					0xE75F4,
					0xE7623,
					0xE7627,
					0xE762B,
					0xE762F,
					0xE7633,
					0xE7637 
				} 
			},
			{ 
				EnderalSkillbookTier::kExpert,
				{ 
					0x33A5F,
					0x39938,
					0x39943,
					0x3F86C,
					0x85616,
					0x8561A,
					0x8561E,
					0x85620,
					0x85626,
					0x85644,
					0x8591D,
					0xE75F0,
					0xE7624,
					0xE7628,
					0xE762C,
					0xE7630,
					0xE7634,
					0xE7638 
				} 
			},
			{ 
				EnderalSkillbookTier::kMaster,
				{ 
					0x39935,
					0x39939,
					0x39944,
					0x3F86D,
					0x85617,
					0x8561B,
					0x8561F,
					0x85623,
					0x85627,
					0x85642,
					0x8591B,
					0xE75F2,
					0xE7625,
					0xE7629,
					0xE762D,
					0xE7631,
					0xE7635,
					0xE7639
				} 
			}
		};

		// Enderal skillbook form IDs mapped to their tier and associated skill AV.
		static inline const 
		std::unordered_map<RE::FormID, std::pair<EnderalSkillbookTier, RE::ActorValue>> 
		ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP = 
		{
			{ 0x31ACC, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kOneHanded } },
			{ 0x39936, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kTwoHanded } },
			{ 0x39941, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kBlock } },
			{ 0x3F86A, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kHeavyArmor } },
			{ 0x85614, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kDestruction } },
			{ 0x85618, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kAlteration } },
			{ 0x8561C, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kIllusion } },
			{ 0x85621, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kConjuration } },
			{ 0x85624, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kRestoration } },
			{ 0x85641, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kArchery } },
			{ 0x8591F, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kSneak } },
			{ 0xE75F3, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kLightArmor } },
			{ 0xE7622, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kAlchemy } },
			{ 0xE7626, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kEnchanting } },
			{ 0xE762A, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kLockpicking } },
			{ 0xE762E, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kPickpocket } },
			{ 0xE7632, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kSmithing } },
			{ 0xE7636, { EnderalSkillbookTier::kApprentice, RE::ActorValue::kSpeech } },
			{ 0x31ACE, { EnderalSkillbookTier::kAdept, RE::ActorValue::kOneHanded } },
			{ 0x39937, { EnderalSkillbookTier::kAdept, RE::ActorValue::kTwoHanded } },
			{ 0x39942, { EnderalSkillbookTier::kAdept, RE::ActorValue::kBlock } },
			{ 0x3F86B, { EnderalSkillbookTier::kAdept, RE::ActorValue::kHeavyArmor } },
			{ 0x85615, { EnderalSkillbookTier::kAdept, RE::ActorValue::kDestruction } },
			{ 0x85619, { EnderalSkillbookTier::kAdept, RE::ActorValue::kAlteration } },
			{ 0x8561D, { EnderalSkillbookTier::kAdept, RE::ActorValue::kIllusion } },
			{ 0x85622, { EnderalSkillbookTier::kAdept, RE::ActorValue::kConjuration } },
			{ 0x85625, { EnderalSkillbookTier::kAdept, RE::ActorValue::kRestoration } },
			{ 0x85643, { EnderalSkillbookTier::kAdept, RE::ActorValue::kArchery } },
			{ 0x8591E, { EnderalSkillbookTier::kAdept, RE::ActorValue::kSneak } },
			{ 0xE75F4, { EnderalSkillbookTier::kAdept, RE::ActorValue::kLightArmor } },
			{ 0xE7623, { EnderalSkillbookTier::kAdept, RE::ActorValue::kAlchemy } },
			{ 0xE7627, { EnderalSkillbookTier::kAdept, RE::ActorValue::kEnchanting } },
			{ 0xE762B, { EnderalSkillbookTier::kAdept, RE::ActorValue::kLockpicking } },
			{ 0xE762F, { EnderalSkillbookTier::kAdept, RE::ActorValue::kPickpocket } },
			{ 0xE7633, { EnderalSkillbookTier::kAdept, RE::ActorValue::kSmithing } },
			{ 0xE7637, { EnderalSkillbookTier::kAdept, RE::ActorValue::kSpeech } },
			{ 0x33A5F, { EnderalSkillbookTier::kExpert, RE::ActorValue::kOneHanded } },
			{ 0x39938, { EnderalSkillbookTier::kExpert, RE::ActorValue::kTwoHanded } },
			{ 0x39943, { EnderalSkillbookTier::kExpert, RE::ActorValue::kBlock } },
			{ 0x3F86C, { EnderalSkillbookTier::kExpert, RE::ActorValue::kHeavyArmor } },
			{ 0x85616, { EnderalSkillbookTier::kExpert, RE::ActorValue::kDestruction } },
			{ 0x8561A, { EnderalSkillbookTier::kExpert, RE::ActorValue::kAlteration } },
			{ 0x8561E, { EnderalSkillbookTier::kExpert, RE::ActorValue::kIllusion } },
			{ 0x85620, { EnderalSkillbookTier::kExpert, RE::ActorValue::kConjuration } },
			{ 0x85626, { EnderalSkillbookTier::kExpert, RE::ActorValue::kRestoration } },
			{ 0x85644, { EnderalSkillbookTier::kExpert, RE::ActorValue::kArchery } },
			{ 0x8591D, { EnderalSkillbookTier::kExpert, RE::ActorValue::kSneak } },
			{ 0xE75F0, { EnderalSkillbookTier::kExpert, RE::ActorValue::kLightArmor } },
			{ 0xE7624, { EnderalSkillbookTier::kExpert, RE::ActorValue::kAlchemy } },
			{ 0xE7628, { EnderalSkillbookTier::kExpert, RE::ActorValue::kEnchanting } },
			{ 0xE762C, { EnderalSkillbookTier::kExpert, RE::ActorValue::kLockpicking } },
			{ 0xE7630, { EnderalSkillbookTier::kExpert, RE::ActorValue::kPickpocket } },
			{ 0xE7634, { EnderalSkillbookTier::kExpert, RE::ActorValue::kSmithing } },
			{ 0xE7638, { EnderalSkillbookTier::kExpert, RE::ActorValue::kSpeech } },
			{ 0x39935, { EnderalSkillbookTier::kMaster, RE::ActorValue::kOneHanded } },
			{ 0x39939, { EnderalSkillbookTier::kExpert, RE::ActorValue::kTwoHanded } },
			{ 0x39944, { EnderalSkillbookTier::kExpert, RE::ActorValue::kBlock } },
			{ 0x3F86D, { EnderalSkillbookTier::kExpert, RE::ActorValue::kHeavyArmor } },
			{ 0x85617, { EnderalSkillbookTier::kExpert, RE::ActorValue::kDestruction } },
			{ 0x8561B, { EnderalSkillbookTier::kExpert, RE::ActorValue::kAlteration } },
			{ 0x8561F, { EnderalSkillbookTier::kExpert, RE::ActorValue::kIllusion } },
			{ 0x85623, { EnderalSkillbookTier::kExpert, RE::ActorValue::kConjuration } },
			{ 0x85627, { EnderalSkillbookTier::kExpert, RE::ActorValue::kRestoration } },
			{ 0x85642, { EnderalSkillbookTier::kExpert, RE::ActorValue::kArchery } },
			{ 0x8591B, { EnderalSkillbookTier::kExpert, RE::ActorValue::kSneak } },
			{ 0xE75F2, { EnderalSkillbookTier::kExpert, RE::ActorValue::kLightArmor } },
			{ 0xE7625, { EnderalSkillbookTier::kExpert, RE::ActorValue::kAlchemy } },
			{ 0xE7629, { EnderalSkillbookTier::kExpert, RE::ActorValue::kEnchanting } },
			{ 0xE762D, { EnderalSkillbookTier::kExpert, RE::ActorValue::kLockpicking } },
			{ 0xE7631, { EnderalSkillbookTier::kExpert, RE::ActorValue::kPickpocket } },
			{ 0xE7635, { EnderalSkillbookTier::kExpert, RE::ActorValue::kSmithing } },
			{ 0xE7639, { EnderalSkillbookTier::kExpert, RE::ActorValue::kSpeech } }
		};

		// Enderal skill mapped to index which is then used to pick a random skillbook 
		// to give to other active players when one is looted.
		static inline const std::unordered_map<RE::ActorValue, uint8_t> 
		ENDERAL_SKILL_TO_SKILLBOOK_INDEX_MAP = 
		{
			{ RE::ActorValue::kOneHanded, 0 },
			{ RE::ActorValue::kTwoHanded, 1 },
			{ RE::ActorValue::kBlock, 2 },
			{ RE::ActorValue::kHeavyArmor, 3 },
			{ RE::ActorValue::kDestruction, 4 },
			{ RE::ActorValue::kAlteration, 5 },
			{ RE::ActorValue::kIllusion, 6 },
			{ RE::ActorValue::kConjuration, 7 },
			{ RE::ActorValue::kRestoration, 8 },
			{ RE::ActorValue::kArchery, 9 },
			{ RE::ActorValue::kSneak, 10 },
			{ RE::ActorValue::kLightArmor, 11 },
			{ RE::ActorValue::kAlchemy, 12 },
			{ RE::ActorValue::kEnchanting, 13 },
			{ RE::ActorValue::kLockpicking, 14 },
			{ RE::ActorValue::kPickpocket, 15 },
			{ RE::ActorValue::kSmithing, 16 },
			{ RE::ActorValue::kSpeech, 17 }
		};

		// Set of menus for which menu control is transferable from one player to another.
		// NOTE:
		// Opening these menus never involves importing a companion player's data to P1,
		// and should also only contain menus that can open without a clear trigger
		// that links the opened menu back to a player.
		// Ex. Activating a refr that opens a custom menu, or the Enderal level up message
		// box opening up once combat ends, or a guard starting a warning dialogue with the player.
		static inline const std::set<std::string_view> TRANSFERABLE_CONTROL_MENU_NAMES = 
		{
			RE::DialogueMenu::MENU_NAME,
			RE::MessageBoxMenu::MENU_NAME,
			CUSTOM_MENU
		};

		// Set of menus that a companion player can interact with when opened.
		// Most of these menus will have custom control maps, 
		// and if not, a default control map is used.
		static inline const std::set<std::string_view> SUPPORTED_MENU_NAMES = 
		{
			RE::BarterMenu::MENU_NAME,
			RE::BookMenu::MENU_NAME,
			RE::Console::MENU_NAME,
			RE::ContainerMenu::MENU_NAME,
			RE::CraftingMenu::MENU_NAME,
			RE::DialogueMenu::MENU_NAME,
			RE::FavoritesMenu::MENU_NAME,
			RE::GiftMenu::MENU_NAME,
			RE::JournalMenu::MENU_NAME,
			RE::InventoryMenu::MENU_NAME, 
			RE::LevelUpMenu::MENU_NAME, 
			RE::MagicMenu::MENU_NAME,
			RE::MapMenu::MENU_NAME, 
			RE::MessageBoxMenu::MENU_NAME,
			RE::RaceSexMenu::MENU_NAME,
			RE::SleepWaitMenu::MENU_NAME,
			RE::StatsMenu::MENU_NAME, 
			RE::TrainingMenu::MENU_NAME,
			RE::TweenMenu::MENU_NAME, 
			ENHANCED_HERO_MENU,
			CUSTOM_MENU,
			LOOT_MENU
		};

		// Player nodes to raycast to from the camera node position 
		// when performing camera LOS checks.
		static inline const std::vector<RE::BSFixedString> CAM_VISIBILITY_NPC_NODES = 
		{
			RE::FixedStrings::GetSingleton()->npcSpine,
			RE::FixedStrings::GetSingleton()->npcSpine1,
			RE::FixedStrings::GetSingleton()->npcSpine2,
			RE::FixedStrings::GetSingleton()->npcHead
		};

		// Player nodes to raycast from when performing ragdoll collision checks.
		static inline const std::vector<RE::BSFixedString> RAGDOLL_COLLISION_NPC_NODES = 
		{
			RE::FixedStrings::GetSingleton()->npcSpine,
			RE::FixedStrings::GetSingleton()->npcSpine1,
			RE::FixedStrings::GetSingleton()->npcSpine2,
			RE::FixedStrings::GetSingleton()->npcHead,
			RE::FixedStrings::GetSingleton()->npcLUpperArm,
			RE::FixedStrings::GetSingleton()->npcRUpperArm,
			RE::FixedStrings::GetSingleton()->npcLForearm,
			"NPC R Forearm [RLar]",
			"NPC L Hand [LHnd]",
			"NPC R Hand [RHnd]",
			"NPC L Thigh [LThg]",
			"NPC R Thigh [RThg]",
			RE::FixedStrings::GetSingleton()->npcLCalf,
			RE::FixedStrings::GetSingleton()->npcRCalf,
			RE::FixedStrings::GetSingleton()->npcLFoot,
			RE::FixedStrings::GetSingleton()->npcRFoot
		};

		// Player spinal nodes that have their pitch/roll/yaw angles modified 
		// when the player adjusts their aim pitch.
		// Headtracking gets a bit wonky if the head node is locked in place while facing a target.
		static inline const std::vector<RE::BSFixedString> TORSO_ADJUSTMENT_NPC_NODES = 
		{
			RE::FixedStrings::GetSingleton()->npcSpine,
			RE::FixedStrings::GetSingleton()->npcSpine1,
			RE::FixedStrings::GetSingleton()->npcSpine2
		};

		// Outer outline of prong of aim pitch indicator arrow facing right.
		// Origin: leftmost point, centered vertically on prong.
		static inline const std::vector<glm::vec2> AIM_PITCH_INDICATOR_HEAD_OUTER_PIXEL_OFFSETS =
		{
			{ 0.0f, 0.0f },
			{ 0.0f, 5.0f },
			{ 1.5f, 5.0f },
			{ 3.5f, 4.0f },
			{ 5.0f, 4.0f },
			{ 7.0f, 5.0f },
			{ 7.5f, 5.0f },
			{ 19.5f, 0.0f },
			{ 7.5f, -5.0f },
			{ 7.0f, -5.0f },
			{ 5.0f, -4.0f },
			{ 1.5f, -5.0f },
			{ 0.0f, -5.0f }
		};
		
		// Default length of the aim pitch indicator outer shape in pixels.
		static inline const float AIM_PITCH_INDICATOR_OUTER_DEF_LENGTH = 20.0f;

		// Inner outline of prong of aim pitch indicator arrow facing right.
		// Origin: leftmost point, centered vertically on prong.
		static inline const std::vector<glm::vec2> AIM_PITCH_INDICATOR_HEAD_MID_PIXEL_OFFSETS =
		{
			{ 2.0f, 0.0f },
			{ 2.0f, 1.75f },
			{ 7.0f, 3.75f },
			{ 8.0f, 3.75f },
			{ 17.0f, 0.0f },
			{ 8.0f, -3.75f },
			{ 7.0f, -3.75f },
			{ 2.0f, -1.75f },
		};
		
		// Default length of the aim pitch indicator middle shape in pixels.
		static inline const float AIM_PITCH_INDICATOR_MID_DEF_LENGTH = 15.0f;

		// Body of prong of aim pitch indicator arrow facing right.
		// Origin: leftmost point, centered vertically on prong.
		// Default height: 12 pixels.
		// Default width: 11 pixels.
		static inline const std::vector<glm::vec2> AIM_PITCH_INDICATOR_HEAD_INNER_PIXEL_OFFSETS =
		{
			{ 6.5f, 0.0f },
			{ 6.5f, 1.0f },
			{ 7.5f, 1.5f },
			{ 8.0f, 1.5f },
			{ 12.0f, 0.5f },
			{ 12.0f, 0.0f },
			{ 8.0f, -1.5f },
			{ 7.5f, -1.5f },
			{ 6.5f, -1.0f },
		};
		
		// Default length of the aim pitch indicator inner shape in pixels.
		static inline const float AIM_PITCH_INDICATOR_INNER_DEF_LENGTH = 5.5f;

		// Prong of default crosshair facing right.
		// Origin: leftmost point on prong.
		// Default length: 22px
		static inline const std::vector<glm::vec2> CROSSHAIR_PRONG_PIXEL_OFFSETS = 
		{
			{ 0.0f, 0.0f },
			{ 3.0f, 6.0f },
			{ 6.0f, 3.0f },
			{ 22.0f, 0.0f },
			{ 6.0f, -3.0f },
			{ 3.0f, -6.0f }
		};

		// Default prong length for the player crosshair in pixels.
		static inline const float CROSSHAIR_PRONG_DEF_LENGTH = 22.0f;

		// Upper "diamond" portion of the quest marker indicator.
		// Drawn first.
		// Origin: lower portion's bottom.
		// Default length: 20
		static inline const std::vector<glm::vec2> PLAYER_INDICATOR_UPPER_PIXEL_OFFSETS = 
		{
			{ 0.0f, -47.0f },
			{ -5.0f, -38.0f },
			{ -5.0f, -37.0f },
			{ -4.0f, -37.0f },
			{ 0.0f, -27.0f },
			{ 4.0f, -37.0f },
			{ 5.0f, -37.0f },
			{ 5.0f, -38.0f }
		};

		// Lower "caret" portion of the quest marker indicator.
		// Drawn second.
		// Origin: bottom of the caret.
		// Default length: 34
		static inline const std::vector<glm::vec2> PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS = 
		{
			{ -14.0f, -33.0f },
			{ -3.0f, -34.0f },
			{ 0.0f, -26.0f },
			{ 3.0f, -34.0f },
			{ 14.0f, -33.0f },
			{ 0.0f, 0.0f }
		};

		// Default length of the player indicator in pixels.
		static inline const float PLAYER_INDICATOR_DEF_LENGTH = 47.0f;

		// Adjustable player left arm nodes when rotating that arm.
		static inline const std::unordered_set<RE::BSFixedString> ADJUSTABLE_LEFT_ARM_NODES = 
		{
			RE::FixedStrings::GetSingleton()->npcLUpperArm,
			RE::FixedStrings::GetSingleton()->npcLForearm,
			"NPC L Hand [LHnd]"
		};

		// Adjustable player right arm nodes when rotating that arm.
		static inline const std::unordered_set<RE::BSFixedString> ADJUSTABLE_RIGHT_ARM_NODES = 
		{
			RE::FixedStrings::GetSingleton()->npcRUpperArm,
			"NPC R Forearm [RLar]",
			"NPC R Hand [RHnd]"
		};

		// Adjustable player torso nodes when adjusting aim pitch.
		static inline const std::unordered_set<RE::BSFixedString> ADJUSTABLE_TORSO_NODES =
		{
			RE::FixedStrings::GetSingleton()->npcSpine,
			RE::FixedStrings::GetSingleton()->npcSpine1,
			RE::FixedStrings::GetSingleton()->npcSpine2
		};

		// Maps weapon animation types to their 1H or 2H weapon animation type equivalents.
		static inline const std::unordered_map<RE::WEAPON_TYPE, RE::WEAPON_TYPE> 
		WEAP_ANIM_SWITCH_MAP =
		{
			{ RE::WEAPON_TYPE::kBow, RE::WEAPON_TYPE::kBow },
			{ RE::WEAPON_TYPE::kCrossbow, RE::WEAPON_TYPE::kCrossbow },
			{ RE::WEAPON_TYPE::kHandToHandMelee, RE::WEAPON_TYPE::kHandToHandMelee },
			{ RE::WEAPON_TYPE::kOneHandAxe, RE::WEAPON_TYPE::kTwoHandAxe },
			{ RE::WEAPON_TYPE::kOneHandDagger, RE::WEAPON_TYPE::kTwoHandSword },
			{ RE::WEAPON_TYPE::kOneHandMace, RE::WEAPON_TYPE::kTwoHandAxe },
			{ RE::WEAPON_TYPE::kOneHandSword, RE::WEAPON_TYPE::kTwoHandSword },
			{ RE::WEAPON_TYPE::kStaff, RE::WEAPON_TYPE::kTwoHandAxe },
			{ RE::WEAPON_TYPE::kTwoHandAxe, RE::WEAPON_TYPE::kOneHandAxe },
			{ RE::WEAPON_TYPE::kTwoHandSword, RE::WEAPON_TYPE::kOneHandSword }
		};

		//
		// Members
		//

		// Camera Manager.
		// Controls placement of the camera when running.
		std::unique_ptr<CameraManager> cam;
		// Controller Data Holder.
		// Keeps track of players' controller button and analog sticks' states.
		std::unique_ptr<ControllerDataHolder> cdh;
		// Menu Input Manager.
		// Allows companion players to control menus as if they were P1, 
		// with some limitations, of course.
		std::unique_ptr<MenuInputManager> mim;
		// Menu Opening Action Requests Manager.
		// Keeps track of requests to open menus, and from that info, determines which controller
		// should be awarded control of menus when they open.
		std::unique_ptr<MenuOpeningActionRequestsManager> moarm;
		// Contact listener for thrown object collisions.
		std::unique_ptr<ContactListener> contactListener;
		// Stores info about the last request to copy player data 
		// between a co-op companion player and P1.
		std::unique_ptr<CopyPlayerDataRequestInfo> copyDataReqInfo;
		// Saved menu data for the menu-controlling co-op player.
		std::unique_ptr<ExchangeablePlayerData> coopCompanionExchangeableData;
		// Saved UseSkill() call arguments used to delay awarding of melee Skill XP
		// until after friendly fire and other conditions are checked.
		std::unique_ptr<LastP1MeleeUseSkillCallArgs> lastP1MeleeUseSkillCallArgs;
		// Saved menu data for P1 before importing another player's data.
		std::unique_ptr<ExchangeablePlayerData> p1ExchangeableData;
		// Player Action Functions Holder.
		// Handles calling player action condition/press/hold/release functions.
		std::unique_ptr<PlayerActionFunctionsHolder> paFuncsHolder;
		// Player Action Info Holder.
		// Holds player action information.
		std::unique_ptr<PlayerActionInfoHolder> paInfoHolder;
		// Detached thread running queued async tasks.
		std::unique_ptr<TaskRunner> taskRunner;

		// P1's actor
		RE::ActorPtr player1Actor;
		// Effects to indicate that the companion player is 'paragliding'.
		RE::BGSArtObject* paraglideIndicatorEffect1;
		RE::BGSArtObject* paraglideIndicatorEffect2;
		// Effects applied when reviving players.
		RE::BGSArtObject* reviveDragonSoulEffect;
		RE::BGSArtObject* reviveHealingEffect;
		// Reference alias to P1.
		// Used to register co-op player menu events:
		// Summoning Menu, Debug Menu. Helper Menu.
		RE::BGSRefAlias* player1RefAlias;
		// Equip slots: both hands, either hand, left, right, voice.
		RE::BGSEquipSlot* bothHandsEquipSlot;
		RE::BGSEquipSlot* eitherHandEquipSlot;
		RE::BGSEquipSlot* leftHandEquipSlot;
		RE::BGSEquipSlot* rightHandEquipSlot;
		RE::BGSEquipSlot* voiceEquipSlot;
		// NPC keyword (used when filtering playable races).
		RE::BGSKeyword* npcKeyword;
		// NOTE: Unused as of now:
		// Default formlist containing base game shouts' variation spells.
		RE::BGSListForm* shoutVarSpellsFormList;
		// Perks that add new tecniques/skills linked with a specific anim event.
		RE::BGSPerk* criticalChargePerk;
		RE::BGSPerk* dualCastingAlterationPerk;
		RE::BGSPerk* dualCastingConjurationPerk;
		RE::BGSPerk* dualCastingDestructionPerk;
		RE::BGSPerk* dualCastingIllusionPerk;
		RE::BGSPerk* dualCastingRestorationPerk;
		RE::BGSPerk* greatCriticalChargePerk;
		RE::BGSPerk* powerBashPerk;
		RE::BGSPerk* shieldChargePerk;
		RE::BGSPerk* sneakRollPerk;
		// Other perks referenced when checking conditions for certain actions.
		RE::BGSPerk* assassinsBladePerk;
		RE::BGSPerk* backstabPerk;
		RE::BGSPerk* deadlyAimPerk;
		RE::BGSPerk* quickShotPerk;
		// Paraglide movement type.
		RE::BGSMovementType* paraglidingMT;
		// Paraglider updraft effect.
		RE::EffectSetting* tarhielsGaleEffect;
		// For QuickLoot compatibility:
		// Last set container refr's handle.
		RE::ObjectRefHandle reqQuickLootContainerHandle;
		// Paraglide updraft spell.
		RE::SpellItem* tarhielsGaleSpell;
		// Current copied menu data types (co-op companion onto P1).
		RE::stl::enumeration<CopyablePlayerDataTypes, uint16_t> copiedPlayerDataTypes;
		// Bound object for fists.
		RE::TESBoundObject* fists;
		// Shaders.
		RE::TESEffectShader* activateHighlightShader;
		RE::TESEffectShader* dragonHolesShader;
		RE::TESEffectShader* dragonSoulAbsorbShader;
		RE::TESEffectShader* ghostFXShader;
		// Globals.
		// [Enderal only]
		// Crafting/Learning/Memory point globals.
		RE::TESGlobal* craftingPointsGlob;
		RE::TESGlobal* learningPointsGlob;
		RE::TESGlobal* memoryPointsGlob;
		// [Both]
		// Player level global.
		RE::TESGlobal* playerLevelGlob;
		// Is a summoning request active?
		// Set by summoning menu script.
		// Prevents opening of multiple summoning menus at the same time.
		RE::TESGlobal* summoningMenuOpenGlob;
		// Is P1 transformed into a werewolf?
		RE::TESGlobal* werewolfTransformationGlob;

		// Menu opening requests' event registrations.
		SKSE::Impl::RegistrationSet<void, RE::Actor*, uint32_t, uint32_t> onCoopHelperMenuRequest =
			SKSE::Impl::RegistrationSet<std::enable_if_t<std::conjunction_v<
				RE::BSScript::is_return_convertible<RE::Actor*>, 
				RE::BSScript::is_return_convertible<uint32_t>>>,
				RE::Actor*, uint32_t, uint32_t>("OnCoopHelperMenuRequest"sv);
		SKSE::Impl::RegistrationSet<void, RE::Actor*, uint32_t> onDebugMenuRequest =
			SKSE::Impl::RegistrationSet<std::enable_if_t<std::conjunction_v<
				RE::BSScript::is_return_convertible<RE::Actor*>, 
				RE::BSScript::is_return_convertible<uint32_t>>>,
				RE::Actor*, uint32_t>("OnDebugMenuRequest"sv);
		SKSE::Impl::RegistrationSet<void> onSummoningMenuRequest =
			SKSE::Impl::RegistrationSet<void>("OnSummoningMenuRequest"sv);

		// Time point at which co-op companion player Enderal skill AVs 
		// were last checked for changes.
		SteadyClock::time_point lastCoopCompanionSkillLevelsCheckTP;
		// Time point at which all supported menus were last closed.
		SteadyClock::time_point lastSupportedMenusClosedTP;
		// Time point at which the level up XP threshold was last checked.
		SteadyClock::time_point lastXPThresholdCheckTP;

		// Mutex for threads attempting to add skill XP for each player.
		std::array<std::mutex, ALYSLC_MAX_PLAYER_COUNT> skillXPMutexes;
		// List of current players (P1 and co-op companions, active and inactive).
		std::array<std::shared_ptr<CoopPlayer>, ALYSLC_MAX_PLAYER_COUNT> coopPlayers;
		// Set of co-op entities that are players or are blocked from selection.
		// Used to filter out these entities in the targeting manager.
		std::set<RE::FormID> coopEntityBlacklistFIDSet;
		// For co-op companion players:
		// Since the package procedure 'UseMagic' does not allow for a variable spell, 
		// we can still cast any spell by setting the procedure's spell target to a placeholder 
		// spell that then has the requested spell's data copied into it.
		// Holds all placeholder spells for all players.
		std::set<RE::SpellItem*> placeholderSpellsSet;
		// Serializable data to write to/read from the SKSE co-save for each player, 
		// indexed by the players' form IDs.
		std::unordered_map<RE::FormID, std::unique_ptr<SerializablePlayerData>> 
		serializablePlayerData;
		// Killmoves idles categorized by skeleton name hash and then by weapon type.
		std::unordered_map<uint32_t, std::vector<std::vector<RE::TESIdleForm*>>> 
		skeletonKillmoveIdlesMap;
		// All bound arrow ammo types. Used to pair the correct bound arrows 
		// when co-op companion players equip bound bows.
		std::vector<RE::TESAmmo*> boundArrowAmmoList;
		// Indicates which hand(s) are casting when evaluating a ranged attack package 
		// Order: LH, RH, 2H, Dual, Shout, Voice.
		std::vector<RE::TESGlobal*> castingGlobVars;
		// Co-op entities that are players or are blocked from selection.
		// Used to filter out these entities in the targeting manager.
		std::vector<RE::ActorPtr> coopEntityBlacklist;
		// Extra storage space for co-op players.
		// Also used as temporary storage when swapping inventories.
		// Indexed by player ID.
		std::vector<RE::TESObjectREFRPtr> coopInventoryChests;
		// Co-op package formlists used as package override stacks.
		// Holds the co-op players' packages below.
		std::vector<RE::BGSListForm*> coopPackageFormlists;
		// List of co-op player packages. 
		// Currently 4 PER player (in order): 
		// default, combat override, ranged attack, special interaction.
		std::vector<RE::TESPackage*> coopPackages;
		// Base factions that all co-op players should be a member of.
		std::vector<RE::TESFaction*> coopPlayerFactions;
		// General killmoves applicable to humanoid and any other unaccounted-for races.
		// Categorized by weapon type.
		std::vector<std::vector<RE::TESIdleForm*>> genericKillmoveIdles;
		// NOTE: Currently unused since the 'Shout' package procedure does not work.
		// Shouts which are equipped and overriden by another shout's data.
		// Used in ranged attack package.
		std::vector<RE::TESShout*> placeholderShouts;
		// For co-op companion players:
		// Since the package procedure 'UseMagic' does not allow for a variable spell,
		// we can still cast any spell by setting the procedure's spell target to a placeholder 
		// spell that then has the requested spell's data copied into it.
		// Per player: LH, RH, 2H, and voice casting.
		std::vector<RE::SpellItem*> placeholderSpells;
		// Weapon type keywords list. 
		std::vector<RE::BGSKeyword*> weapTypeKeywordsList;
		// All players constructed.
		bool allPlayersInit = false;
		// Co-op session started (all players summoned and session cleanup
		// and player dismissal are not finished).
		bool coopSessionActive = false;
		// Finished setting global co-op data.
		bool globalDataInit = false;
		// Is the game loading a save?
		bool loadingASave = false;
		// Default level up base and mult game settings' values
		// set each time the player first loads a save, 
		// since the default values are re-applied on loading a save.
		float defXPLevelUpBase;
		float defXPLevelUpMult;
		// [Enderal only]
		// Saved Crafting/Learning/Memory points from the last time these globals were checked.
		// Used to check for increases in these values each frame.
		float savedCraftingPoints;
		float savedLearningPoints;
		float savedMemoryPoints;
		// At least one supported menu is open.
		std::atomic_bool supportedMenuOpen;
		// Menu controller ID mutex when setting/resetting menu controller IDs.
		std::mutex menuCIDMutex;
		// P1 skill XP modification mutex.
		std::mutex p1SkillXPMutex;
		// The last menu CID resolved (computed during the ProcessMessage() hook)
		// and before the menu opens/closes/updates.
		int32_t lastResolvedMenuCID;
		// Controller ID for the player currently controlling menus.
		// -1 if only "always open menus" are open.
		int32_t menuCID;
		// Controller ID for P1 (typically, but not always, 0)
		int32_t player1CID;
		// Last set menu controller ID for the player that was in control of menus.
		// Should never be -1 if a controllable menu is open, 
		// unless no player has opened the Summoning Menu after loading a save.
		int32_t prevMenuCID;
		// For QuickLoot compatibility:
		// CID of the player who last requested to open the menu.
		int32_t quickLootReqCID;
		// Number of active, summoned co-op players.
		uint32_t activePlayers;
		// Last unlocked shared perks count on perk tree export.
		uint32_t exportUnlockedSharedPerksCount;
		// Last unlocked shared perks count on perk tree import.
		uint32_t importUnlockedSharedPerksCount;
		// Number of living co-op players.
		uint32_t livingPlayers;

		// TODO: Use as max number of usable perk points,
		// allowing for a variable number of perk points
		// given per level up to each player,
		// instead of just 1 as in the default game.
		// Saved perk count from the P1 singleton.
		int32_t p1SavedPerkCount;
		int32_t perkPointsAvailable;
		int32_t totalPerkPointsGiven;

	private:
		GlobalCoopData() = default;
		GlobalCoopData(const GlobalCoopData& _globalCoopData) = delete;
		GlobalCoopData(GlobalCoopData&& _globalCoopData) = delete;
		GlobalCoopData& operator=(const GlobalCoopData& _globalCoopData) = delete;
		GlobalCoopData& operator=(GlobalCoopData&& _globalCoopData) = delete;

		//
		// Helpers
		//
				
		// Rescale health, magicka, and stamina AVs to their serialized values for this player.
		// Use the passed-in base level to determine 
		// if the player has leveled up in co-op before rescaling.
		static void RescaleHMS(RE::Actor* a_playerActor, const float& a_baseLevel = 1.0f);
		
		// Rescale all skill AVs for this player.
		// Set skill levels equal to the serialized base AV level + skill increment amount.
		static void RescaleSkillAVs(RE::Actor* a_playerActor);
		
		// The given player has all their perks removed, is refunded all perk points,
		// and can respec.
		// In addition, all other players are refunded shared perk points and can re-select 
		// the perks that they want with these points.
		static void ResetPerkData(RE::Actor* a_playerActor);

		// Resets the given player's health/magicka/stamina actor values to their initial values,
		// undoing all serialized progress to these AVs.
		static void ResetToBaseHealthMagickaStamina(RE::Actor* a_playerActor);

		// Modify P1's level to trigger the game's actor value auto-scaling on other players.
		// If the given player actor is nullptr,
		// all players have their AVs auto-scaled without resetting any perk data.
		// Otherwise, the given player has their AVs auto-scaled 
		// and their perks reset (called on class/race change).
		//
		// If requested, the given player or all active players' serialized data
		// for base skill actor values and unlocked perks is modified.
		// Returns true if successful or if no auto-scaling was necessary.
		static bool TriggerAVAutoScaling(RE::Actor* a_playerActor, bool&& a_updateBaseAVs);
	};
}



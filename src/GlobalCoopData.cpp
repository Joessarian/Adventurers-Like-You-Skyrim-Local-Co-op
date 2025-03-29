#include "GlobalCoopData.h"
#include <CameraManager.h>
#include <Compatibility.h>
#include <Controller.h>
#include <MenuInputManager.h>
#include <Player.h>

namespace ALYSLC 
{
	GlobalCoopData& GlobalCoopData::GetSingleton() 
	{
		static GlobalCoopData glob;
		return glob;
	}

	void GlobalCoopData::InitializeGlobalCoopData(RE::BGSRefAlias* a_player1RefAlias)
	{
		// Initialize all global co-op data.
		// Called from script the first time a save is loaded after starting the game.

		auto& glob = GetSingleton();

		// Global primitive data type members.
		glob.allPlayersInit = false;
		glob.coopSessionActive = false;
		glob.activePlayers = 0;
		glob.exportUnlockedSharedPerksCount = 0;
		glob.importUnlockedSharedPerksCount = 0;
		glob.livingPlayers = 0;
		glob.lastResolvedMenuCID = -1;
		glob.menuCID = -1;
		glob.prevMenuCID = -1;
		glob.p1SavedPerkCount = 0;
		glob.player1CID = -1;
		glob.quickLootReqCID = -1;
		glob.supportedMenuOpen.store(false);
		// Handles.
		glob.reqQuickLootContainerHandle = RE::ObjectRefHandle();
		// Time points.
		glob.lastCoopCompanionSkillLevelsCheckTP =
		glob.lastSupportedMenusClosedTP =
		glob.lastXPThresholdCheckTP = SteadyClock::now();
		// Set global entities and lists.
		glob.player1Actor = RE::ActorPtr(RE::PlayerCharacter::GetSingleton());
		glob.player1RefAlias = a_player1RefAlias;
		glob.castingGlobVars.clear();
		glob.coopEntityBlacklist.clear();
		glob.coopEntityBlacklistFIDSet.clear();
		glob.coopInventoryChests.clear();
		glob.coopPackages.clear();
		glob.coopPackageFormlists.clear();
		glob.coopPlayerFactions.clear();
		glob.placeholderSpells.clear();
		glob.placeholderSpellsSet.clear();

		// Load in data by form ID.
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			// Actors that are blacklisted from selection via targeting.
			// P1 first.
			glob.coopEntityBlacklist.emplace_back(RE::PlayerCharacter::GetSingleton());
			// Co-op companion player actors.
			glob.coopEntityBlacklist.emplace_back
			(
				dataHandler->LookupForm<RE::Actor>(0x22FD, PLUGIN_NAME)
			);
			glob.coopEntityBlacklist.emplace_back
			(
				dataHandler->LookupForm<RE::Actor>(0x22FE, PLUGIN_NAME)
			);
			glob.coopEntityBlacklist.emplace_back
			(
				dataHandler->LookupForm<RE::Actor>(0x22FF, PLUGIN_NAME)
			);

			// Used to check if an actor is a blacklisted one.
			for (const auto& blacklistedActorPtr : glob.coopEntityBlacklist)
			{
				if (blacklistedActorPtr && blacklistedActorPtr.get())
				{
					glob.coopEntityBlacklistFIDSet.insert(blacklistedActorPtr->formID);
				}
			}

			// One inventory chest per player.
			glob.coopInventoryChests.emplace_back
			(
				dataHandler->LookupForm<RE::TESObjectREFR>(0x13674, PLUGIN_NAME)
			);
			glob.coopInventoryChests.emplace_back
			(
				dataHandler->LookupForm<RE::TESObjectREFR>(0x13675, PLUGIN_NAME)
			);
			glob.coopInventoryChests.emplace_back
			(
				dataHandler->LookupForm<RE::TESObjectREFR>(0x13676, PLUGIN_NAME)
			);
			glob.coopInventoryChests.emplace_back
			(
				dataHandler->LookupForm<RE::TESObjectREFR>(0x13677, PLUGIN_NAME)
			);

			// Packages for co-op companion player actors.
			// (Default, combat override, ranged attack packages, special interaction) per player.
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4B4, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4B5, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4B3, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x9E17, PLUGIN_NAME)
			);

			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4B9, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4B6, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4BC, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x9E18, PLUGIN_NAME)
			);

			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4BA, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4B7, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4BD, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x9E19, PLUGIN_NAME)
			);

			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4BB, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4B8, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x4D4BE, PLUGIN_NAME)
			);
			glob.coopPackages.emplace_back
			(
				dataHandler->LookupForm<RE::TESPackage>(0x9E1A, PLUGIN_NAME)
			);

			// Package formlists for each player 
			// that hold the co-op packages above when they are added.
			// (Default, combat override) for each player.
			glob.coopPackageFormlists.emplace_back
			(
				dataHandler->LookupForm<RE::BGSListForm>(0xA509, PLUGIN_NAME)
			);
			glob.coopPackageFormlists.emplace_back
			(
				dataHandler->LookupForm<RE::BGSListForm>(0x9FA3, PLUGIN_NAME)
			);

			glob.coopPackageFormlists.emplace_back
			(
				dataHandler->LookupForm<RE::BGSListForm>(0x1B7DD, PLUGIN_NAME)
			);
			glob.coopPackageFormlists.emplace_back
			(
				dataHandler->LookupForm<RE::BGSListForm>(0x1B7DC, PLUGIN_NAME)
			);

			glob.coopPackageFormlists.emplace_back
			(
				dataHandler->LookupForm<RE::BGSListForm>(0x208D2, PLUGIN_NAME)
			);
			glob.coopPackageFormlists.emplace_back
			(
				dataHandler->LookupForm<RE::BGSListForm>(0x208D3, PLUGIN_NAME)
			);

			glob.coopPackageFormlists.emplace_back
			(
				dataHandler->LookupForm<RE::BGSListForm>(0x24ED3, PLUGIN_NAME)
			);
			glob.coopPackageFormlists.emplace_back
			(
				dataHandler->LookupForm<RE::BGSListForm>(0x24ED2, PLUGIN_NAME)
			);

			// Enderal experience point-gaining faction.
			// Actors in this faction will award P1 XP when they kill certain enemies.
			glob.coopPlayerFactions.emplace_back
			(
				ALYSLC::EnderalCompat::g_enderalSSEInstalled ? 
				RE::TESForm::LookupByID<RE::TESFaction>(0x39DCE) :
				nullptr
			);

			// Global variables that indicate whether a co-op companion player is trying to cast
			// a spell/shout using the LH, RH, 2H, dual, or voice slots.
			// NOTE: Currently, dual casting is not functional.
			// Order: LH, RH, 2H, Dual, Shout, Voice (same as cast package indexing enum).
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x1E87F, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x1E883, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1D7, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x4D4AE, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1DB, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1DF, PLUGIN_NAME)
			);

			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x1E880, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x1E884, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1D8, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x4D4AF, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1DC, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1E0, PLUGIN_NAME)
			);

			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x1E881, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x1E885, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1D9, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x4D4B0, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1DD, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1E1, PLUGIN_NAME)
			);

			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x25998, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x25999, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1DA, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x4D4B1, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1DE, PLUGIN_NAME)
			);
			glob.castingGlobVars.emplace_back
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x5B1E2, PLUGIN_NAME)
			);

			// Other global variables.
			glob.summoningMenuOpenGlob = 
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x11A76, PLUGIN_NAME)
			);
			glob.werewolfTransformationGlob =
			(
				dataHandler->LookupForm<RE::TESGlobal>(0x2EA9A, "Enderal - Forgotten Stories.esm")
			);

			// NOTE: Not functional as of now, but may be used later.
			// Placeholder shouts that hold copied data from existing shouts.
			// Allows co-op companion player actors to cast different shouts 
			// through their ranged attack package.
			// For each player.
			glob.placeholderShouts.emplace_back
			(
				dataHandler->LookupForm<RE::TESShout>(0x5B1D3, PLUGIN_NAME)
			);
			glob.placeholderShouts.emplace_back
			(
				dataHandler->LookupForm<RE::TESShout>(0x5B1D4, PLUGIN_NAME)
			);
			glob.placeholderShouts.emplace_back
			(
				dataHandler->LookupForm<RE::TESShout>(0x5B1D5, PLUGIN_NAME)
			);
			glob.placeholderShouts.emplace_back
			(
				dataHandler->LookupForm<RE::TESShout>(0x5B1D6, PLUGIN_NAME)
			);

			// Placeholder spells that hold copied data from existing spells.
			// Allows co-op companion player actors to cast different spells 
			// through their ranged attack package.
			// Order: (LH, RH, 2H, Voice) for each player.
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x1BD44, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x1BD48, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x58A2A, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x5B1CB, PLUGIN_NAME)
			);

			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x1BD45, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x1BD4B, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x58A2C, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x5B1CD, PLUGIN_NAME)
			);

			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x1BD4A, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x1BD4E, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x58A2E, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x5B1CF, PLUGIN_NAME)
			);

			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x2599A, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x2599C, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x58A30, PLUGIN_NAME)
			);
			glob.placeholderSpells.emplace_back
			(
				dataHandler->LookupForm<RE::SpellItem>(0x5B1D1, PLUGIN_NAME)
			);

			// For spell-to-placeholder spell comparison checks.
			for (auto spell : glob.placeholderSpells)
			{
				glob.placeholderSpellsSet.emplace(spell);
			}

			// Keyword for companion players.
			glob.companionPlayerKeyword = 
			(
				dataHandler->LookupForm<RE::BGSKeyword>(0x4AF06, PLUGIN_NAME)
			);

			// Factions.
			
			// PlayerFaction.
			glob.coopPlayerFactions.emplace_back
			(
				RE::TESForm::LookupByID<RE::TESFaction>(0xDB1)
			);

			// ALYSLC companion player faction (P1 and 3 base co-op characters).
			glob.coopPlayerFactions.emplace_back
			(
				dataHandler->LookupForm<RE::TESFaction>(0x53AF5, GlobalCoopData::PLUGIN_NAME)
			);

			// Potential follower faction.
			glob.coopPlayerFactions.emplace_back
			(
				RE::TESForm::LookupByID<RE::TESFaction>(0x5C84D)
			);
			
			// [Enderal Only]
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
			{
				// PlayerAlliesFaction:
				glob.coopPlayerFactions.emplace_back
				(
					RE::TESForm::LookupByID<RE::TESFaction>(0x39BD7)
				);
			
				// EPFaction: Enderal XP-granting faction.
				// Actors in this faction give the player XP 
				// when they perform actions, such as killing enemies.
				glob.coopPlayerFactions.emplace_back
				(
					RE::TESForm::LookupByID<RE::TESFaction>(0x39DCE)
				);
			}
			else
			{
				// Default factions from the P1's 'Player' actor base.
				
				// MagicCharmFaction
				glob.coopPlayerFactions.emplace_back
				(
					RE::TESForm::LookupByID<RE::TESFaction>(0x8F3E8)
				);

				// MagicAllegianceFaction
				glob.coopPlayerFactions.emplace_back
				(
					RE::TESForm::LookupByID<RE::TESFaction>(0x9E0C9)
				);

				// PlayerBedOwnership
				glob.coopPlayerFactions.emplace_back
				(
					RE::TESForm::LookupByID<RE::TESFaction>(0xF2073)
				);
			}

			// Magic effects.
			glob.tarhielsGaleEffect = 
			(
				ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled ? 
				dataHandler->LookupForm<RE::EffectSetting>(0x10C68, "Paragliding.esp") : 
				nullptr
			);

			// Movement types.
			glob.paraglidingMT = 
			(
				ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled ? 
				dataHandler->LookupForm<RE::BGSMovementType>(0x33D1, "Paragliding.esp") : 
				nullptr
			);

			// Shaders.
			glob.activateHighlightShader = 
			(
				dataHandler->LookupForm<RE::TESEffectShader>(0x3262B, PLUGIN_NAME)
			);
			glob.dragonHolesShader = RE::TESForm::LookupByID<RE::TESEffectShader>(0x4CEC8);
			glob.dragonSoulAbsorbShader = RE::TESForm::LookupByID<RE::TESEffectShader>(0x280C0);
			glob.ghostFXShader = RE::TESForm::LookupByID<RE::TESEffectShader>(0x64D67);

			// Spells.
			glob.tarhielsGaleSpell = 
			(
				ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled ? 
				dataHandler->LookupForm<RE::SpellItem>(0x10C67, "Paragliding.esp") :
				nullptr
			);

			// Get all bound arrow ammo types.
			const auto& ammoList = dataHandler->GetFormArray<RE::TESAmmo>();
			for (auto ammo : ammoList)
			{
				if (!ammo)
				{
					continue;
				}

				if (ammo->HasKeywordByEditorID("WeapTypeBoundArrow"))
				{
					glob.boundArrowAmmoList.emplace_back(ammo);
				}
			}
		}

		// Get all hand equip slots by ID.
		glob.bothHandsEquipSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F45);
		glob.eitherHandEquipSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F44);
		glob.leftHandEquipSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F43);
		glob.rightHandEquipSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13F42);
		glob.voiceEquipSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x25BEE);

		// NPC keyword.
		glob.npcKeyword = RE::TESForm::LookupByID<RE::BGSKeyword>(0x13794);
		// Get all weapon type (aside from Bound Arrow) keywords by ID.
		// Cannot insert by RE::WEAPON_TYPE since warhammer is not included as its own type.
		glob.weapTypeKeywordsList.clear();
		// Warhammer (No weapon type enum member).
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x6D930));
		// Sword.
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x1E711));
		// Dagger.
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x1E713));
		// War Axe.
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x1E712));
		// Mace.
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x1E714));
		// Greatsword.
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x6D931));
		// Battleaxe.
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x6D932));
		// Bow.
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x1E715));
		// Staff.
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x1E716));
		// Crossbow (No weapon type keyword, so using bow).
		glob.weapTypeKeywordsList.emplace_back(RE::TESForm::LookupByID<RE::BGSKeyword>(0x1E715));

		//=================
		// Base game forms.
		//=================
		// Art Objects:
		glob.paraglideIndicatorEffect1 = 
		(
			RE::TESForm::LookupByEditorID<RE::BGSArtObject>("FXWispParticleAttachObject")
		);
		glob.paraglideIndicatorEffect2 = 
		(
			RE::TESForm::LookupByEditorID<RE::BGSArtObject>("CallOfValorTargetFX01")
		);
		glob.reviveDragonSoulEffect = RE::TESForm::LookupByID<RE::BGSArtObject>(0x2E6AA);
		glob.reviveHealingEffect = RE::TESForm::LookupByID<RE::BGSArtObject>(0x3F810);
		// Bound objects.
		// 2H slot clearer.
		glob.fists = RE::TESForm::LookupByID<RE::TESBoundObject>(0x1F4);
		if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
		{
			// Formlists:
			glob.shoutVarSpellsFormList = RE::TESForm::LookupByID<RE::BGSListForm>(0x167D9);
			// Perks:
			glob.assassinsBladePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58211);
			glob.backstabPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58210);
			glob.criticalChargePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0xCB406);
			glob.deadlyAimPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x1036F0);
			glob.dualCastingAlterationPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153CD);
			glob.dualCastingConjurationPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153CE);
			glob.dualCastingDestructionPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153CF);
			glob.dualCastingIllusionPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153D0);
			glob.dualCastingRestorationPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153D1);
			glob.greatCriticalChargePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0xCB407);
			glob.powerBashPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58F67);
			glob.quickShotPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x105F19);
			glob.shieldChargePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58F6A);
			glob.sneakRollPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x105F23);
			// Globals:
			// Not for Skyrim.
			glob.craftingPointsGlob = nullptr;
			glob.learningPointsGlob = nullptr;
			glob.memoryPointsGlob = nullptr;
			glob.playerLevelGlob = nullptr;
			glob.werewolfTransformationGlob = nullptr;
		}
		else
		{
			// None used -- either renamed or not compatible.
			// Also only added to companion players if P1 chooses
			// a compatible perk when meditating.
			glob.shoutVarSpellsFormList = nullptr;
			// Perks:
			glob.assassinsBladePerk = nullptr;
			glob.backstabPerk = nullptr;
			glob.criticalChargePerk = nullptr;
			glob.deadlyAimPerk = nullptr;
			glob.dualCastingAlterationPerk = nullptr;
			glob.dualCastingConjurationPerk = nullptr;
			glob.dualCastingDestructionPerk = nullptr;
			glob.dualCastingIllusionPerk = nullptr;
			glob.dualCastingRestorationPerk = nullptr;
			glob.greatCriticalChargePerk = nullptr;
			glob.powerBashPerk = nullptr;
			glob.quickShotPerk = nullptr;
			glob.shieldChargePerk = nullptr;
			glob.sneakRollPerk = nullptr;
			// Globals:
			glob.craftingPointsGlob = 
			(
				RE::TESForm::LookupByEditorID<RE::TESGlobal>("Handwerkspunkte"sv)
			);
			glob.learningPointsGlob = RE::TESForm::LookupByEditorID<RE::TESGlobal>("Lernpunkte"sv);
			glob.memoryPointsGlob = RE::TESForm::LookupByEditorID<RE::TESGlobal>("TalentPoints"sv);
			glob.playerLevelGlob = RE::TESForm::LookupByEditorID<RE::TESGlobal>("PlayerLevel"sv);
		}

		// Get all selectable level up perks.
		SELECTABLE_PERKS.clear();
		if (const auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
		{
			auto getSelectablePerks =
			[](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) 
			{
				auto perk = a_node->perk;
				while (perk)
				{
					SELECTABLE_PERKS.insert(perk);
					perk = perk->nextPerk;
				}
			};

			Util::TraverseAllPerks(p1, getSelectablePerks);
		}

		// Assign triggerable killmoves.
		AssignSkeletonSpecificKillmoves();
		AssignGenericKillmoves();

		// Set default XP-related game settings' values.
		SetDefaultXPBaseAndMultFromGameSettings();

		// Initialize managers, holders, and other global data members and wrap in smart pointers.
		glob.cam = std::make_unique<CameraManager>();
		glob.cdh = std::make_unique<ControllerDataHolder>();
		glob.mim = std::make_unique<MenuInputManager>();
		glob.moarm = std::make_unique<MenuOpeningActionRequestsManager>();
		glob.contactListener = std::make_unique<ContactListener>();
		glob.copyDataReqInfo = std::make_unique<CopyPlayerDataRequestInfo>();
		glob.coopCompanionExchangeableData = std::make_unique<ExchangeablePlayerData>();
		glob.p1ExchangeableData = std::make_unique<ExchangeablePlayerData>();
		glob.lastP1MeleeUseSkillCallArgs = std::make_unique<LastP1MeleeUseSkillCallArgs>();
		glob.paFuncsHolder = std::make_unique<PlayerActionFunctionsHolder>();
		glob.paInfoHolder = std::make_unique<PlayerActionInfoHolder>();
		glob.taskRunner	= std::make_unique<TaskRunner>();

		// Create inactive co-op players.
		std::generate
		(
			glob.coopPlayers.begin(), glob.coopPlayers.end(), 
			[]() 
			{
				return std::make_shared<CoopPlayer>();
			}
		);

		// Done initializing.
		glob.globalDataInit = true;
		SPDLOG_INFO("[GLOB] Global data initialiezd!");
	}

	//=============================================================================================

	void GlobalCoopData::AddSkillXP
	(
		const int32_t& a_cid, RE::ActorValue a_skillAV, const float& a_baseXP
	)
	{
		// Add skill XP for the co-op actor.
		// Source for leveling formulas: https://en.uesp.net/wiki/Skyrim:Leveling
		// 
		// XP to level up:
		// Skill Improve Mult * (level-1)^1.95 + Skill Improve Offset, Cost(0) = 0
		// Skill XP awarded:
		// Skill Use Mult * (base XP * skill specific multipliers) + Skill Use Offset
		// "skill specific multipliers" are not accounted for and left as 1.0.

		auto& glob = GetSingleton();

		// Don't add skill XP for P1.
		if (a_cid == glob.player1CID)
		{
			return;
		}

		// Enderal has no usage-based skill levelling.
		if (ALYSLC::EnderalCompat::g_enderalSSEInstalled ||
			a_cid <= -1 || a_cid >= ALYSLC_MAX_PLAYER_COUNT) 
		{
			return;
		}

		auto actorValueList = RE::ActorValueList::GetSingleton(); 
		if (!actorValueList)
		{
			return;
		}

		const auto& p = glob.coopPlayers[a_cid];
		// Do not add XP for shared skills here,
		// since all such skills are progressed via P1.
		if (!p->coopActor || glob.SHARED_SKILL_AVS_SET.contains(a_skillAV))
		{
			return;
		}

		float skillCurveExp = 1.95f;
		auto valueOpt = Util::GetGameSettingFloat("fSkillUseCurve");
		if (valueOpt.has_value())
		{
			skillCurveExp = valueOpt.value();
		}

		auto avInfo = actorValueList->actorValues[!a_skillAV];
		const auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (!p1 || !avInfo || !avInfo->skill)
		{
			return;
		}

		auto avSkillInfo = avInfo->skill;
		float xpInc = 
		(
			(Settings::vfSkillXPMult[p->playerID]) * 
			(avSkillInfo->useMult * a_baseXP + avSkillInfo->offsetMult)
		);

		SPDLOG_DEBUG("[GLOB] AddSkillXP: {}: Getting lock. (0x{:X})",
			p->coopActor->GetName(),
			std::hash<std::jthread::id>()(std::this_thread::get_id()));
		{
			std::unique_lock<std::mutex> skillXPLock(glob.skillXPMutexes[a_cid]);
			SPDLOG_DEBUG("[GLOB] AddSkillXP: {}: Lock obtained. (0x{:X})", 
				p->coopActor->GetName(),
				std::hash<std::jthread::id>()(std::this_thread::get_id()));
			
			const auto& skill = glob.AV_TO_SKILL_MAP.at(a_skillAV);
			glob.serializablePlayerData.at(p->coopActor->formID)->skillXPList.at(skill) += xpInc;
		}
	}

	void GlobalCoopData::AdjustAllPlayerPerkCounts()
	{
		// Adjust serialized used, available, extra, and shared perk counts for all players.

		SPDLOG_DEBUG("[GLOB] AdjustAllPlayerPerkCounts");

		auto& glob = GetSingleton();

		auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (!p1)
		{
			return;
		}

		// If this function is called before any LevelUp menus open,
		// we have to anticipate what level P1 will level up to after all LevelUp menus open.
		auto currentLevel = p1->GetLevel();
		uint32_t expectedLevelAfterLevelUp = currentLevel;
		// Default values.
		float fXPLevelUpMult = glob.defXPLevelUpMult;
		float fXPLevelUpBase = glob.defXPLevelUpBase;
		float thresholdAtLevel = Settings::fLevelUpXPThresholdMult *
		(
			fXPLevelUpBase + (fXPLevelUpMult * expectedLevelAfterLevelUp)
		);
		float remainingXP = p1->skills->data->xp;
		SPDLOG_DEBUG
		(
			"[GLOB] AdjustAllPlayerPerkCounts: Current level: {}, "
			"remaining XP: {}, threshold: {}. Base and mult: {}, {}.",
			currentLevel,
			remainingXP,
			thresholdAtLevel,
			fXPLevelUpBase,
			fXPLevelUpMult
		);
		// Increment the expected level until there is not enough remaining XP to advance a level.
		while (remainingXP >= thresholdAtLevel)
		{
			remainingXP -= thresholdAtLevel;
			expectedLevelAfterLevelUp++;
			// Set the level up XP threshold for the next level.
			thresholdAtLevel = Settings::fLevelUpXPThresholdMult *
			(
				fXPLevelUpBase + (fXPLevelUpMult * expectedLevelAfterLevelUp)
			);
			SPDLOG_DEBUG
			(
				"[GLOB] AdjustAllPlayerPerkCounts: Next level: {}, "
				"remaining XP: {}, new threshold: {}.",
				expectedLevelAfterLevelUp,
				remainingXP,
				thresholdAtLevel
			);
		}

		// Adjust perk counts (used, available, extra, shared) for each player.
		// Used perk points = 
		// this player's total unlocked perks - 
		// total unlocked shared perks - 
		// extra perk points +
		// this player's unlocked shared perks
		for (auto& [fid, data] : glob.serializablePlayerData)
		{
			const auto& unlockedPerksList = data->GetUnlockedPerksList();
			// Players start with 3 perk points at level 1 if using Requiem.
			// TODO: 
			// Additional/variable awarded perk points per level.
			uint32_t maxPerkPointsFromLevel = 
			/*(
				ALYSLC::RequiemCompat::g_requiemInstalled ?
				p1->GetLevel() + 2 : 
				p1->GetLevel() - 1
			);*/
			static_cast<uint32_t>
			(
				(
					ALYSLC::RequiemCompat::g_requiemInstalled ? 
					expectedLevelAfterLevelUp + 2 :
					expectedLevelAfterLevelUp - 1
				) * 
				Settings::fPerkPointsPerLevelUp + 
				Settings::uFlatPerkPointsIncrease
			);

			uint32_t totalUnlockedPerks = unlockedPerksList.size();
			RE::Actor* playerActor = nullptr;
			bool isP1 = fid == p1->formID;
			if (isP1)
			{
				playerActor = p1;
				SPDLOG_DEBUG
				(
					"[GLOB] AdjustAllPlayerPerkCounts: P1: CurrentXP: {}, "
					"current level: {}, post-levelups: {}, "
					"serialized number of unlocked perks: {}", 
					p1->skills->data->xp, 
					currentLevel, 
					expectedLevelAfterLevelUp,
					totalUnlockedPerks
				);

				// Ensure glob perk list matches the serialized one.
				for (auto i = 0; i < unlockedPerksList.size(); ++i)
				{
					auto perkToAdd = unlockedPerksList[i];
					bool alreadyAdded = std::any_of
					(
						p1->perks.begin(), p1->perks.end(),
						[p1, perkToAdd](RE::BGSPerk* a_perk) 
						{
							return a_perk == perkToAdd;
						}
					);

					if (!alreadyAdded)
					{
						Util::Player1AddPerk(perkToAdd, -1);
						SPDLOG_DEBUG
						(
							"[GLOB] AdjustAllPlayerPerkCounts. "
							"Re-adding {} to p1's perks list. New perk count: {}",
							perkToAdd->GetName(), p1->perks.size()
						);
					}
				}

				// Get total unlocked perks count from singleton list.
				totalUnlockedPerks = 0;
				for (auto perk : p1->perks)
				{
					if (glob.SELECTABLE_PERKS.contains(perk))
					{
						++totalUnlockedPerks;
					}
				}

				SPDLOG_DEBUG
				(
					"[GLOB] AdjustAllPlayerPerkCounts. "
					"Perk glob list gives total unlocked perks count of {}.", 
					totalUnlockedPerks
				);
			}
			else
			{
				// Get companion player from plugin.
				if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) 
				{
					playerActor = dataHandler->LookupForm<RE::Actor>
					(
						fid & 0x00FFFFFF, PLUGIN_NAME
					);
				}
			}

			if (playerActor)
			{
				const auto totalSharedPerksUnlocked = GetUnlockedSharedPerksCount();
				// Get unlocked shared perks count for P1 by adding
				// all shared perk counts for each non-P1 player and then
				// subtracting this total from the total number of shared perks.
				auto coopCompanionSharedPerksUnlocked = 0;
				for (auto& [fid, data] : glob.serializablePlayerData)
				{
					if (fid == p1->formID)
					{
						continue;
					}

					if (data->sharedPerksUnlocked <= totalSharedPerksUnlocked) 
					{
						coopCompanionSharedPerksUnlocked += data->sharedPerksUnlocked;
					}
					else
					{
						// Error: this player has unlocked more shared perks than the saved total.
						// Clamp to total shared perks.
						SPDLOG_DEBUG
						(
							"[GLOB] ERR: AdjustAllPlayerPerkCounts: "
							"Player with FID 0x{:X} has {} unlocked shared perks on record, "
							"resetting to {}.",
							fid, 
							data->sharedPerksUnlocked, 
							totalSharedPerksUnlocked
						);
						data->sharedPerksUnlocked = totalSharedPerksUnlocked;
					}
				}

				// Set number of shared perks unlocked for P1 here.
				// Co-op companions' shared perk counts are updated 
				// after P1's perk tree is restored on menu exit.
				if (isP1)
				{
					data->sharedPerksUnlocked = max
					(
						0, 
						static_cast<int32_t>
						(
							totalSharedPerksUnlocked - coopCompanionSharedPerksUnlocked
						)
					);
				}

				// Extra perk points are points in excess with respect to 
				// how many points the player should have received from leveling
				// and from the shared perk points count:
				// 
				// Extra perk points = 
				// total unlocked perk count -
				// number of shared perks NOT unlocked by this player - 
				// max unlocked perk count from leveling up
				// 
				// Ensure never below 0, which can happen if the unlocked perks total 
				// hasn't been updated to reflect the current perk count state.
				// (eg. Perks added via console command outside of co-op. Please don't do this.)
				int32_t extraPerkPoints = 
				(
					totalUnlockedPerks - 
					totalSharedPerksUnlocked + 
					data->sharedPerksUnlocked - 
					maxPerkPointsFromLevel
				); 
				if (extraPerkPoints >= 0)
				{
					data->extraPerkPoints = extraPerkPoints;
					SPDLOG_DEBUG
					(
						"[GLOB] AdjustAllPlayerPerkCounts: "
						"{} has {} extra perks from external sources.",
						playerActor->GetName(), extraPerkPoints
					);
				}
				else
				{
					SPDLOG_DEBUG
					(
						"[GLOB] AdjustAllPlayerPerkCounts: "
						"{} has {} extra perks from external sources. Resetting to 0.",
						playerActor->GetName(), extraPerkPoints
					);
					data->extraPerkPoints = 0;
				}

				// Handle decreases in unlocked perks count during co-op.
				int32_t perkCountDec = data->prevTotalUnlockedPerks - totalUnlockedPerks; 
				if (perkCountDec > 0)
				{
					data->extraPerkPoints = max
					(
						0, 
						static_cast<int32_t>
						(
							data->extraPerkPoints - perkCountDec
						)
					);
					SPDLOG_DEBUG
					(
						"[GLOB] AdjustAllPlayerPerkCounts: "
						"{} has {} extra perks after total perk count decrease "
						"of {} from {} to {}.",
						playerActor->GetName(),
						data->extraPerkPoints,
						perkCountDec, 
						data->prevTotalUnlockedPerks, 
						totalUnlockedPerks
					);
				}

				// Used perk points before clamp = 
				// total - extra - shared perks NOT unlocked by this player.
				int32_t rawUsedPerkPoints = 
				(
					totalUnlockedPerks - 
					data->extraPerkPoints - 
					totalSharedPerksUnlocked +
					data->sharedPerksUnlocked
				);
				// Clamp to [0, max total at level]
				data->usedPerkPoints = min(max(0, rawUsedPerkPoints), maxPerkPointsFromLevel);
				// Available = Max total for the current level - used total
				data->availablePerkPoints = max(0, maxPerkPointsFromLevel - data->usedPerkPoints);

				SPDLOG_DEBUG
				(
					"[GLOB] AdjustAllPlayerPerkCounts: "
					"{} has {}/{} unlocked perks, "
					"{} unlocked shared perks out of {} total unlocked ({} by co-op companions), "
					"max perk points from leveling: {}, "
					"extra perks: {}, for a total of {} used perk points. "
					"Result: {} available perk points. "
					"Expected level after level up, current: {}, {}.",
					playerActor->GetName(),
					unlockedPerksList.size(),
					totalUnlockedPerks,
					data->sharedPerksUnlocked,
					totalSharedPerksUnlocked,
					coopCompanionSharedPerksUnlocked,
					maxPerkPointsFromLevel,
					data->extraPerkPoints,
					data->usedPerkPoints,
					data->availablePerkPoints,
					expectedLevelAfterLevelUp,
					currentLevel
				);
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[GLOB] ERR: AdjustAllPlayerPerkCounts: "
					"Could not get player form for FID 0xXX{:X}", 
					fid & 0x00FFFFFF
				);
			}

			// Update previous unlocked perks count.
			data->prevTotalUnlockedPerks = data->GetUnlockedPerksList().size();
		}
	}

	void GlobalCoopData::AdjustBaseHMSData(RE::Actor* a_playerActor, const bool& a_shouldImport)
	{
		// Save the player's HMS base AVs on entering the Stats Menu 
		// and then record any increases to these values on exit.
		// Also update the last serialized player level if it differs from the cached one.

		auto& glob = GetSingleton();

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_playerActor) 
		{
			return;
		}

		auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
		if (a_shouldImport)
		{
			// Save base HMS actor values on menu entry.
			// Co-op player AVs are not imported to P1 yet.
			data->hmsBaseAVsOnMenuEntry[0] = 
			(
				a_playerActor->GetBaseActorValue(RE::ActorValue::kHealth)
			);
			data->hmsBaseAVsOnMenuEntry[1] = 
			(
				a_playerActor->GetBaseActorValue(RE::ActorValue::kMagicka)
			);
			data->hmsBaseAVsOnMenuEntry[2] = 
			(
				a_playerActor->GetBaseActorValue(RE::ActorValue::kStamina)
			);

			SPDLOG_DEBUG
			(
				"[GLOB] AdjustBaseHMSData: {}'s base HMS values saved as {}, {}, {} ON ENTRY. "
				"First saved level: {}.",
				a_playerActor->GetName(),
				a_playerActor->GetBaseActorValue(RE::ActorValue::kHealth),
				a_playerActor->GetBaseActorValue(RE::ActorValue::kMagicka),
				a_playerActor->GetBaseActorValue(RE::ActorValue::kStamina),
				data->firstSavedLevel
			);
		}
		else
		{
			// Set co-op HMS skill increases based on P1's HMS AV changes in the Stats Menu.
			// Done before restoring P1's HMS values later.
			data->hmsPointIncreasesList[0] += 
			(
				p1->GetBaseActorValue(RE::ActorValue::kHealth) - data->hmsBaseAVsOnMenuEntry[0]
			);
			data->hmsPointIncreasesList[1] += 
			(
				p1->GetBaseActorValue(RE::ActorValue::kMagicka) - data->hmsBaseAVsOnMenuEntry[1]
			);
			data->hmsPointIncreasesList[2] += 
			(
				p1->GetBaseActorValue(RE::ActorValue::kStamina) - data->hmsBaseAVsOnMenuEntry[2]
			);

			SPDLOG_DEBUG
			(
				"[GLOB] AdjustBaseHMSData: {}'s HMS AVs have increased by {}, {}, {} "
				"since initial leveling. {}, {}, {} since entering the Stats Menu.",
				a_playerActor->GetName(),
				data->hmsPointIncreasesList[0],
				data->hmsPointIncreasesList[1],
				data->hmsPointIncreasesList[2],
				p1->GetBaseActorValue(RE::ActorValue::kHealth) - data->hmsBaseAVsOnMenuEntry[0],
				p1->GetBaseActorValue(RE::ActorValue::kMagicka) - data->hmsBaseAVsOnMenuEntry[1],
				p1->GetBaseActorValue(RE::ActorValue::kStamina) - data->hmsBaseAVsOnMenuEntry[2]
			);
		}

		// Update serialized player level if it does not match the current one.
		if (const uint16_t currentLevel = a_playerActor->GetLevel(); currentLevel != data->level)
		{
			SPDLOG_DEBUG
			(
				"[GLOB] AdjustBaseHMSData: Levels do not match for {}: "
				"saved ({}) != current ({}). Updating now.",
				a_playerActor->GetName(), data->level, currentLevel
			);

			data->level = currentLevel;
		}
	}

	// Source for leveling formulas: https://en.uesp.net/wiki/Skyrim:Leveling
	bool GlobalCoopData::AdjustInitialPlayer1PerkPoints(RE::Actor* a_playerActor)
	{
		// Adjust P1's available perk points and trigger level up menus
		// as required to give the current player the number of perk points 
		// and level ups that they require.
		// Available perk points are modified with the P1 singleton's perk points member.
		// Available level ups (opens LevelUp Menu) are modified 
		// by lowering P1's level temporarily by the requisite number of level ups 
		// and keeping XP constant.

		auto& glob = GetSingleton();

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_playerActor)
		{
			return false;
		}

		RE::FormID fid = 
		{
			glob.serializablePlayerData.contains(a_playerActor->formID) ? 
			a_playerActor->formID : 
			0
		};

		if (!fid) 
		{
			SPDLOG_DEBUG
			(
				"[GLOB] ERR: AdjustInitialPlayer1PerkPoints: "
				"Could not get serialized player FID for {}.", 
				a_playerActor->GetName()
			);
			return false;
		}
		
		// If this function is called before any LevelUp menus open,
		// we have to anticipate what level P1 will level up to after all LevelUp menus open.
		auto currentLevel = p1->GetLevel();
		uint32_t expectedLevelAfterLevelUp = currentLevel;
		uint32_t expectedLevelUps = 0;
		// Default values.
		float fXPLevelUpMult = glob.defXPLevelUpMult;
		float fXPLevelUpBase = glob.defXPLevelUpBase;
		float thresholdAtLevel = Settings::fLevelUpXPThresholdMult *
		(
			fXPLevelUpBase + (fXPLevelUpMult * expectedLevelAfterLevelUp)
		);
		float remainingXP = p1->skills->data->xp;
		SPDLOG_DEBUG
		(
			"[GLOB] AdjustInitialPlayer1PerkPoints: Current level: {}, "
			"remaining XP: {}, threshold: {}. Base and mult: {}, {}.",
			currentLevel,
			remainingXP,
			thresholdAtLevel,
			fXPLevelUpBase,
			fXPLevelUpMult
		);
		// Increment the expected level until there is not enough remaining XP 
		// to advance a level.
		while (remainingXP >= thresholdAtLevel)
		{
			remainingXP -= thresholdAtLevel;
			expectedLevelAfterLevelUp++;
			// Set the level up XP threshold for the next level.
			thresholdAtLevel = Settings::fLevelUpXPThresholdMult *
			(
				fXPLevelUpBase + (fXPLevelUpMult * expectedLevelAfterLevelUp)
			);
			SPDLOG_DEBUG
			(
				"[GLOB] AdjustInitialPlayer1PerkPoints: Next level: {}, "
				"remaining XP: {}, new threshold: {}.",
				expectedLevelAfterLevelUp,
				remainingXP,
				thresholdAtLevel
			);
		}

		// Set the number of expected level ups due to XP overflowing the level up threshold.
		expectedLevelUps = expectedLevelAfterLevelUp - currentLevel;

		// Get HMS points increase per level up.
		uint32_t iAVDhmsLevelUp = 10;
		auto valueOpt = Util::GetGameSettingInt("iAVDhmsLevelUp");
		if (valueOpt.has_value())
		{
			iAVDhmsLevelUp = valueOpt.value();
		}

		const auto& data = glob.serializablePlayerData.at(fid);
		// NOTE: 
		// This method of checking how many level ups the player has received will not work
		// if other mods change the system by which player increase their HMS AVs.
		// Will also cause issues if modifying perks or HMS AVs while in the Stats Menu.
		// Get the number of level ups used by dividing the sum of HMS increases 
		// by the number of points granted per level up.
		uint32_t hmsLevelUpsCount = 0;
		hmsLevelUpsCount = 
		(
			std::accumulate
			(
				data->hmsPointIncreasesList.begin(), 
				data->hmsPointIncreasesList.end(), 
				hmsLevelUpsCount
			) / iAVDhmsLevelUp
		);
		int32_t availableHMSLevelUps = max(0.0f, a_playerActor->GetLevel() - 1 - hmsLevelUpsCount);
		
		SPDLOG_DEBUG
		(
			"[GLOB] AdjustInitialPlayer1PerkPoints: "
			"{}'s level up count from HMS increases so far: "
			"{} (({} + {} + {}) / {}), level ups still available: {}. "
			"Available perk points: {}. "
			"Perk points total from P1 singleton before modification: {}.",
			a_playerActor->GetName(),
			hmsLevelUpsCount,
			data->hmsPointIncreasesList[0],
			data->hmsPointIncreasesList[1],
			data->hmsPointIncreasesList[2],
			iAVDhmsLevelUp,
			availableHMSLevelUps,
			data->availablePerkPoints,
			p1->perkCount
		);

		uint16_t playerLevel = p1->GetLevel();
		// Artificially drop P1's level (and consequently all active players' levels) 
		// to open the desired number of LevelUp menus.
		uint16_t targetDipLevel = playerLevel - availableHMSLevelUps;
		bool dipP1Level = targetDipLevel != playerLevel;

		// Adjust P1's perk points count and dip P1's level as necessary
		// to provide this player with the opportunity to level up their HMS actor values.
		// Also, add perk points without showing the HMS level up message box.
		if (availableHMSLevelUps == 0)
		{
			// LevelUp menus will trigger and each one that triggers will add 1 to the perk
			// count total, so we must anticipate and factor out these extra perks
			// when figuring out how many perk points to set.
			p1->perkCount = data->availablePerkPoints - expectedLevelUps;

			SPDLOG_DEBUG
			(
				"[GLOB] AdjustInitialPlayer1PerkPoints: "
				"No available HMS level ups, but there are {} perk points available for use. "
				"{} is expected to level up from {} to {}, "
				"meaning an extra {} perk points must be subtracted from their total.", 
				data->availablePerkPoints,
				a_playerActor ? a_playerActor->GetName() : "P1",
				currentLevel,
				expectedLevelAfterLevelUp,
				expectedLevelUps
			);
		}
		else
		{
			SPDLOG_DEBUG
			(
				"[GLOB] AdjustInitialPlayer1PerkPoints: "
				"{} is attempting to access the level up menu, and has {} available perk points, "
				"with {} available HMS level ups and {} expected level ups. "
				"Adding {} perk points on top of the points given by the LevelUp dialogs.",
				a_playerActor->GetName(), 
				data->availablePerkPoints, 
				availableHMSLevelUps, 
				expectedLevelUps,
				data->availablePerkPoints - availableHMSLevelUps - expectedLevelUps
			);
			
			// Additional perk points to add on top of the ones granted with each LevelUp Menu,
			// whether triggered naturally by leveling or by dipping P1's level below.
			p1->perkCount = data->availablePerkPoints - availableHMSLevelUps - expectedLevelUps;
			if (!dipP1Level) 
			{
				SPDLOG_DEBUG("[GLOB] AdjustInitialPlayer1PerkPoints: No level dip needed.");
				return false;
			}

			// Lower p1's level by the number of required level ups 
			// and give P1 the necessary XP to open the LevelUp menu 
			// the desired number of times.
			const auto scriptFactory = 
			(
				RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
			);
			// Cannot dip level without script.
			const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
			if (!script)
			{
				SPDLOG_ERROR
				(
					"[GLOB] ERR: AdjustInitialPlayer1PerkPoints: No console command script to run."
				);
				return false;
			}

			float savedPlayerXP = p1->skills->data->xp;
			float defMult = glob.defXPLevelUpMult;
			float defBase = glob.defXPLevelUpBase;
			float stepLevel = targetDipLevel;
			float thresholdAtLevel = (defBase + (defMult * stepLevel));
			float xpInc = 0.0f;
			// Accumulate the required XP to return to the pre-dip level.
			while (stepLevel < playerLevel)
			{
				// Modify the level up threshold by the user-set threshold mult 
				// before adding up XP increments per level.
				thresholdAtLevel = 
				(
					Settings::fLevelUpXPThresholdMult * 
					(defBase + (defMult * stepLevel))
				);
				xpInc += thresholdAtLevel;
				++stepLevel;
			}

			// Set XP to the pre-dip level XP + the XP increment needed to 
			// return to the pre-dip level from the post-dip level.
			// Since the XP level is over each level threshold 
			// from the post-dip level to the pre-dip level - 1,
			// the LevelUp menu will open the desired number of times.
			p1->skills->data->xp = savedPlayerXP + xpInc;
			script->SetCommand("SetLevel " + std::to_string(targetDipLevel));
			script->CompileAndRun(p1);
			// Cleanup.
			delete script;

			SPDLOG_DEBUG
			(
				"[GLOB] AdjustInitialPlayer1PerkPoints: "
				"After dip: current XP, threshold: {}, {}, "
				"current level: {}, xpInc: {} from prev {}.",
				p1->skills->data->xp, 
				p1->skills->data->levelThreshold,
				p1->GetLevel(),
				xpInc, 
				savedPlayerXP
			);
		}

		return dipP1Level;
	}

	void GlobalCoopData::AdjustPerkDataForPlayer1(const bool& a_enteringMenu)
	{
		// Adjust P1's HMS AVs, perks, perk count, 
		// and skill AVs when entering/exiting the Stats Menu.

		auto& glob = GetSingleton();

		const auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || glob.serializablePlayerData.empty()) 
		{
			return;
		}

		SPDLOG_DEBUG("[GLOB] AdjustPerkDataForPlayer1: {} menu.",
			a_enteringMenu ? "Entering" : "Exiting");
		
		SaveUnlockedPerksForPlayer(p1);
		if (a_enteringMenu)
		{
			// Adjust perk counts before potentially copying data to P1.
			AdjustAllPlayerPerkCounts();
			AdjustBaseHMSData(p1, a_enteringMenu);

			// If P1's level was lowered to trigger LevelUp menus,
			// rescale co-op companions' AVs,
			// since the game will have auto-scaled them when P1's level changes.
			bool rescaleSkillAVsOnP1LevelDip = AdjustInitialPlayer1PerkPoints(p1);
			if (rescaleSkillAVsOnP1LevelDip)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] AdjustPerkDataForPlayer1: "
					"About to rescale all companions' AVs after dipping P1's level "
					"to spawn level up menus."
				);
				RescaleActivePlayerAVs();
			}
		}
		else
		{
			// Save HMS changes for P1.
			AdjustBaseHMSData(p1, a_enteringMenu);
			// Trigger auto-scaling first to update base actor values 
			// to what they were at the player's first saved level in co-op.
			TriggerAVAutoScaling(nullptr, true);
			// Rescale HMS and skill AVs up 
			// from the new base actor values for all active players.
			RescaleActivePlayerAVs();
			// Sync changes to shared perks on menu exit.
			SyncSharedPerks();
			// Lastly, adjust perk counts.
			AdjustAllPlayerPerkCounts();

			// REMOVE when done debugging.
			if (!glob.serializablePlayerData.empty() && 
				glob.serializablePlayerData.contains(p1->formID))
			{
				const auto& data = glob.serializablePlayerData.at(p1->formID);
				const auto& unlockedPerksSet = data->GetUnlockedPerksSet();
				SPDLOG_DEBUG
				(
					"[GLOB] AdjustPerkDataForPlayer1: P1 has unlocked {} perks "
					"and has {} remaining perk points for {} total perk points available "
					"(default max is {}).",
					unlockedPerksSet.size(),
					p1->perkCount,
					unlockedPerksSet.size() + p1->perkCount,
					static_cast<uint32_t>
					(
						(
							ALYSLC::RequiemCompat::g_requiemInstalled ? 
							p1->GetLevel() + 2 :
							p1->GetLevel() - 1
						) * 
						Settings::fPerkPointsPerLevelUp + 
						Settings::uFlatPerkPointsIncrease
					)
				);
			}
		}
	}

	void GlobalCoopData::AdjustPerkDataForCompanionPlayer
	(
		RE::Actor* a_playerActor, const bool& a_enteringMenu
	)
	{
		// Adjust companion player's HMS AVs, perks, perk count, 
		// and skill AVs when entering/exiting the Stats Menu.

		auto& glob = GetSingleton();

		const auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_playerActor || glob.serializablePlayerData.empty())
		{
			return;
		}

		SPDLOG_DEBUG("[GLOB] AdjustPerkDataForCompanionPlayer: {} menu.", 
			a_enteringMenu ? "Entering" : "Exiting");
		if (a_enteringMenu)
		{
			// Adjust perk counts before potentially copying data to P1.
			AdjustAllPlayerPerkCounts();
			// Trigger auto scaling first to update base actor values 
			// to their first-saved level equivalents.
			TriggerAVAutoScaling(nullptr, true);
			// Rescale from new base actor values before copying 
			// and checking base actor value data.
			RescaleActivePlayerAVs();
			// Sync changes to shared perks before copying over co-op player's perk tree.
			SyncSharedPerks();

			// Dip P1's level, as necessary, 
			// to open the required number of LevelUp menus.
			bool rescaleSkillAVsOnP1LevelDip = AdjustInitialPlayer1PerkPoints(a_playerActor);
			// Rescale player AVs back to their saved values if P1's level was dipped, 
			// since all co-op companions have their AVs auto-scaled
			// by the game when P1's level changes.
			// Copy the co-op companions AVs over to P1 
			// only after we've rescaled.
			if (rescaleSkillAVsOnP1LevelDip)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] AdjustPerkDataForCompanionPlayer: "
					"About to rescale all companions' AVs "
					"after dipping P1's level to spawn level up menus."
				);
				RescaleActivePlayerAVs();
			}

			// Copy perk tree, 
			// then name and race name, 
			// and then skill AVs.
			if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkTree))
			{
				SPDLOG_DEBUG("[GLOB] AdjustPerkDataForCompanionPlayer: Import perk tree.");
				CopyOverPerkTrees(a_playerActor, a_enteringMenu);
				glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kPerkTree);
			}

			if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
			{
				SPDLOG_DEBUG("[GLOB] AdjustPerkDataForCompanionPlayer: Import name.");
				CopyOverActorBaseData(a_playerActor, a_enteringMenu, true, false, false);
				glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kName);
			}

			if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kRaceName))
			{
				SPDLOG_DEBUG("[GLOB] AdjustPerkDataForCompanionPlayer: Import race name.");
				CopyOverActorBaseData(a_playerActor, a_enteringMenu, false, true, false);
				glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kRaceName);
			}

			if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
			{
				SPDLOG_DEBUG("[GLOB] AdjustPerkDataForCompanionPlayer: Import AVs.");
				CopyOverAVs
				(
					a_playerActor, a_enteringMenu, ALYSLC::RequiemCompat::g_requiemInstalled
				);
				glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kSkillsAndHMS);
			}

			// Save HMS AVs on menu entry.
			AdjustBaseHMSData(a_playerActor, a_enteringMenu);
		}
		else
		{
			// Save HMS AVs on exit.
			AdjustBaseHMSData(a_playerActor, a_enteringMenu);
			// Restore name and race name, 
			// then skill AVs, 
			// and then perk tree.
			if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
			{
				SPDLOG_DEBUG("[GLOB] AdjustPerkDataForCompanionPlayer: Restore name.");
				CopyOverActorBaseData(a_playerActor, a_enteringMenu, true, false, false);
				glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kName);
			}

			if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kRaceName))
			{
				SPDLOG_DEBUG("[GLOB] AdjustPerkDataForCompanionPlayer: Restore race name.");
				CopyOverActorBaseData(a_playerActor, a_enteringMenu, false, true, false);
				glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kRaceName);
			}

			if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
			{
				SPDLOG_DEBUG("[GLOB] AdjustPerkDataForCompanionPlayer: Restore AVs.");
				CopyOverAVs
				(
					a_playerActor, a_enteringMenu, ALYSLC::RequiemCompat::g_requiemInstalled
				);
				glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
			}

			if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkTree))
			{
				SPDLOG_DEBUG("[GLOB] AdjustPerkDataForCompanionPlayer: Restore perk tree.");
				CopyOverPerkTrees(a_playerActor, a_enteringMenu);
				glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kPerkTree);
			}

			// Trigger auto scaling first to update base actor values 
			// to their first-saved level equivalents.
			TriggerAVAutoScaling(nullptr, true);
			// Rescale HMS and skill AVs up 
			// from the new base actor values for all active players.
			RescaleActivePlayerAVs();
			// Sync changes to shared perks on menu exit.
			SyncSharedPerks();
			// Lastly, adjust perk counts.
			AdjustAllPlayerPerkCounts();
		}
	}

	void GlobalCoopData::AssignGenericKillmoves()
	{
		// Assign generic killmoves which are linked 
		// to the 'character' skeleton type,
		// and further categorized by weapon type.

		auto& glob = GetSingleton();

		glob.genericKillmoveIdles = 
		(
			std::vector<std::vector<RE::TESIdleForm*>>
			(
				!KillmoveType::kTotal, 
				std::vector<RE::TESIdleForm*>()
			)
		);

		// H2H
		glob.genericKillmoveIdles[!KillmoveType::kH2H] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("H2HKillMoveBodySlam"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("H2HKillMoveComboA"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("H2HKillMoveKneeThrow"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("H2HKillMoveSlamA"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveH2HSuplex"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HNeckBreak"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HSleeper"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_KillMoveDLC02RipHeartOut"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_KillMoveH2HComboA")
		};

		// 1H
		glob.genericKillmoveIdles[!KillmoveType::k1H] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveA_NoShield"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveB"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveC"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveD"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveE"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveF"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveG"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveGrappleStab"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveH"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveI"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveJ"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveK"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveL"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveM"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveRepeatStabDowns"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveStabDownChest"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveStabUpFace"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveBackStab"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneak1HMThroatSlit"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HNeckBreak"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HSleeper"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveBackStab"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveBleedOutKill"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDecapBleedOut"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveShortA"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveShortB"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveShortC"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveShortD"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveShortJ")
		};

		// 2H
		glob.genericKillmoveIdles[!KillmoveType::k2H] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HM3Slash"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HMStabFromBehind"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HMUnderSwingLeg"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HNeckBreak"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HSleeper"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveDecapBleedOut")
		};

		// DW
		glob.genericKillmoveIdles[!KillmoveType::kDW] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveDualWield"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveDualWieldBleedOutDecap"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveDualWieldDecap"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveDualWieldDualSlash"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveDualWieldXSlash"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDualWieldA"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_KillMoveDualWieldDecap")
		};

		// 1H Axe
		glob.genericKillmoveIdles[!KillmoveType::k1HAxe] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveDecapSlashAxe"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveShortAxeMace"),
		};

		// 1H Dagger
		glob.genericKillmoveIdles[!KillmoveType::k1HDagger] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveShortBlade"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDecapKnife")
		};

		// 1H Mace
		glob.genericKillmoveIdles[!KillmoveType::k1HMace] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveShortAxeMace"),
		};

		// 1H Sword
		glob.genericKillmoveIdles[!KillmoveType::k1HSword] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveDecapSlash"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveShortBlade"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDecapSlash")
		};

		// 2H Axe
		glob.genericKillmoveIdles[!KillmoveType::k2HAxe] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HWB"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HWChopKick"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HWDecap"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HWDecapBleedOut"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HWHackFromBehind"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HWHeadButt"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveDecapA")
		};

		// 2H Sword
		glob.genericKillmoveIdles[!KillmoveType::k2HSword] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HMDecapBleedOut"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HMDecapSlash"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HMSlash"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HMStab"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveDecapSlash"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveStabA")
		};

		// Shield
		glob.genericKillmoveIdles[!KillmoveType::kShield] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveShieldBashAttack"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveShieldBashSlash")
		};

		// Sneak H2H
		glob.genericKillmoveIdles[!KillmoveType::kSneakH2H] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2H"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HNeckBreak"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HSleeper")
		};

		// Sneak 1H
		glob.genericKillmoveIdles[!KillmoveType::kSneak1H] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveBackStab"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneak1HMThroatSlit"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HNeckBreak"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HSleeper"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveBackStab")
		};

		// Sneak 2H
		glob.genericKillmoveIdles[!KillmoveType::kSneak2H] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HMStabFromBehind"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMove2HWHackFromBehind"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HNeckBreak"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveSneakH2HSleeper")
		};

		// General
		glob.genericKillmoveIdles[!KillmoveType::kGeneral] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveShortA"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveShortB"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveShortC"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveShortD"),
		};

		// Vampire Lord
		glob.genericKillmoveIdles[!KillmoveType::kVampireLord] = 
		{

			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("VampireLordRightPowerAttackFeedFront"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("VampireLordRightPowerAttackFeedBack")
		};

		// Werewolf
		glob.genericKillmoveIdles[!KillmoveType::kWerewolf] = 
		{
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("WerewolfPairedHeadSmash"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("WerewolfPairedHeadThrow"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("WerewolfPairedFeedingWithHuman"),
			RE::TESForm::LookupByEditorID<RE::TESIdleForm>("WerewolfPairedMaulingWithHuman"),
		};
	}

	void GlobalCoopData::AssignSkeletonSpecificKillmoves()
	{
		// Assign killmoves based on skeleton type, and further categorize by weapon type.

		auto& glob = GetSingleton();

		// Traversing all forms.
		// Inefficient, I know, but necessary.
		const auto& allForms = RE::TESForm::GetAllForms();
		if (allForms.first)
		{
			auto comp = [](const std::string& a_left, const std::string& a_right) 
			{ 
				std::string left = a_left.c_str();
				Util::ToLowercase(left);
				std::string right = a_right.c_str();
				Util::ToLowercase(right);
				return strcmp(left.c_str(), right.c_str()) < 0; 
			};
			std::set<std::string, decltype(comp)> skeleNames;

			{
				allForms.second.get().LockForRead();
				// Construct list of skeleton names by going through all races.
				std::for_each
				(
					allForms.first->begin(), allForms.first->end(),
					[&glob, &skeleNames](const auto& a_formPair) 
					{
						if (!a_formPair.second || a_formPair.second->IsNot(RE::FormType::Race))
						{
							return;
						}

						const auto asRace = a_formPair.second->As<RE::TESRace>();
						std::string skeleName{ };
						Util::GetSkeletonModelNameForRace(asRace, skeleName);
						if (!skeleName.empty()) 
						{
							skeleNames.insert(skeleName);
						}
					}
				);

				allForms.second.get().UnlockForRead();
			}

			// Assign killmoves based on skeleton name and weapon type.
			uint32_t hash = 0;
			for (const auto& skeleName : skeleNames) 
			{
				hash = Hash(skeleName);
				glob.skeletonKillmoveIdlesMap.emplace
				(
					hash, 
					std::vector<std::vector<RE::TESIdleForm*>>
					(
						!KillmoveType::kTotal, std::vector<RE::TESIdleForm*>()
					)
				);

				// Assign weapon-specific killmoves now for each skeleton type.
				auto& entry = glob.skeletonKillmoveIdlesMap.at(hash);
				switch (hash)
				{
				case ("bear"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveBear"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveBearA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveBearB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveBearA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveBearB"),
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveBear"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveBearA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveBearB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWKillMoveBear"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveBearA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveBearB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveBearA"),
					};

					break;
				}
				case ("canine"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMWolfKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMWolfKillMoveB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveWolfA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveWolfB")
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMWolfKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMWolfKillMoveB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWWolfKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveWolfA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveWolfB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveWolfA"),
					};

					break;
				}
				case ("chaurusflyer"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveChaurusFlyer"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"1HMKillMoveChaurusFlyerKick"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"1HMKillMoveChaurusFlyerStomp"
						)
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveChaurusFlyer"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWKillMoveChaurusFlyer")
					};

					entry[!KillmoveType::kGeneral] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveChaurusFlyer") 
					};

					break;
				}
				case ("dragon"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"1HMSword_KillMoveDragonRodeoSlash"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"1HMSword_KillMoveDragonRodeoStabShort"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_1HMKillMoveDragonRodeoSlash"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_1HMKillMoveDragonRodeoStabShort"
						)
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMDragonKillMoveSlash"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"2HWDragonKillMoveRodeoSlash"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWDragonKillMoveSlash"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_2HMKillMoveDragonSlash"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_2HWKillMoveDragonRodeoSlash"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveDragonSlash")

					};

					entry[!KillmoveType::kGeneral] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"KillMoveDragonBiteGrapple"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_KillMoveDragonBiteGrapple"
						)
					};

					break;
				}
				case ("draugr"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveDraugr"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveDraugrB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveDraugrShortA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveDraugrShortB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDraugrA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDraugrB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_1HMKillMoveDraugrShortA"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_1HMKillMoveDraugrShortB"
						)
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HM_KillMoveDraugrA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HW_KillMoveDraugrA")
					};

					break;
				}
				case ("dwarvensteamcenturion"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"1HMKillMoveSteamCenturionA"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"1HMKillMoveSteamCenturionB"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_1HMKillMoveSteamCenturionA"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_1HMKillMoveSteamCenturionB"
						)
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"2HMSteamCenturionKillMoveA"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"2HWSteamCenturionKillMoveA"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_2HMKillMoveSteamCenturionA"
						)
					};

					break;
				}
				case ("falmer"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveFalmer"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveFalmerA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveFalmerB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveFalmerC"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveFalmerA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveFalmerB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveFalmerC")
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveFalmer"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveFalmerA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveFalmerB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWKillMoveFalmer"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveFalmerA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveFalmerB")
					};

					break;
				}
				case ("frostbitespider"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSpiderSmashA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSpiderStabA")
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMSpiderKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWSpiderKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_2HMKillMoveSpiderSlamA"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveSpiderSlamA")
					};

					break;
				}
				case ("giant"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMGiantKillMove"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMGiantKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMGiantKillMoveB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"1HMGiantKillMoveBleedOutA"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveGiantA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveGiantB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_1HMKillMoveGiantBleedOutA"
						)
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMGiantKillMove"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWGiantKillMove"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveGiantA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveGiantA"),
					};

					break;
				}
				case ("hagraven"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveHagravenA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveHagravenB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveHagravenA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveHagravenB")
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMHagravennKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWHagravenKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveHagravenA")
					};

					break;
				}
				case ("sabrecat"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"1HMKillMoveSabreCatShortA"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"1HMKillMoveSabreCatShortB"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveSabreCat"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HmKillMoveSabreCat"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSabreCat"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_1HMKillMoveSabreCatShortA"
						),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>
						(
							"pa_1HMKillMoveSabreCatShortB"
						)
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWSabreCatKillMove"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HmKillMoveSabreCatA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HmKillMoveSabreCatB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveSabreCatA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveSabreCatB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveSabreCat"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveSabreCatA")
					};

					break;
				}
				case ("spriggan"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSprigganA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSprigganB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSprigganA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSprigganB")
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMSprigganKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWSprigganKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveSprigganA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveSprigganA")
					};

					break;
				}
				case ("troll"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMTrollKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMTrollKillMoveB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveTroll"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveTrollA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveTrollB")
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMTrollKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMTrollKillMoveB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWTrollKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveTrollA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveTrollB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveTrollA")
					};

					break;
				}
				case ("vampirebrute"_h):
				{
					entry[!KillmoveType::k1H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveVampireBrute")
					};

					entry[!KillmoveType::k2H] = 
					{
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveVampireBrute"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWKillMoveVampireBrute")
					};

					entry[!KillmoveType::kGeneral] = 
					{ 
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveVampireBrute") 
					};

					break;
				}
				default:
				{
					break;
				}
				}
			}
		}
	}

	bool GlobalCoopData::CanControlMenus(const int32_t& a_controllerID)
	{
		// Return true if the current player controlling menus 
		// has the same CID as the given one,
		// or if no player is currently controlling menus.

		auto& glob = GetSingleton();

		if (a_controllerID <= -1 || a_controllerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return false;
		}

		const auto& p = glob.coopPlayers[a_controllerID];
		if (!p->isActive) 
		{
			return false;
		}

		return glob.menuCID == a_controllerID || glob.menuCID == -1;;
	}

	void GlobalCoopData::EnableRagdollToActorCollisions()
	{
		// Enable collisions among the biped, biped no char controller,
		// dead biped, and char controller layers for all actors in the current cell.
		// Allows the havok contact listener to respond to collisions between ragdolling bodies.
		// NOTE:
		// Currently haven't figured out how to enable ragdoll-to-P1 collisions,
		// so all ragdolling actors pass through P1 without colliding.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !p1->parentCell)
		{
			return;
		}

		auto bhkWorld = p1->parentCell->GetbhkWorld();
		if (!bhkWorld)
		{
			return;
		}
	
		auto hkpWorld = bhkWorld->GetWorld1();
		if (!hkpWorld)
		{
			hkpWorld = bhkWorld->GetWorld2();
			if (!hkpWorld)
			{
				return;
			}
		}

		auto filterInfo = (RE::bhkCollisionFilter*)hkpWorld->collisionFilter; 
		if (!filterInfo)
		{
			return;
		}

		filterInfo->layerBitfields[!RE::COL_LAYER::kCharController] |= 
		(
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
			)
		);
		filterInfo->layerBitfields[!RE::COL_LAYER::kBiped] |= 
		(
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
			)
		);
		filterInfo->layerBitfields[!RE::COL_LAYER::kBipedNoCC] |= 
		(
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
			)
		);
		filterInfo->layerBitfields[!RE::COL_LAYER::kDeadBip] |= 
		(
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
			) |
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
			)
		);
	}

	int8_t GlobalCoopData::GetCoopPlayerIndex(const RE::ActorPtr& a_actorPtr)
	{
		// Given the actor smart ptr, 
		// get the corresponding player index in co-op players array 
		// (equivalent to the player's CID).
		// -1 if there is no corresponding index.

		if (!a_actorPtr || !a_actorPtr.get())
		{
			return -1;
		}

		return GetCoopPlayerIndex(a_actorPtr.get());
	}

	int8_t GlobalCoopData::GetCoopPlayerIndex(RE::TESObjectREFR* a_refr)
	{
		// Given the object refr, 
		// get the corresponding player index in co-op players array
		// (equivalent to the player's CID).
		// -1 if there is no corresponding index.

		if (!a_refr)
		{
			return -1;
		}

		auto& glob = GetSingleton();

		auto foundIter = std::find_if
		(
			glob.coopPlayers.begin(), glob.coopPlayers.end(),
			[a_refr](const auto& a_p) 
			{
				return a_p->isActive && a_p->coopActor.get() == a_refr; 
			}
		);
		if (foundIter != glob.coopPlayers.end()) 
		{
			return std::distance(glob.coopPlayers.begin(), foundIter);
		}

		return -1;
	}

	int8_t GlobalCoopData::GetCoopPlayerIndex(const RE::TESObjectREFRPtr& a_refrPtr)
	{
		// Given the object refr smart ptr,
		// get the corresponding player index in co-op players array
		// (equivalent to the player's CID).
		// -1 if there is no corresponding index.

		if (!a_refrPtr || !a_refrPtr.get())
		{
			return -1;
		}

		return GetCoopPlayerIndex(a_refrPtr.get());
	}

	int8_t GlobalCoopData::GetCoopPlayerIndex(const RE::FormID& a_formID)
	{
		// Given the FID, 
		// get the corresponding player index in co-op players array
		// (equivalent to the player's CID).
		// -1 if there is no corresponding index.

		auto& glob = GetSingleton();

		auto foundIter = std::find_if
		(
			glob.coopPlayers.begin(), glob.coopPlayers.end(),
			[a_formID](const auto& a_p) 
			{ 
				return a_p->isActive && a_p->coopActor->formID == a_formID; 
			}
		);
		if (foundIter != glob.coopPlayers.end())
		{
			return std::distance(glob.coopPlayers.begin(), foundIter);
		}

		return -1;
	}

	int8_t GlobalCoopData::GetCoopPlayerIndex(const RE::ObjectRefHandle& a_refrHandle)
	{
		// Given the refr handle, 
		// get the corresponding player index in co-op players array
		// (equivalent to the player's CID).
		// -1 if there is no corresponding index.

		auto& glob = GetSingleton();

		if (!a_refrHandle || !a_refrHandle.get() || !a_refrHandle.get()->IsHandleValid())
		{
			return -1;
		}

		auto foundIter = std::find_if
		(
			glob.coopPlayers.begin(), glob.coopPlayers.end(),
			[a_refrHandle](const auto& a_p) 
			{ 
				return 
				(
					a_p->isActive && 
					a_p->coopActor->GetHandle() == a_refrHandle
				); 
			}
		);
		if (foundIter == glob.coopPlayers.end())
		{
			return -1;
		}

		return std::distance(glob.coopPlayers.begin(), foundIter);
	}

	float GlobalCoopData::GetHighestSharedAVLevel(const RE::ActorValue& a_av)
	{
		// Get highest AV level among all players,
		// active or inactive, for the shared AV.
		// -1 indicates that the AV should not be modified.
		// 
		// NOTE:
		// If we were to consider only active players, 
		// the highest skill level might decrease
		// if summoning a different set of players, 
		// which would mean that the party might not 
		// reach certain unlocked shared perks' required AV levels.
		// For example: 
		// Party 1: P1 and P2: Highest Lockpicking Level: 25 (P2), 
		// first Lockpicking perk 'Novice Locks' (required level 20) is unlocked, 
		// set all player Lockpicking levels to 25.
		// Party 2: P1 and P3: Highest Lockpicking Level: 17 (P1), 
		// set all player Lockpicking levels to 17.
		// 'Novice Locks' remains unlocked but its minimum level is not reached (17 < 25).
		// If P2 (now inactive) was also considered instead, 
		// the highest level would stay at 25,
		// and 'Novice Locks' level requirement would still be met.

		auto& glob = GetSingleton();

		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler)
		{
			return -1.0f;
		}

		RE::Actor* playerActor = nullptr;
		float highestAVAmount = -1.0f;
		const auto skill = 
		(
			AV_TO_SKILL_MAP.contains(a_av) ? 
			AV_TO_SKILL_MAP.at(a_av) : 
			Skill::kTotal
		);
		// Index should not equal the length of the skills lists.
		if (skill == Skill::kTotal)
		{
			return -1.0f;
		}

		for (auto& [fid, data] : glob.serializablePlayerData)
		{
			// P1 FID is always 0x14.
			if (fid == 0x14)
			{
				playerActor = RE::PlayerCharacter::GetSingleton();
			}
			else
			{
				playerActor = dataHandler->LookupForm<RE::Actor>(fid & 0x00FFFFFF, PLUGIN_NAME);
			}

			if (!playerActor)
			{
				continue;
			}

			// Base + inc is higher than the previous highest level.
			float value = data->skillBaseLevelsList[skill] + data->skillLevelIncreasesList[skill]; 
			if (value > highestAVAmount)
			{
				highestAVAmount = value;
			}
		}

		SPDLOG_DEBUG("[GLOB] GetHighestSharedAVLevel: {} -> {}.",
			Util::GetActorValueName(a_av), highestAVAmount);
		return highestAVAmount;
	}

	uint32_t GlobalCoopData::GetUnlockedSharedPerksCount()
	{
		// Get the total number of unlocked shared perks.
		// Precondition: 
		// All players have the same set of shared perks 
		// before calling this func.

		auto& glob = GetSingleton();

		const auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1) 
		{
			return 0;
		}

		// Same perk shows up multiple times in some trees. 
		// Do not count the same perk multiple times.
		std::set<RE::BGSPerk*> perksSet;
		auto getSharedPerksCount = 
		[p1, &glob, &perksSet](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) 
		{
			auto perk = a_node->perk; 
			if (!perk)
			{
				return;
			}

			bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
			if (!shared)
			{
				return;
			}

			while (perk)
			{
				// Not always in sync, so if either perk list 
				// says P1 has the perk, add it.
				bool nativeHasPerk = p1->HasPerk(perk);
				bool singletonListHasPerk = Util::Player1PerkListHasPerk(perk);
				if (nativeHasPerk || singletonListHasPerk)
				{
					SPDLOG_DEBUG
					(
						"[GLOB] GetUnlockedSharedPerksCount: Shared perk {} (0x{:X}): {}, {}",
						perk->GetName(), perk->formID, nativeHasPerk, singletonListHasPerk
					);
					perksSet.insert(perk);
				}

				perk = perk->nextPerk;
			}
		};

		// Each player will have the same shared perks, 
		// so simply check P1 for shared perks.
		Util::TraverseAllPerks(p1, getSharedPerksCount);
		SPDLOG_DEBUG("[GLOB] GetUnlockedSharedPerksCount: Total: {}", perksSet.size());
		return perksSet.size();
	}

	void GlobalCoopData::GivePartyWideItemsToP1()
	{
		// Transfer gold, lockpicks, keys, notes, 
		// and non-skill/level granting books to P1.
		// Gold and lockpicks are shared, 
		// since P1 effectively triggers the Lockpicking and Barter Menus,
		// even if another player is controlling these menus. 
		// Less prone to error if these common items
		// are kept on P1 where they can be used by all active players.
		//
		// Likewise, if a key, note, or book is required to open a door,
		// progress a quest, or trigger an event,
		// it must be on P1's person at the time of activation, 
		// so having these items always in P1's inventory
		// is less of a hassle when trying to find a specific item.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return;
		}

		auto& glob = GetSingleton();

		for (const auto& p : glob.coopPlayers) 
		{
			if (!p->isActive || p->isPlayer1)
			{
				continue;
			}

			auto inventory = p->coopActor->GetInventory();
			for (auto& [boundObj, entry] : inventory)
			{
				if (!Util::IsPartyWideItem(boundObj))
				{
					continue;
				}

				if (entry.first <= 0) 
				{
					continue;
				}

				// Transfer to P1.
				p->coopActor->RemoveItem
				(
					boundObj,
					entry.first,
					RE::ITEM_REMOVE_REASON::kStoreInTeammate, 
					nullptr, 
					p1
				);
			}

			auto invChanges = p1->GetInventoryChanges(); 
			if (!invChanges || !invChanges->entryList)
			{
				continue;
			}

			auto invCounts = p1->GetInventoryCounts();
			for (auto invChangesEntry : *invChanges->entryList)
			{
				if (!invChangesEntry || !invChangesEntry->object)
				{
					continue;
				}

				auto boundObj = invChangesEntry->object;
				if (!boundObj)
				{
					continue;
				}

				if (!Util::IsPartyWideItem(boundObj))
				{
					continue;
				}
				
				auto count = 
				(
					invCounts.contains(boundObj) ? 
					invCounts.at(boundObj) : 
					0
				);
				if (count <= 0)
				{
					continue;
				}

				// Transfer to P1.
				p->coopActor->RemoveItem
				(
					boundObj, 
					count, 
					RE::ITEM_REMOVE_REASON::kStoreInTeammate, 
					nullptr, 
					p1
				);
			}
		}
	}

	void GlobalCoopData::HandlePlayerArmCollisions()
	{
		// Check for arm collisions for each player that is rotating their arms,
		// and handle any impacts (impulses, knockdowns, damage) with other actors.

		auto& glob = GetSingleton();

		if (!glob.coopSessionActive || !Settings::bEnableArmsRotation)
		{
			return;
		}

		for (const auto& p : glob.coopPlayers)
		{
			if ((!p->isActive) || (p->isPlayer1 && !glob.cam->IsRunning()))
			{
				continue;
			}

			bool isRotatingShoulders = p->pam->IsPerformingOneOf
			(
				InputAction::kRotateLeftShoulder, 
				InputAction::kRotateRightShoulder
			);
			bool isRotatingForearmsOrHands = 
			{
				p->pam->IsPerformingOneOf
				(
					InputAction::kRotateLeftForearm,
					InputAction::kRotateRightForearm,
					InputAction::kRotateLeftHand,
					InputAction::kRotateRightHand
				)
			};
			if (isRotatingShoulders || isRotatingForearmsOrHands)
			{
				p->mm->nom->CheckAndPerformArmCollisions(p);
			}
		}
	}

	void GlobalCoopData::HandleEnderalProgressionChanges()
	{
		// P1 level up:
		// Rescale all active companions' AVs.
		// Check for increments to crafting/learning/memory points 
		// and multiply these changes by the party-size as necessary.

		auto& glob = GetSingleton();

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !glob.globalDataInit || glob.serializablePlayerData.empty()) 
		{
			return;
		}

		if (glob.serializablePlayerData.contains(p1->formID)) 
		{
			const auto& p1Data = glob.serializablePlayerData.at(p1->formID);
			// Using the Enderal player level global 
			// which is unaffected by the SetLevel console command.
			// Want to ignore false level ups triggered from our AV auto-scaling.
			if (glob.playerLevelGlob) 
			{
				// Only rescale if P1 leveled up during a co-op session.
				if (glob.coopSessionActive && p1Data->level < glob.playerLevelGlob->value)
				{
					TriggerAVAutoScaling(nullptr, true);
					RescaleActivePlayerAVs();

					// Send message box menu control request for P1 
					// to gain control of the Enderal level up menu 
					// that opens post-levelup.
					glob.moarm->InsertRequest
					(
						glob.player1CID, 
						InputAction::kActivate, 
						SteadyClock::now(), 
						RE::MessageBoxMenu::MENU_NAME
					);
				}

				// Update level afterward.
				p1Data->level = glob.playerLevelGlob->value;
			}

			// Crafting points increase.
			if (glob.craftingPointsGlob)
			{
				// Only scale if earned during a co-op session.
				if (glob.coopSessionActive && 
					Settings::bScaleCraftingPointsWithNumPlayers &&
					glob.craftingPointsGlob->value > glob.savedCraftingPoints)
				{
					float newCraftingPointsDelta = 
					(
						(glob.craftingPointsGlob->value - glob.savedCraftingPoints) * 
						(float)glob.activePlayers
					);
					glob.craftingPointsGlob->value = 
					(
						glob.savedCraftingPoints + newCraftingPointsDelta
					);
					RE::DebugMessageBox
					(
						fmt::format
						(
							"Gained {} Crafting Point(s) after party scaling.\nNew total: {}",
							newCraftingPointsDelta, glob.craftingPointsGlob->value
						).c_str()
					);
				}

				glob.savedCraftingPoints = glob.craftingPointsGlob->value;
			}

			// Learning points increase.
			if (glob.learningPointsGlob)
			{
				// Only scale if earned during a co-op session.
				if (glob.coopSessionActive && 
					Settings::bScaleLearningPointsWithNumPlayers &&
					glob.learningPointsGlob->value > glob.savedLearningPoints)
				{
					float newLearningPointsDelta = 
					(
						(glob.learningPointsGlob->value - glob.savedLearningPoints) * 
						(float)glob.activePlayers
					);
					glob.learningPointsGlob->value = 
					(
						glob.savedLearningPoints + newLearningPointsDelta
					);
					RE::DebugMessageBox
					(
						fmt::format
						(
							"Gained {} Learning Point(s) after party scaling.\nNew total: {}",
							newLearningPointsDelta, glob.learningPointsGlob->value
						).c_str()
					);
				}

				glob.savedLearningPoints = glob.learningPointsGlob->value;
			}

			// Memory points increase.
			if (glob.memoryPointsGlob)
			{
				// Only scale if earned during a co-op session.
				if (glob.coopSessionActive && 
					Settings::bScaleMemoryPointsWithNumPlayers &&
					glob.memoryPointsGlob->value > glob.savedMemoryPoints)
				{
					float newMemoryPointsDelta = 
					(
						(glob.memoryPointsGlob->value - glob.savedMemoryPoints) *
						(float)glob.activePlayers
					);
					glob.memoryPointsGlob->value = glob.savedMemoryPoints + newMemoryPointsDelta;
					RE::DebugMessageBox
					(
						fmt::format
						(
							"Gained {} Memory Point(s) after party scaling.\nNew total: {}",
							newMemoryPointsDelta, glob.memoryPointsGlob->value
						).c_str()
					);
				}

				glob.savedMemoryPoints = glob.memoryPointsGlob->value;
				// Memory points count are also stored in 'DragonSouls' AV.
				p1->SetActorValue(RE::ActorValue::kDragonSouls, glob.savedMemoryPoints);
			}

			// Werewolf transformation.
			// Global variable keeps track of any P1 transformations.
			if (glob.coopSessionActive && glob.player1CID != -1) 
			{
				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
				if (!coopP1->isTransformed && 
					glob.werewolfTransformationGlob->value == 1.0f) 
				{
					for (auto effect : *coopP1->coopActor->GetActiveEffectList())
					{
						if ((effect->GetBaseObject()->formID & 0x00FFFFFF) == 0x29BA4)
						{
							coopP1->secsMaxTransformationTime = effect->duration;
						}
					}

					coopP1->transformationTP = SteadyClock::now();
					coopP1->isTransformed = true;
				}
				else if (glob.coopPlayers[glob.player1CID]->isTransformed &&
						 glob.werewolfTransformationGlob->value == 0.0f)
				{
					coopP1->isTransformed = false;
				}
			}
		}

		// TODO: Attempt to find an event-driven method 
		// or hook to check for AV changes, instead of every second.
		// Restore skill AVs since the game will make changes to them 
		// after our rescaling on level up.
		// I haven't found a place to hook yet to listen for skill AV changes.
		if (Util::GetElapsedSeconds(glob.lastCoopCompanionSkillLevelsCheckTP) > 1.0f) 
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive || 
					p->isPlayer1 || 
					!glob.serializablePlayerData.contains(p->coopActor->formID))
				{
					continue;
				}

				auto& data = glob.serializablePlayerData.at(p->coopActor->formID);
				for (uint8_t i = 0; i < SKILL_ACTOR_VALUES_LIST.size(); ++i)
				{
					const auto& av = SKILL_ACTOR_VALUES_LIST[i];
					float currentValue = p->coopActor->GetBaseActorValue(av);
					float newValue = 
					(
						data->skillBaseLevelsList[i] + data->skillLevelIncreasesList[i]
					);
					if (currentValue == newValue)
					{
						continue;
					}

					SPDLOG_DEBUG
					(
						"[GLOB] HandleEnderalProgressionChanges: {}: "
						"skill AV {} was set to {} when it should be set to {} ({} + {}).",
						p->coopActor->GetName(), 
						Util::GetActorValueName(av), 
						currentValue, 
						newValue, 
						data->skillBaseLevelsList[i], data->skillLevelIncreasesList[i]
					);
					p->coopActor->SetBaseActorValue(av, newValue);
				}
			}

			glob.lastCoopCompanionSkillLevelsCheckTP = SteadyClock::now();
		}
	}

	void GlobalCoopData::HavokPrePhysicsStep(RE::bhkWorld* a_world)
	{
		// Cache player arm and torso node rotations to restore later.
		// which will overwrite the game's changes to all handled player arm/torso nodes.
		// NOTE: 
		// We do not set our computed custom rotations here
		// because they will get overwritten sometime between when this callback is executed 
		// and when the NiNode UpwardDownwardPass() hook executes.
		// When this hook is run, reported node local rotations have been reset by the game,
		// so the previously set custom rotations from the last frame are no longer in effect.
		// However, we can save the game's default rotations to use when blending in/out.
		// 
		// NOTE: Not run every frame if the game's framerate is above 60.

		auto& glob = GetSingleton();
		if (!glob.coopSessionActive)
		{
			return;
		}

		for (const auto& p : glob.coopPlayers)
		{
			if ((!p->isActive) || (p->isPlayer1 && !glob.cam->IsRunning()))
			{
				continue;
			}

			// Obtain lock for node rotation data.
			{
				std::unique_lock<std::mutex> lock(p->mm->nom->rotationDataMutex);
				// Continue early if the fixed strings are not available.
				const auto strings = RE::FixedStrings::GetSingleton();
				if (!strings)
				{
					continue;
				}

				// Continue early if the player's loaded 3D data is invalid.
				auto loadedData = p->coopActor->loadedData;
				if (!loadedData)
				{
					continue;
				}

				// Continue early if the player's 3D is invalid.
				auto data3D = loadedData->data3D;
				if (!data3D || !data3D->parent)
				{
					continue;
				}

				if (Settings::bEnableArmsRotation)
				{
					// Get all arm nodes.
					auto leftShoulderNode = 
					(
						RE::NiPointer<RE::NiAVObject>
						(
							data3D->GetObjectByName(strings->npcLUpperArm)
						)
					);
					auto rightShoulderNode =
					(
						RE::NiPointer<RE::NiAVObject>
						(
							data3D->GetObjectByName(strings->npcRUpperArm)
						)
					);
					auto leftForearmNode = 
					(
						RE::NiPointer<RE::NiAVObject>
						(
							data3D->GetObjectByName(strings->npcLForearm)
						)
					);
					auto rightForearmNode =
					(
						RE::NiPointer<RE::NiAVObject>
						(
							data3D->GetObjectByName("NPC R Forearm [RLar]")
						)
					);
					auto leftHandNode =
					(
						RE::NiPointer<RE::NiAVObject>
						(
							data3D->GetObjectByName("NPC L Hand [LHnd]")
						)
					);
					auto rightHandNode =
					(
						RE::NiPointer<RE::NiAVObject>
						(
							data3D->GetObjectByName("NPC R Hand [RHnd]")
						)
					);
					// Continue early if any node is invalid.
					if (!leftShoulderNode			||
						!leftShoulderNode.get()		||
						!rightShoulderNode			||
						!rightShoulderNode.get()	||
						!leftForearmNode			||
						!leftForearmNode.get()		||
						!leftHandNode				||
						!leftHandNode.get()			||
						!rightForearmNode			||
						!rightForearmNode.get()		||
						!rightHandNode				||
						!rightHandNode.get())
					{
						continue;
					}

					p->mm->nom->UpdateShoulderNodeRotationData(p, leftShoulderNode, false);
					p->mm->nom->UpdateShoulderNodeRotationData(p, rightShoulderNode, true);
					p->mm->nom->UpdateArmNodeRotationData(p, leftForearmNode, leftHandNode, false);
					p->mm->nom->UpdateArmNodeRotationData
					(
						p, rightForearmNode, rightHandNode, true
					);
				}

				auto spineNode = 
				(
					RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcSpine))
				);
				auto spineNode1 = 
				(
					RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcSpine1))
				);
				auto spineNode2 =
				(
					RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcSpine2))
				);
				auto neckNode =
				(
					RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcNeck))
				);
				auto headNode =	
				(
					RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcHead))
				);
				// Continue early if any node is invalid.
				if (!spineNode			||
					!spineNode.get()	||
					!spineNode1			||
					!spineNode1.get()	||
					!spineNode2			||
					!spineNode2.get()	||
					!neckNode			||
					!neckNode.get()		||
					!headNode			||
					!headNode.get())
				{
					continue;
				}	
				
				// Adjust torso nodes' rotations after updating blending state.
				p->mm->nom->UpdateTorsoNodeRotationData(p);
			}
		}
	}

	void GlobalCoopData::ImportUnlockedPerks(RE::Actor* a_coopActor)
	{
		// Import all serialized perks that the player has unlocked.

		SPDLOG_DEBUG("[GLOB] ImportUnlockedPerks: {}", a_coopActor->GetName());
		auto& glob = GetSingleton();
		// Add saved perks to the player if they do not have them added already.
		if (!glob.serializablePlayerData.contains(a_coopActor->formID))
		{
			return;
		}

		bool isP1 = a_coopActor == RE::PlayerCharacter::GetSingleton();
		auto removeAllPerks = 
		[&isP1, a_coopActor](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) 
		{
			if (!a_node)
			{
				return;
			}

			auto perk = a_node->perk;
			// Must remove perks from highest rank to lowest.
			std::stack<RE::BGSPerk*> perkStack;
			uint32_t perkIndex = 0;
			while (perk)
			{
				perkStack.push(perk);
				perk = perk->nextPerk;
				++perkIndex;
			}

			while (!perkStack.empty())
			{
				if (auto perkToRemove = perkStack.top(); perkToRemove)
				{
					// NOTE: 
					// Removing all perks, regardless of whether or not the Actor::HasPerk()
					// check returns true, since for some reason, 
					// the check will return false here for some previously-added perk 
					// (via Util::ChangePerk()) on the current save,
					// and then return true later when importing perks in CopyOverPerkTree() 
					// after reloading the first save,
					// instead of discarding the added perk changes made before reloading. 
					// Possibly due to actor base perk changes traveling across saves, 
					// irrespective of which save the changes were made in?
					// Either way, the bug has been hell to trace, 
					// so remove all perks regardless just to be safe.
					if (isP1)
					{
						Util::Player1RemovePerk(perkToRemove);
					}
					else
					{
						Util::ChangePerk(a_coopActor, perkToRemove, false);
					}
				}

				perkStack.pop();
			}
		};

		Util::TraverseAllPerks(a_coopActor, removeAllPerks);
		const auto& data = glob.serializablePlayerData.at(a_coopActor->formID);
		const auto& unlockedPerksList = data->GetUnlockedPerksList();
		SPDLOG_DEBUG
		(
			"[GLOB] ImportUnlockedPerks: {} has {} unlocked perks serialized for this save file.", 
			a_coopActor->GetName(), unlockedPerksList.size()
		);

		// Add any new animation event-based perks, if needed.
		if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled && Settings::bAddAnimEventSkillPerks)
		{
			// Need to get perks if global data has not been initialized yet.
			if (!glob.globalDataInit) 
			{
				glob.assassinsBladePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58211);
				glob.backstabPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58210);
				glob.criticalChargePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0xCB406);
				glob.deadlyAimPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x1036F0);
				glob.dualCastingAlterationPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153CD);
				glob.dualCastingConjurationPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153CE);
				glob.dualCastingDestructionPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153CF);
				glob.dualCastingIllusionPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153D0);
				glob.dualCastingRestorationPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153D1);
				glob.greatCriticalChargePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0xCB407);
				glob.powerBashPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58F67);
				glob.quickShotPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x105F19);
				glob.shieldChargePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58F6A);
				glob.sneakRollPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x105F23);
			}

			if (glob.criticalChargePerk && !data->HasUnlockedPerk(glob.criticalChargePerk))
			{
				data->InsertUnlockedPerk(glob.criticalChargePerk);
			}

			if (glob.dualCastingAlterationPerk && 
				!data->HasUnlockedPerk(glob.dualCastingAlterationPerk))
			{
				data->InsertUnlockedPerk(glob.dualCastingAlterationPerk);
			}

			if (glob.dualCastingConjurationPerk && 
				!data->HasUnlockedPerk(glob.dualCastingConjurationPerk))
			{
				data->InsertUnlockedPerk(glob.dualCastingConjurationPerk);
			}

			if (glob.dualCastingDestructionPerk && 
				!data->HasUnlockedPerk(glob.dualCastingDestructionPerk))
			{
				data->InsertUnlockedPerk(glob.dualCastingDestructionPerk);
			}

			if (glob.dualCastingIllusionPerk && 
				!data->HasUnlockedPerk(glob.dualCastingIllusionPerk))
			{
				data->InsertUnlockedPerk(glob.dualCastingIllusionPerk);
			}

			if (glob.dualCastingRestorationPerk && 
				!data->HasUnlockedPerk(glob.dualCastingRestorationPerk))
			{
				data->InsertUnlockedPerk(glob.dualCastingRestorationPerk);
			}

			if (glob.greatCriticalChargePerk &&
				!data->HasUnlockedPerk(glob.greatCriticalChargePerk))
			{
				data->InsertUnlockedPerk(glob.greatCriticalChargePerk);
			}

			if (glob.powerBashPerk && !data->HasUnlockedPerk(glob.powerBashPerk))
			{
				data->InsertUnlockedPerk(glob.powerBashPerk);
			}

			if (glob.shieldChargePerk && !data->HasUnlockedPerk(glob.shieldChargePerk))
			{
				data->InsertUnlockedPerk(glob.shieldChargePerk);
			}

			if (glob.sneakRollPerk && !data->HasUnlockedPerk(glob.sneakRollPerk))
			{
				data->InsertUnlockedPerk(glob.sneakRollPerk);
			}
		}

		// Add back all unlocked perks.
		for (const auto perk : unlockedPerksList)
		{
			SPDLOG_DEBUG
			(
				"[GLOB] ImportUnlockedPerks: "
				"Adding back {}'s has saved unlocked perk {} (0x{:X}). "
				"Has perk already: {}",
				a_coopActor->GetName(), perk->GetName(), perk->formID, a_coopActor->HasPerk(perk)
			);
			// NOTE:
			// Adding all unlocked perks again, regardless of whether or not the Actor::HasPerk()
			// check returns true. Same reasoning as removing all perks above.
			if (isP1)
			{
				Util::Player1AddPerk(perk, -1);
			}
			else
			{
				Util::ChangePerk(a_coopActor, perk, true);
			}
		}

		// Prints out all unlocked perks in the perk tree.
		// REMOVE after debugging.
		auto checkPerkTree = 
		[](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) 
		{
			if (!a_node)
			{
				return;
			}
			
			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			auto perk = a_node->perk;
			uint32_t perkIndex = 0;
			while (perk)
			{
				// Selected perks do not get added to the P1 glob list 
				// while the level up menu is open?
				// Have to use native func check here as a result.
				if (p1 && a_actor == p1)
				{
					bool nativeFuncHasPerk = p1->HasPerk(perk);
					bool singletonListHasPerk = Util::Player1PerkListHasPerk(perk);
					if (nativeFuncHasPerk || singletonListHasPerk)
					{
						SPDLOG_DEBUG
						(
							"[GLOB] ImportUnlockedPerks: AFTER IMPORT: "
							"{} has perk #{} {} (0x{:X}): {}, {}.",
							p1->GetName(), perkIndex, perk->GetName(), perk->formID,
							nativeFuncHasPerk, singletonListHasPerk
						);
					}
				}
				else
				{
					if (a_actor->HasPerk(perk))
					{
						SPDLOG_DEBUG
						(
							"[GLOB] ImportUnlockedPerks: "
							"AFTER IMPORT: {} has perk #{} {} (0x{:X})",
							a_actor->GetName(), perkIndex, perk->GetName(), perk->formID
						);
					}
				}

				perk = perk->nextPerk;
				++perkIndex;
			}
		};

		Util::TraverseAllPerks(a_coopActor, checkPerkTree);
	}

	bool GlobalCoopData::IsControllingMenus(const int32_t& a_controllerID)
	{
		// Return true if the player with the given controller ID
		// is controlling open temporary menus.

		auto& glob = GetSingleton();
		if (a_controllerID <= -1 && a_controllerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return false;
		}

		const auto& p = glob.coopPlayers[a_controllerID];
		if (!p->isActive)
		{
			return false;
		}

		return glob.menuCID == p->controllerID;
	}

	bool GlobalCoopData::IsCoopPlayer(const RE::ActorPtr& a_actorPtr)
	{
		// Return true if the given actor smart pointer is a player.

		if (!a_actorPtr || !a_actorPtr.get())
		{
			return false;
		}

		return IsCoopPlayer(a_actorPtr.get());
	}

	bool GlobalCoopData::IsCoopPlayer(RE::TESObjectREFR* a_refr)
	{
		// Return true if the given object refr is a player.

		auto& glob = GetSingleton();
		return 
		(
			std::any_of
			(
				glob.coopPlayers.begin(), glob.coopPlayers.end(),
				[a_refr](const auto& a_p) 
				{
					return a_p->isActive && a_p->coopActor && a_p->coopActor.get() == a_refr; 
				}
			)
		);
	}

	bool GlobalCoopData::IsCoopPlayer(const RE::TESObjectREFRPtr& a_refrPtr)
	{
		// Return true if the given object refr smart ptr is a player.

		if (!a_refrPtr || !a_refrPtr.get())
		{
			return false;
		}

		return IsCoopPlayer(a_refrPtr.get());
	}

	bool GlobalCoopData::IsCoopPlayer(const RE::ObjectRefHandle& a_refrHandle)
	{
		// Return true if the given object refr handle is for a player.

		auto& glob = GetSingleton();
		// Ensure refr is valid first.
		auto refrPtr = Util::GetRefrPtrFromHandle(a_refrHandle); 
		if (refrPtr && refrPtr->IsHandleValid()) 
		{
			return 
			(
				std::any_of
				(
					glob.coopPlayers.begin(), glob.coopPlayers.end(),
					[a_refrHandle](const auto& a_p) 
					{
						return 
						(
							a_p->isActive && 
							a_p->coopActor &&
							a_p->coopActor->GetHandle() == a_refrHandle
						); 
					}
				)
			);
		}
		
		return false;
	}

	bool GlobalCoopData::IsCoopPlayer(const RE::FormID& a_formID)
	{
		// Return true if the given FID is for a player.

		auto& glob = GetSingleton();
		return 
		(
			std::any_of
			(
				glob.coopPlayers.begin(), glob.coopPlayers.end(),
				[a_formID](const auto& a_p) 
				{
					return a_p->isActive && a_p->coopActor && a_p->coopActor->formID == a_formID; 
				}
			)
		);
	}

	bool GlobalCoopData::IsNotControllingMenus(const int32_t& a_controllerID)
	{
		// Return true if the player with the given CID 
		// is not controlling any open temporary menus.

		auto& glob = GetSingleton();
		if (a_controllerID <= -1 || a_controllerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return true;
		}

		const auto& p = glob.coopPlayers[a_controllerID];
		if (!p->isActive)
		{
			return true;
		}

		return glob.menuCID != p->controllerID;
	}

	bool GlobalCoopData::IsSupportedMenuOpen()
	{
		// Return true if a co-op player controllable menu is open.

		auto& glob = GetSingleton();
		const auto ui = RE::UI::GetSingleton(); 
		if (!ui) 
		{
			return false;
		}

		bool supportedMenuOpen = false;
		for (const auto& menuName : glob.SUPPORTED_MENU_NAMES)
		{
			if (!ui->IsMenuOpen(menuName))
			{
				continue;
			}

			return true;
		}

		return false;
	}

	void GlobalCoopData::ModifyLevelUpXPThreshold(const bool& a_setForCoop)
	{
		// Should be called on co-op start/end and after leveling up.
		// Source: https://en.uesp.net/wiki/Skyrim:Leveling#Level_and_Skill_XP_Formulae
		// The XP levelup mult gamesetting is changed each level
		// to indirectly get the desired level up XP threshold.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto p1Skills = p1->skills;
		if (!p1 || !p1Skills) 
		{
			return;
		}

		// Scale levelup threshold with respect to the vanilla game's base and mult values for XP.
		float defBase = glob.defXPLevelUpBase;
		auto valueOpt = Util::GetGameSettingFloat("fXPLevelUpBase");
		if (valueOpt.has_value())
		{
			defBase = valueOpt.value();
		}

		float defMult = glob.defXPLevelUpMult;
		float currentMult = defMult;
		if (valueOpt = Util::GetGameSettingFloat("fXPLevelUpMult"); valueOpt.has_value())
		{
			currentMult = valueOpt.value();
		}

		float currentLevel = p1->GetLevel();
		float newMult = currentMult;
		if (a_setForCoop)
		{
			// Modify the XP levelup mult with our setting.
			newMult = 
			(
				(
					Settings::fLevelUpXPThresholdMult * 
					(defBase + currentLevel * defMult) - defBase
				) / (currentLevel)
			);
		}
		else
		{
			// Restore the saved original.
			newMult = glob.defXPLevelUpMult;
		}

		// Set the new mult.
		if (newMult != currentMult)
		{
			SPDLOG_DEBUG
			(
				"[GLOB] ModifyLevelUpXPThreshold: Level {}, set for co-op: {}. "
				"P1's XP levelup mult is now {}, was {}.",
				p1->GetLevel(), a_setForCoop, newMult, currentMult
			);
			Util::SetGameSettingFloat("fXPLevelUpMult", newMult);
		}

		// Scale levelup threshold with respect to the vanilla game's base and mult values for XP.
		float defaultThreshold = defBase + currentLevel * defMult;
		const float& currentThreshold = p1Skills->data->levelThreshold;
		float newThreshold = defaultThreshold;
		if (a_setForCoop)
		{
			newThreshold = Settings::fLevelUpXPThresholdMult * defaultThreshold;
		}
		else
		{
			// Restore the original.
			newThreshold = defaultThreshold;
		}

		// Set new threshold.
		if (newThreshold != currentThreshold)
		{
			SPDLOG_DEBUG
			(
				"[GLOB] ModifyLevelUpXPThreshold: Level {}, set for co-op: {}. "
				"P1's level threshold is now {}, was {}, XP: {}.",
				p1->GetLevel(), a_setForCoop, newThreshold, currentThreshold, p1Skills->data->xp
			);
			p1Skills->data->levelThreshold = newThreshold;
		}
	}

	void GlobalCoopData::ModifyXPPerSkillLevelMult(const bool& a_setForCoop)
	{
		// Modify the XP per skill level up multiplier based on the number of players.
		// Inversely proportional to the number of living players.

		auto& glob = GetSingleton();
		// Scale down skill levelup XP mult, based on the co-op party size.
		// Defaults to 1.0.
		float currentXPMult = 1.0f;
		float newXPMult = 1.0f;
		auto valueOpt = Util::GetGameSettingFloat("fXPPerSkillRank"); 
		if (valueOpt.has_value())
		{
			currentXPMult = valueOpt.value();
		}

		if (a_setForCoop)
		{
			newXPMult = 1.0f / glob.livingPlayers;
		}

		if (currentXPMult != newXPMult)
		{
			bool succ = Util::SetGameSettingFloat("fXPPerSkillRank", newXPMult);
			SPDLOG_DEBUG
			(
				"[GLOB] ModifyXPPerSkillLevelMult: Update fXPPerSkillRank: "
				"{} -> {}: {}. Set for co-op: {}.", 
				currentXPMult, newXPMult, succ ? "SUCCESS" : "FAILURE", a_setForCoop
			);
		}
	}

	void GlobalCoopData::PerformInitialAVAutoScaling()
	{
		// Sometimes player actors do not auto-scale their AVs until their 3D is loaded in (?):
		// Auto-scaling during serialization load fails at times. 
		// Called on co-op session start/end instead.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return;
		}

		// Auto scale without updating base AVs first.
		bool succ = TriggerAVAutoScaling(nullptr, false);
		if (!succ) 
		{
			SPDLOG_DEBUG
			(
				"[GLOB] ERR: PerformInitialAVAutoScaling: Auto-scaling failed. "
				"Not updating serialized base AVs for any player."
			);
		}

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive || 
				p->isPlayer1 || 
				!glob.serializablePlayerData.contains(p->coopActor->formID))
			{
				continue;
			}

			auto& data = glob.serializablePlayerData.at(p->coopActor->formID);
			// Already saved previously, no need to handle.
			if (data->firstSavedLevel != 0)
			{
				continue;
			}

			data->firstSavedLevel = p1->GetLevel();
			SPDLOG_DEBUG
			(
				"[GLOB] PerformInitialAVAutoScaling: First co-op level-up for {}. "
				"First saved level set to {}. Auto-scale AVs.",
				data->firstSavedLevel, p->coopActor->GetName()
			);

			// Check for differences between auto-scaled skills and current skills lists.
			// If the current skill AV is greater than the auto-scaled one,
			// indicating some progression before the first co-op session,
			// save the difference to the skill increments list.
			auto currentSkills = data->skillBaseLevelsList;
			auto autoScaledSkills = Util::GetActorSkillLevels(p->coopActor.get());
			for (auto j = 0; j < currentSkills.size(); ++j)
			{
				if (currentSkills[j] > autoScaledSkills[j])
				{
					data->skillBaseLevelsList[j] = autoScaledSkills[j];
					data->skillLevelIncreasesList[j] = currentSkills[j] - autoScaledSkills[j];
				}
				else
				{
					data->skillBaseLevelsList[j] = autoScaledSkills[j];
					data->skillLevelIncreasesList[j] = 0.0f;
				}
					
#ifdef ALYSLC_DEBUG_MODE
				// REMOVE after debugging.
				auto currentSkill = static_cast<Skill>(j);
				if (!SKILL_TO_AV_MAP.contains(currentSkill))
				{
					continue;
				}
					
				auto currentAV = SKILL_TO_AV_MAP.at(currentSkill);
				SPDLOG_DEBUG
				(
					"[GLOB] PerformInitialAVAutoScaling: "
					"{}'s {} skill levels are now: ({} + {}) (current: {}, rescaled: {}).",
					p->coopActor->GetName(),
					Util::GetActorValueName(currentAV),
					data->skillBaseLevelsList[j],
					data->skillLevelIncreasesList[j],
					currentSkills[j],
					autoScaledSkills[j]
				);
#endif
			}
		}
	}

	void GlobalCoopData::PerformPlayerRespec(RE::Actor* a_playerActor)
	{
		// Reset the player's health/magicka/stamina AVs to their initial values,
		// remove all unlocked perks, and also remove all unlocked shared perks 
		// for all active players.

		if (!a_playerActor)
		{
			return;
		}

		ResetToBaseHealthMagickaStamina(a_playerActor);
		ResetPerkData(a_playerActor);
	}

	void GlobalCoopData::RegisterEvents()
	{
		// Register the P1 ref alias for script events.

		auto& glob = GetSingleton();
		if (!glob.onCoopHelperMenuRequest.Register(glob.player1RefAlias))
		{
			SPDLOG_DEBUG
			(
				"[GLOB] RegisterEvents: Could not register player ref alias ({}) "
				"for OnCoopHelperMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str()
			);
		}
		else
		{
			SPDLOG_DEBUG("[GLOB] RegisterEvents: Registered OnCoopHelperMenuRequest() event");
		}

		if (!glob.onDebugMenuRequest.Register(glob.player1RefAlias))
		{
			SPDLOG_DEBUG
			(
				"[GLOB] RegisterEvents: Could not register player ref alias ({}) "
				"for OnDebugMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str()
			);
		}
		else
		{
			SPDLOG_DEBUG("[GLOB] RegisterEvents: Registered OnDebugMenuRequest() event");
		}

		if (!glob.onSummoningMenuRequest.Register(glob.player1RefAlias))
		{
			SPDLOG_DEBUG
			(
				"[GLOB] RegisterEvents: Could not register player ref alias ({}) "
				"for OnSummoningMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str()
			);
		}
		else
		{
			SPDLOG_DEBUG("[GLOB] RegisterEvents: Registered OnSummoningMenuRequest() event");
		}
	}

	void GlobalCoopData::RescaleActivePlayerAVs()
	{
		// Rescale active player HMS and skill AVs to serialized values. 
		// Should be performed after the game auto-scales any of these values.

		auto& glob = GetSingleton();
		if (!glob.allPlayersInit)
		{
			return;
		}

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			// Ensure active player's FID is used to index into serializable data map.
			if (!glob.serializablePlayerData.contains(p->coopActor->formID))
			{
				SPDLOG_DEBUG
				(
					"[GLOB] ERR: RescaleActivePlayerAVs: "
					"Could not index serialized data with {}'s form ID (0x{:X}).",
					p->coopActor->GetName(), p->coopActor->formID
				);
				continue;
			}
			
			// NOTE for Enderal:
			// No need to rescale HMS AVs for P1,
			// and companion player HMS values are only affected by the player's class as of now.
			if (!p->isPlayer1)
			{
				SPDLOG_DEBUG("[GLOB] RescaleActivePlayerAVs: About to rescale HMS for {}.", 
					p->coopActor->GetName());
				// Skill AVs first.
				RescaleSkillAVs(p->coopActor.get());
				if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
				{
					const auto& data = glob.serializablePlayerData.at(p->coopActor->formID);
					RescaleHMS(p->coopActor.get(), data->firstSavedLevel);
				}
			}
			else if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
			{
				SPDLOG_DEBUG("[GLOB] RescaleActivePlayerAVs: About to rescale HMS for P1.");
				RescaleHMS(p->coopActor.get());
			}
		}
	}

	void GlobalCoopData::RescaleAVsOnBaseSkillAVChange(RE::Actor* a_playerActor)
	{
		// Rescale HMS and skill AVs.
		// Call when the given player's base stats change.
		// NOTE: Can be called with no co-op session active:
		// for example, when in the summoning menu and changing a companion player's class/race.
		
		// Cannot scale if either the co-op companion actor or P1 are invalid.
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_playerActor)
		{
			return;
		}

		auto& glob = GetSingleton();
		// Ensure active player's FID is used to index into serializable data map.
		if (!glob.serializablePlayerData.contains(a_playerActor->formID))
		{
			SPDLOG_DEBUG
			(
				"[GLOB] ERR: RescaleAVsOnBaseSkillAVChange: "
				"Could not index serialized data with {}'s form ID (0x{:X}).",
				a_playerActor->GetName(), a_playerActor->formID
			);
			return;
		}

		// Auto scale first and save new base actor values.
		if (TriggerAVAutoScaling(a_playerActor, true)) 
		{
			// Scale skill AVs next.
			RescaleSkillAVs(a_playerActor);
			// Lastly, scale up HMS with saved increases.
			// NOTE for Enderal:
			// Health, magicka, and stamina are only modified 
			// by auto-scaling based on your chosen class.
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
			{
				return;
			}

			const auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
			RescaleHMS(a_playerActor, data->firstSavedLevel);
		}
	}

	void GlobalCoopData::ResetMenuCIDs()
	{
		// Update previous menu CID to the current one and clear the current menu CID 
		// or set to P1's CID if menus are open still.

		auto& glob = GetSingleton();
		// With no menus open, no player is in control of menus.
		// Otherwise, give P1 control.
		int32_t newCID = Util::MenusOnlyAlwaysOpen() ? -1 : glob.player1CID;
		// Previous menu CID is NEVER -1 after it is first set.
		int32_t newPrevCID = newCID != -1 ? newCID : glob.prevMenuCID;

		// REMOVE when done debugging.
		SPDLOG_DEBUG
		(
			"[GLOB] ResetMenuCIDs: Reset menu CID from {} to {}, last menu CID from {} to {}.",
			glob.menuCID, 
			newCID,
			glob.prevMenuCID,
			newPrevCID
		);

		{
			std::unique_lock<std::mutex> lock(glob.menuCIDMutex, std::try_to_lock);
			if (lock)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] ResetMenuCIDs: Lock obtained. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
				glob.prevMenuCID = newPrevCID;
				glob.menuCID = newCID;
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[GLOB] ResetMenuCIDs: Failed to obtain lock. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
			}
		}
	}

	void GlobalCoopData::SaveUnlockedPerksForAllPlayers()
	{
		// Serialize unlocked perk data for all players.

		auto& glob = GetSingleton();
		if (!glob.allPlayersInit || !glob.coopSessionActive) 
		{
			return;
		}

		for (const auto& p : glob.coopPlayers) 
		{
			if (!p->isActive) 
			{
				continue;
			}

			SaveUnlockedPerksForPlayer(p->coopActor.get());
		}
	}

	void GlobalCoopData::SaveUnlockedPerksForPlayer(RE::Actor* a_coopActor)
	{
		// Save all unlocked perks to serialized data for the given player actor.
		

		// NOTE: 
		// The game randomly clears P1's perks sometimes.
		// I have yet to find a reason why it does this or find a direct solution,
		// so the current workaround is to import P1's serialized perks
		// when opening the Stats Menu and only save P1's perks 
		// on exiting the Stats Menu, even outside of co-op.

		if (!a_coopActor)
		{
			return;
		}

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return;
		}

		SPDLOG_DEBUG("[GLOB] SaveUnlockedPerksForPlayer: {}", a_coopActor->GetName());
		auto& glob = GetSingleton();
		if (!glob.serializablePlayerData.contains(a_coopActor->formID)) 
		{
			SPDLOG_DEBUG
			(
				"[GLOB] ERR: SaveUnlockedPerksForPlayer: {}: "
				"Could not get serializable data for player with form ID 0x{:X}.",
				a_coopActor->GetName(), a_coopActor->formID
			);
			return;
		}

		auto& serializedData = glob.serializablePlayerData.at(a_coopActor->formID);
		// Save each player's perks to their serializable unlocked perks list.
		auto savePlayerPerksVisitor = 
		[p1, &serializedData, &glob](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) 
		{
			if (!a_node)
			{
				return;
			}

			auto perk = a_node->perk;
			// Save p1's perks to serializable data.
			while (perk)
			{
				bool shared = SHARED_SKILL_NAMES_SET.contains
				(
					a_node->associatedSkill->enumName
				);
				bool nativeFuncHasPerk = a_actor->HasPerk(perk);
				if (a_actor == p1) 
				{
					bool singletonListHasPerk = Util::Player1PerkListHasPerk(perk);
					// When not in co-op, the native func perk check either does not recognize 
					// that a perk was added (when first loading a save after starting the game),
					// or captures changes made in a newer save
					// (if loading an older save after choosing new perks). 
					// Reference the glob perk list when out of co-op for these reasons.
					if (nativeFuncHasPerk || singletonListHasPerk)
					{
						// If either check indicates that the player has the current perk, 
						// add it to the list.
						if (glob.coopSessionActive) 
						{
							SPDLOG_DEBUG
							(
								"[GLOB] SaveUnlockedPerksForPlayer: {} has perk {} (0x{:X}) "
								"(native func: {}, glob list: {})",
								a_actor->GetName(), 
								perk->GetName(), 
								perk->formID,
								nativeFuncHasPerk,
								singletonListHasPerk
							);
							if (nativeFuncHasPerk != singletonListHasPerk)
							{
								SPDLOG_DEBUG
								(
									"[GLOB] ERR: SaveUnlockedPerksForPlayer "
									"{} has perk check inconsistency. Adding {} (0x{:X}).",
									a_actor->GetName(), perk->GetName(), perk->formID
								);
								Util::Player1AddPerk(perk, -1);
							}

							serializedData->InsertUnlockedPerk(perk);
						}
						else if (nativeFuncHasPerk)
						{
							SPDLOG_DEBUG
							(
								"[GLOB] SaveUnlockedPerksForPlayer: NO CO-OP: "
								"{} has perk {} (0x{:X}) (native func: {}, glob list: {})",
								a_actor->GetName(), 
								perk->GetName(), 
								perk->formID, 
								nativeFuncHasPerk, 
								singletonListHasPerk
							);
							// When out of co-op, only add the current perk 
							// if it is in the glob list.
							Util::Player1AddPerk(perk, -1);
							serializedData->InsertUnlockedPerk(perk);
						}
					}
				}
				else if (nativeFuncHasPerk)
				{
					SPDLOG_DEBUG
					(
						"[GLOB] SaveUnlockedPerksForPlayer: {} has perk {} (0x{:X}), "
						"is shared: {}", 
						a_actor->GetName(), perk->GetName(), perk->formID, shared
					);
					serializedData->InsertUnlockedPerk(perk);
				}

				perk = perk->nextPerk;
			}
		};

		SPDLOG_DEBUG("[GLOB] SaveUnlockedPerksForPlayer BEFORE: {} has {} unlocked perks.",
			a_coopActor->GetName(), serializedData->GetUnlockedPerksList().size());

		// Clear and re-add.
		serializedData->ClearUnlockedPerks();
		Util::TraverseAllPerks(a_coopActor, savePlayerPerksVisitor);

		SPDLOG_DEBUG("[GLOB] SaveUnlockedPerksForPlayer AFTER: {} has {} unlocked perks.",
			a_coopActor->GetName(), serializedData->GetUnlockedPerksList().size());
	}

	void GlobalCoopData::SetCrosshairText()
	{
		// Credits to Ryan-rsm-McKenzie and his quick loot repo for
		// the code on modifying the crosshair text.
		// https://github.com/Ryan-rsm-McKenzie
		// Set crosshair text to the concatenation of all players' notification messages.

		auto& glob = GetSingleton(); 
		if (!glob.coopSessionActive ||
			glob.player1CID == -1 ||
			!glob.coopPlayers[glob.player1CID]) 
		{
			return;
		}

		// Can't concatenate to fixed string, so use a temp string. P1 is always first.
		std::string tempCrosshairText = 
		(
			std::string(glob.coopPlayers[glob.player1CID]->tm->crosshairMessage->text) + "\n"
		);
		// Concatenate the other active players' messages to P1's.
		for (uint8_t i = 0; i < glob.coopPlayers.size(); ++i) 
		{
			const auto& p = glob.coopPlayers[i]; 
			if (!p || !p->isActive || p->isPlayer1) 
			{
				continue;
			}
			
			tempCrosshairText += std::string(p->tm->crosshairMessage->text) + "\n";
		}

		// Copy over to fixed string and send a copy to the task.
		RE::BSFixedString crosshairTextToSet = tempCrosshairText;
		SKSE::GetTaskInterface()->AddUITask
		(
			[&glob, crosshairTextToSet]() 
			{
				auto ui = RE::UI::GetSingleton(); 
				if (!ui)
				{
					return;
				}

				auto hudMenu = ui->GetMenu<RE::HUDMenu>(); 
				if (!hudMenu)
				{
					return;
				}

				auto view = hudMenu->uiMovie; 
				if (!view)
				{
					return;
				}

				RE::GFxValue hudBase;
				view->GetVariable(std::addressof(hudBase), "_root.HUDMovieBaseInstance");
				if (hudBase.IsNull() || hudBase.IsUndefined() || !hudBase.IsObject())
				{
					return;
				}

				std::array<RE::GFxValue, HUDBaseArgs::kTotal> crosshairTextArgs;
				crosshairTextArgs.fill(RE::GFxValue());
				crosshairTextArgs[HUDBaseArgs::kActivate] = RE::GFxValue(false);
				crosshairTextArgs[HUDBaseArgs::kShowButton] = RE::GFxValue(false);
				crosshairTextArgs[HUDBaseArgs::kTextOnly] = RE::GFxValue(true);
				crosshairTextArgs[HUDBaseArgs::kFavorMode] = RE::GFxValue(false);
				crosshairTextArgs[HUDBaseArgs::kShowCrosshair] = RE::GFxValue(false);
				crosshairTextArgs[HUDBaseArgs::kName] = RE::GFxValue(crosshairTextToSet);
				hudBase.Invoke("SetCrosshairTarget", crosshairTextArgs);

				RE::GFxValue rolloverText;
				view->GetVariable(&rolloverText, "HUDMovieBaseInstance.RolloverText");
				if (rolloverText.IsNull() || 
					rolloverText.IsUndefined() || 
					!rolloverText.IsObject())
				{
					return;
				}

				// Ensure text is visible and has full alpha.
				if (rolloverText.HasMember("_alpha"))
				{
					RE::GFxValue alpha;
					rolloverText.GetMember("_alpha", std::addressof(alpha));
					if (alpha.GetNumber() != 100.0)
					{
						alpha.SetNumber(100.0);
						rolloverText.SetMember("_alpha", alpha);
						view->SetVariable
						(
							"HUDMovieBaseInstance.RolloverText", 
							rolloverText,
							RE::GFxMovie::SetVarType::kPermanent
						);
					}
				}

				if (rolloverText.HasMember("_visible"))
				{
					RE::GFxValue visible;
					rolloverText.GetMember("_visible", std::addressof(visible));
					if (!visible.GetBool())
					{
						visible.SetBoolean(true);
						rolloverText.SetMember("_visible", visible);
						view->SetVariable
						(
							"HUDMovieBaseInstance.RolloverText",
							rolloverText,
							RE::GFxMovie::SetVarType::kPermanent
						);
					}
				}
			}
		);
	}

	void GlobalCoopData::SetDefaultXPBaseAndMultFromGameSettings()
	{
		auto& glob = GetSingleton();

		glob.defXPLevelUpMult = 25.0f;
		glob.defXPLevelUpBase = 75.0f;
		auto valueOpt = Util::GetGameSettingFloat("fXPLevelUpMult");
		if (valueOpt.has_value())
		{
			glob.defXPLevelUpMult = valueOpt.value();
		}

		if (valueOpt = Util::GetGameSettingFloat("fXPLevelUpBase"); valueOpt.has_value())
		{
			glob.defXPLevelUpBase = valueOpt.value();
		}

		SPDLOG_DEBUG
		(
			"[GLOB] SetDefaultXPBaseAndMultFromGameSettings: "
			"Default XP game settings: base: {}, mult: {}.",
			glob.defXPLevelUpBase, glob.defXPLevelUpMult
		);
	}

	void GlobalCoopData::SetMenuCIDs(const int32_t a_controllerID)
	{
		// Set previous and current menu CIDs directly to the given CID.

		auto& glob = GetSingleton();
		if (a_controllerID == -1) 
		{
			ResetMenuCIDs();
		}
		else
		{
			SPDLOG_DEBUG("[GLOB] SetMenuCIDs: Set current/last menu CIDs from {}/{} to {}.",
				glob.menuCID, glob.prevMenuCID, a_controllerID);
			{
				std::unique_lock<std::mutex> lock(glob.menuCIDMutex, std::try_to_lock);
				if (lock)
				{
					SPDLOG_DEBUG("[GLOB] SetMenuCIDs: Lock obtained. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id()));
					glob.prevMenuCID = glob.menuCID = a_controllerID;
				}
				else
				{
					SPDLOG_DEBUG("[GLOB] SetMenuCIDs: Failed to obtain lock. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id()));
				}
			}
		}
	}

	void GlobalCoopData::StopAllCombatOnCoopPlayers
	(
		bool&& a_onlyAmongParty, bool&& a_removeCrimeGold
	)
	{
		// Stop combat for all NPCs or only party NPCs towards all players. 
		// Optionally remove all crime gold counts (bounties) as well.

		auto& glob = GetSingleton();
		if (!glob.globalDataInit)
		{
			return;
		}

		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto procLists = RE::ProcessLists::GetSingleton(); 
		RE::TESForm* goldObj = nullptr;
		if (auto defObjMgr = RE::BGSDefaultObjectManager::GetSingleton(); defObjMgr)
		{
			goldObj = defObjMgr->objects[RE::DEFAULT_OBJECT::kGold];
		}

		// Exit if P1, process lists, or the gold object is invalid.
		if (!p1 || !procLists || !goldObj) 
		{
			return;
		}

		auto actorStopCombat =
		[p1, goldObj, procLists, &a_onlyAmongParty, &a_removeCrimeGold]
		(RE::ActorHandle a_actorHandle) 
		{
			auto actorPtr = Util::GetActorPtrFromHandle(a_actorHandle); 
			if (!actorPtr || !actorPtr.get())
			{
				return;
			}

			// Ignore actors that are not in combat or angry with P1.
			if (!actorPtr->IsInCombat() && !actorPtr->IsHostileToActor(p1))
			{
				return;
			}

			// Give P1 an amount of gold equal to their bounty before paying their bounty 
			// so that they break even. P1 and co. get off scot-free.
			if (a_removeCrimeGold)
			{
				// Traverse all added and actorbase factions.
				if (auto base = actorPtr->GetActorBase(); base)
				{
					for (auto& factionInfo : base->factions)
					{
						auto faction = factionInfo.faction; 
						if (!faction)
						{
							continue;
						}

						float crimeGold = faction->GetCrimeGold();
						// Ignore factions with no bounty on P1.
						if (crimeGold <= 0 || !goldObj || !goldObj->IsBoundObject())
						{
							continue;
						}

						p1->AddObjectToContainer
						(
							goldObj->As<RE::TESBoundObject>(), nullptr, crimeGold, p1
						);
						faction->PlayerPayCrimeGold(false, false);
					}

					auto factionChanges = actorPtr->extraList.GetByType<RE::ExtraFactionChanges>();
					if (factionChanges)
					{
						for (auto& change : factionChanges->factionChanges)
						{
							auto faction = change.faction;
							if (!faction)
							{
								continue;
							}

							float crimeGold = faction->GetCrimeGold();
							// Ignore factions with no bounty on P1.
							if (crimeGold <= 0 || !goldObj || !goldObj->IsBoundObject())
							{
								continue;
							}
							
							p1->AddObjectToContainer
							(
								goldObj->As<RE::TESBoundObject>(), nullptr, crimeGold, p1
							);
							faction->PlayerPayCrimeGold(false, false);
						}
					}
				}
			}

			// Stop combat for all actors.
			if (!a_onlyAmongParty)
			{
				if (actorPtr->combatController)
				{
					actorPtr->combatController->ignoringCombat = true;
					actorPtr->combatController->stoppedCombat = true;
				}

				actorPtr->StopCombat();
			}
			else if (actorPtr->IsPlayerTeammate())
			{
				// Only stop combat for player teammates, which includes players.

				// Stop attacking this instant.
				if (!GlobalCoopData::IsCoopPlayer(actorPtr)) 
				{
					actorPtr->NotifyAnimationGraph("attackStop");
				}

				if (actorPtr->combatController)
				{
					actorPtr->combatController->stoppedCombat = true;
				}

				actorPtr->StopCombat();
				actorPtr->currentProcess->lowProcessFlags.reset
				(
					RE::AIProcess::LowProcessFlags::kAlert
				);
			}

			procLists->ClearCachedFactionFightReactions();
		};

		// Stop combat for all actors at each process level, 
		// so that there are no straggling NPCs that are still hostile,
		// causing everyone's favorite 'can't wait while enemies are nearby' issue.
		for (const auto& actorHandle : procLists->highActorHandles)
		{
			actorStopCombat(actorHandle);
		}

		for (const auto& actorHandle : procLists->middleHighActorHandles)
		{
			actorStopCombat(actorHandle);
		}

		for (const auto& actorHandle : procLists->middleLowActorHandles)
		{
			actorStopCombat(actorHandle);
		}

		for (const auto& actorHandle : procLists->lowActorHandles)
		{
			actorStopCombat(actorHandle);
		}

		if (glob.allPlayersInit)
		{
			// Stop all NPC alarms on players 
			// if not stopping combat within the player party itself.
			if (!a_onlyAmongParty)
			{
				for (const auto& p : glob.coopPlayers)
				{
					if (!p || !p->isActive || !p->coopActor)
					{
						continue;
					}
					
					procLists->StopCombatAndAlarmOnActor(p->coopActor.get(), !a_removeCrimeGold);
				}
			}
		}
		else
		{
			// Also works outside of co-op.
			procLists->StopCombatAndAlarmOnActor(glob.player1Actor.get(), !a_removeCrimeGold);
		}

		if (a_removeCrimeGold)
		{
			// Remove all crime gold from factions within P1's crime gold map too.
			// P1 and co. get off scot-free.
			for (auto& [constFaction, _] : p1->crimeGoldMap)
			{
				auto faction = RE::TESForm::LookupByID<RE::TESFaction>(constFaction->formID); 
				if (!faction || faction->GetCrimeGold() <= 0.0f)
				{
					continue;
				}

				// Quick reimbursement.
				p1->AddObjectToContainer
				(
					goldObj->As<RE::TESBoundObject>(), nullptr, faction->GetCrimeGold(), p1
				);
				faction->PlayerPayCrimeGold(false, false);
			}
		}
	}

	void GlobalCoopData::StopMenuInputManager()
	{
		// Request to pause the menu input manager.
		// Gives active menu control to P1.

		auto& glob = GetSingleton();
		if (glob.globalDataInit)
		{
			if (glob.mim->IsRunning())
			{
				glob.mim->ToggleCoopPlayerMenuMode(-1);
				glob.mim->ResetPlayerMenuControlOverlay();
			}

			ResetMenuCIDs();
		}
	}

	void GlobalCoopData::SyncSharedPerks()
	{
		// Ensure all players have the same set of unlocked shared perks.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1) 
		{
			return;
		}

		if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
		{
			// For Enderal, just sync shared perks with P1.
			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive || p->isPlayer1)
				{
					continue;
				}
				
				// Add any shared perks that P1 has but this player does not have.
				for (auto perk : p1->perks)
				{
					// Already has the perk, so on to the next one.
					if (p->coopActor->HasPerk(perk)) 
					{
						continue;
					}
					
					SPDLOG_DEBUG("[GLOB] SyncSharedPerks: P1 {} has perk {}. Adding to {}.",
						p1->GetName(), perk->GetName(), p->coopActor->GetName());
					Util::ChangePerk(p->coopActor.get(), perk, true);
				}
			}
		}
		else
		{
			// Add all shared skill trees' perks to the co-op player 
			// to keep these perks in sync among all players.
			auto addSharedSkillPerks = 
			[p1, &glob](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) 
			{
				if (!a_node)
				{
					return;
				}

				auto perk = a_node->perk;
				while (perk)
				{
					bool shared = SHARED_SKILL_NAMES_SET.contains
					(
						a_node->associatedSkill->enumName
					);
					// P1 has the shared perk, so all other player should as well.
					if ((shared) && 
						(p1->HasPerk(perk) || Util::Player1PerkListHasPerk(perk)))
					{
						bool hadSharedPerk = true;
						// Add unlocked shared perk to serialized data
						// if it's not already present.
						if (glob.serializablePlayerData.contains(a_actor->formID))
						{
							auto& data = glob.serializablePlayerData.at(a_actor->formID);
							if (!data->HasUnlockedPerk(perk))
							{
								data->InsertUnlockedPerk(perk);
								hadSharedPerk = false;
							}
						}

						// Add the perk.
						bool succ = Util::ChangePerk(a_actor, perk, true);
						SPDLOG_DEBUG
						(
							"[GLOB] SyncSharedPerks TraversePerkTree. "
							"Adding shared perk {} (0x{:X}) to {}: {}. "
							"Had unlocked shared perk in list: {}",
							perk->GetName(), 
							perk->formID,
							a_actor->GetName(), 
							succ ? "SUCC" : "FAIL",
							hadSharedPerk
						);
					}

					perk = perk->nextPerk;
				}
			};

			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive || p->isPlayer1)
				{
					continue;
				}

				// Sync all shared skill tree perks for companion players.
				Util::TraverseAllPerks(p->coopActor.get(), addSharedSkillPerks);
			}
		}
	}

	void GlobalCoopData::SyncSharedSkillAVs()
	{
		// Sync shared skills' AV levels among all active players.

		auto& glob = GetSingleton();
		// Sync all shared skill AVs for each player.
		// The highest shared skill level is used 
		// for each shared skill.
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			p->pam->CopyOverSharedSkillAVs();
		}
	}

	void GlobalCoopData::TeardownCoopSession(bool a_shouldDismiss)
	{
		// End the current co-op session by signalling all managers to await refresh, 
		// and optionally dismiss all companion players.

		auto& glob = GetSingleton();
		// No global data or players, so bail.
		if (!glob.globalDataInit || 
			!glob.allPlayersInit ||
			glob.player1CID < 0 || 
			glob.player1CID >= ALYSLC_MAX_PLAYER_COUNT) 
		{
			return;
		}

		// Set the co-op session as ended.
		glob.coopSessionActive = false;
		if (a_shouldDismiss)
		{
			// Dismiss P1 last, as the P1 ref alias script performs final cleanup measures 
			// for the co-op session.
			for (const auto& p : glob.coopPlayers)
			{
				if (!p || !p->isActive || p->isPlayer1)
				{
					continue;
				}

				SPDLOG_DEBUG
				(
					"[GLOB] TeardownCoopSession: Co-op session over. Dismissing companion {}.", 
					p->coopActor->GetName()
				);
				p->DismissPlayer();
			}

			const auto& coopP1 = glob.coopPlayers[glob.player1CID];
			if (coopP1 && coopP1.get() && coopP1->isActive) 
			{
				SPDLOG_DEBUG
				(
					"[GLOB] TeardownCoopSession: Co-op session over. Dismissing P1 {}.", 
					coopP1->coopActor->GetName()
				);
				glob.coopPlayers[glob.player1CID]->DismissPlayer();
			}
		}
		else
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (!p || !p->isActive)
				{
					continue;
				}
				
				SPDLOG_DEBUG
				(
					"[GLOB] TeardownCoopSession: Co-op session over. "
					"Signalling managers to await refresh for {}.", 
					p->coopActor->GetName()
				);
				p->RequestStateChange(ManagerState::kAwaitingRefresh);
			}
		}

		// Ensure any copied data is reverted for P1.
		if (glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone) 
		{
			SPDLOG_DEBUG
			(
				"[GLOB] TeardownCoopSession: Co-op session ended with data copied "
				"(types: 0b{:B}) over to P1. Restoring P1's data.",
				*glob.copiedPlayerDataTypes
			);
			CopyOverCoopPlayerData
			(
				false, "CO-OP SESSION ENDED", glob.player1Actor->GetHandle(), nullptr
			);
		}

		SPDLOG_DEBUG
		(
			"[GLOB] TeardownCoopSession: Co-op session over. "
			"Pausing camera manager and awaiting the start of a new co-op session."
		);
		glob.cam->RequestStateChange(ManagerState::kPaused);
	}

	void GlobalCoopData::ToggleGodModeForAllPlayers(const bool& a_enable)
	{
		// Enable or disable god mode for all players.

		auto& glob = GetSingleton();
		for (const auto& p : glob.coopPlayers) 
		{
			if (!p || !p->isActive)
			{
				continue;
			}

			ToggleGodModeForPlayer(p->controllerID, a_enable);
		}
	}

	void GlobalCoopData::ToggleGodModeForPlayer
	(
		const int32_t& a_controllerID, const bool& a_enable
	)
	{
		// Enable or disable god mode for the player associated with the given CID.

		if (a_controllerID <= -1 || a_controllerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}

		auto& glob = GetSingleton();
		const auto& p = glob.coopPlayers[a_controllerID];
		if (!p->isActive) 
		{
			return;
		}

		if (p->isPlayer1)
		{
			p->isInGodMode = p->coopActor->IsInvulnerable() && !p->coopActor->IsGhost();
			if ((a_enable && !p->isInGodMode) || (!a_enable && p->isInGodMode))
			{
				SPDLOG_DEBUG("[GLOB] ToggleGodModeForPlayer: Should {} god mode for P1.", 
					a_enable ? "set" : "unset");

				const auto scriptFactory = 
				(
					RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
				);
				const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
				if (script)
				{
					script->SetCommand("tgm");
					script->CompileAndRun(p->coopActor.get());
					// Cleanup.
					delete script;
					p->isInGodMode = a_enable;
				}
			}

			// Enderal: 
			// Remove arcane fever related effects (uses the 'LastFlattered' AV), 
			// since reaching 100% arcane fever will not kill P1 while in god mode, 
			// and will also completely prevent leveling up in the future
			// if the game is saved while at 100% arcane fever.
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled && p->isInGodMode)
			{
				for (auto effect : *p->coopActor->GetActiveEffectList())
				{
					if (!effect)
					{
						continue;
					}

					if (auto baseObj = effect->GetBaseObject(); (baseObj) && 
						(baseObj->data.primaryAV == RE::ActorValue::kLastFlattered || 
						baseObj->data.secondaryAV == RE::ActorValue::kLastFlattered))
					{
						effect->Dispel(true);
					}
				}

				// Set all 'LastFlattered' AV and AV modifiers to 0.
				p->coopActor->SetActorValue(RE::ActorValue::kLastFlattered, 0.0f);
				p->coopActor->SetBaseActorValue(RE::ActorValue::kLastFlattered, 0.0f);
				auto avOwner = p->coopActor->As<RE::ActorValueOwner>();
				if (!avOwner)
				{
					return;
				}
				
				float restoreAmount = 
				(
					-p->coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kLastFlattered
					)
				);
				avOwner->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, 
					RE::ActorValue::kLastFlattered,
					restoreAmount
				);

				restoreAmount = 
				(
					-p->coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kLastFlattered
					)
				);
				avOwner->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, 
					RE::ActorValue::kLastFlattered,
					restoreAmount
				);

				restoreAmount = 
				(
					-p->coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kLastFlattered
					)
				);
				avOwner->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent,
					RE::ActorValue::kLastFlattered,
					restoreAmount
				);

				restoreAmount = -p->coopActor->GetActorValue(RE::ActorValue::kLastFlattered);
				avOwner->ModActorValue(RE::ActorValue::kLastFlattered, restoreAmount);
			}
		}
		else
		{
			auto actorBase = p->coopActor->GetActorBase(); 
			if (!actorBase)
			{
				return;
			}

			auto& baseFlags = actorBase->actorData.actorBaseFlags;
			p->isInGodMode = baseFlags.all
			(
				RE::ACTOR_BASE_DATA::Flag::kInvulnerable, RE::ACTOR_BASE_DATA::Flag::kDoesntBleed
			);
			// Set god mode flag to prevent AV expenditure.
			if (a_enable && !p->isInGodMode)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] ToggleGodModeForPlayer: Set is ghost/invuln/nobleed to TRUE for {}",
					p->coopActor->GetName()
				);

				baseFlags.set
				(
					RE::ACTOR_BASE_DATA::Flag::kInvulnerable, 
					RE::ACTOR_BASE_DATA::Flag::kDoesntBleed
				);
				p->isInGodMode = true;
			}
			else if (!a_enable && p->isInGodMode)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] ToggleGodModeForPlayer: Set is ghost/invuln/nobleed to FALSE for {}", 
					p->coopActor->GetName()
				);

				baseFlags.reset
				(
					RE::ACTOR_BASE_DATA::Flag::kInvulnerable,
					RE::ACTOR_BASE_DATA::Flag::kDoesntBleed
				);
				p->isInGodMode = false;
			}
		}
	}

	void GlobalCoopData::UnregisterEvents()
	{
		// Unregister P1 ref alias for script events.

		auto& glob = GetSingleton();
		if (!glob.onCoopHelperMenuRequest.Unregister(glob.player1RefAlias))
		{
			SPDLOG_DEBUG
			(
				"[GLOB] UnregisterEvents: Could not unregister player ref alias ({}) "
				"for OnCoopHelperMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str()
			);
		}
		else
		{
			SPDLOG_DEBUG("[GLOB] UnregisterEvents: Unregistered OnCoopHelperMenuRequest() event");
		}

		if (!glob.onDebugMenuRequest.Unregister(glob.player1RefAlias))
		{
			SPDLOG_DEBUG
			(
				"[GLOB] UnregisterEvents: Could not unregister player ref alias ({}) "
				"for OnDebugMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str()
			);
		}
		else
		{
			SPDLOG_DEBUG("[GLOB] UnregisterEvents: Unregistered OnDebugMenuRequest() event");
		}

		if (!glob.onSummoningMenuRequest.Unregister(glob.player1RefAlias))
		{
			SPDLOG_DEBUG
			(
				"[GLOB] UnregisterEvents: Could not unregister player ref alias ({}) "
				"for OnSummoningMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str()
			);
		}
		else
		{
			SPDLOG_DEBUG("[GLOB] UnregisterEvents: Unregistered OnSummoningMenuRequest() event");
		}
	}

	void GlobalCoopData::UpdateAllSerializedCompanionPlayerFIDKeys()
	{
		// This mod's load index might have changed between
		// the initial serialization load function call and this function call,
		// meaning that all the co-op companion players' serialized FID keys are invalid.
		// Update them here (must be called before starting co-op).

		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			// Co-op companion player actors.
			auto companion1 = 
			(
				dataHandler->LookupForm<RE::Actor>(0x22FD, GlobalCoopData::PLUGIN_NAME)
			);
			auto companion2 = 
			(
				dataHandler->LookupForm<RE::Actor>(0x22FE, GlobalCoopData::PLUGIN_NAME)
			);
			auto companion3 = 
			(
				dataHandler->LookupForm<RE::Actor>(0x22FF, GlobalCoopData::PLUGIN_NAME)
			);

			bool succ1 = GlobalCoopData::UpdateSerializedCompanionPlayerFIDKey(companion1);
			bool succ2 = GlobalCoopData::UpdateSerializedCompanionPlayerFIDKey(companion2);
			bool succ3 = GlobalCoopData::UpdateSerializedCompanionPlayerFIDKey(companion3);
			if (!succ1 || !succ2 || !succ3)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] ERR: UpdateAllSerializedCompanionPlayerFIDKeys: "
					"Failed to update serialized FID key for {}: {}, {}: {}, {}: {}.",
					companion1 ? companion1->GetName() : "NONE", !succ1,
					companion2 ? companion2->GetName() : "NONE", !succ2,
					companion3 ? companion3->GetName() : "NONE", !succ3
				);
			}
		}
	}

	bool GlobalCoopData::UpdateSerializedCompanionPlayerFIDKey(RE::Actor* a_playerActor)
	{
		// Update the given player's FID serialization key.

		auto dataHandler = RE::TESDataHandler::GetSingleton(); 
		if (!a_playerActor || !dataHandler) 
		{
			return false;
		}

		auto& glob = GetSingleton();
		// Serializable data:
		// Ensure that the actor's updated FID is used 
		// as the key for accessing their serializable data.
		// Extract the node and update its key 
		// if this mod's position has changed in the load order.
		std::vector<RE::FormID> keys;
		// Get current keys.
		for (auto& [k, _] : glob.serializablePlayerData)
		{
			keys.emplace_back(k);
		}

		for (const auto& formID : keys)
		{
			// Full FIDs do not match but the raw FIDs match.
			bool newActorFID = 
			(
				(formID != a_playerActor->formID) && 
				(formID & 0xFFFFFF) == (a_playerActor->formID & 0xFFFFFF)
			);
			if (!newActorFID)
			{
				continue;
			}

			SPDLOG_DEBUG
			(
				"[GLOB] UpdateSerializedCompanionPlayerFIDKey: "
				"{}'s FID went from 0x{:X} to 0x{:X}, "
				"inserting new FID key into serializable data now.",
				a_playerActor->GetName(), formID, a_playerActor->formID, newActorFID
			);

			auto node = glob.serializablePlayerData.extract(formID);
			node.key() = a_playerActor->formID;
			glob.serializablePlayerData.insert(std::move(node));
		}

		return glob.serializablePlayerData.contains(a_playerActor->formID);
	}

	void GlobalCoopData::YouDied(RE::Actor* a_deadPlayer)
	{
		// All players downed or dead. Perform cleanup and end the co-op session.
		// NOTE: P1 kill calls still fail at times.
		// One example being when all other players die 
		// while P1 is getting up after being revived.
		// Use the 'player.kill' console command 
		// or use the Debug Menu's 'Reset Equip State' option on P1
		// to properly end the co-op session, 
		// since P1 will remain paralyzed on the ground otherwise.

		auto& glob = GetSingleton();
		// Ignore if there are no living players or if the dead player is valid and not a player.
		if ((glob.livingPlayers == 0) || (a_deadPlayer && !IsCoopPlayer(a_deadPlayer)))
		{
			return;
		}

		// No more living players now, sorry.
		glob.livingPlayers = 0;
		RE::CreateMessage
		(
			"Your party was bested this time.\n"
			"One thread of fate severed, another thread spun.", 
			nullptr, 
			0, 
			4, 
			10, 
			"Ok",	//"Wow, that's a corny line",
			nullptr
		);

		SPDLOG_DEBUG("[GLOB] YouDied: All players downed or dead. Ending co-op session.");

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}
			
			// Ragdoll first.
			Util::PushActorAway(p->coopActor.get(), p->coopActor->data.location, -1.0f, true);

			// Make sure god mode is disabled for each player first; 
			// otherwise, they won't die below.
			if (p->isInGodMode) 
			{
				GlobalCoopData::ToggleGodModeForPlayer(p->controllerID, false);
				Util::StopEffectShader(p->coopActor.get(), glob.ghostFXShader);
			}

			// Revert any active transformation.
			if (p->isTransforming || p->isTransformed)
			{
				p->RevertTransformation();
			}

			auto actorBase = p->coopActor->GetActorBase(); 
			if (!actorBase)
			{
				continue;
			}
			
			// Reset essential flags before killing actor.
			Util::NativeFunctions::SetActorBaseDataFlag
			(
				actorBase, RE::ACTOR_BASE_DATA::Flag::kEssential, false
			);
			p->coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
			glob.player1RefAlias->SetEssential(false);

			// Kill calls fail on P1 at times, especially when the player dies in water,
			// and the game will not reload.
			// The kill console command appears to work more often when this happens, 
			// so as an extra layer of insurance,
			// run that command here.
			const auto scriptFactory = 
			(
				RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
			);
			const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
			if (script)
			{
				script->SetCommand("kill");
				script->CompileAndRun(p->coopActor.get());
				// Cleanup.
				delete script;
			}

			// Also run the other kill functions and set life state to dead.
			p->coopActor->KillImpl(p->coopActor.get(), FLT_MAX, true, false);
			p->coopActor->KillImmediate();
			p->coopActor->SetLifeState(RE::ACTOR_LIFE_STATE::kDead);
			
			auto currentHealth = p->coopActor->GetActorValue(RE::ActorValue::kHealth);
			// Set health to 0.
			if (currentHealth > 0.0f)
			{
				p->pam->ModifyAV(RE::ActorValue::kHealth, -currentHealth);
			}
			else
			{
				// Sometimes when the player's health is negative, 
				// the game does not consider them as dead and won't reload.
				// Set to 1 health and then reduce to 0 again to simulate the player dying again.
				p->pam->ModifyAV(RE::ActorValue::kHealth, 1.0f - currentHealth);
				p->pam->ModifyAV(RE::ActorValue::kHealth, -1.0f);
			}

			// And through all that... P1 is usually still not dead.
			SPDLOG_DEBUG("[GLOB] YouDied: {}: is dead: {}, health: {}. Essential flag: {}, {}.",
				p->coopActor->GetName(),
				p->coopActor->IsDead(),
				p->coopActor->GetActorValue(RE::ActorValue::kHealth),
				actorBase->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kEssential),
				p->coopActor->boolFlags.all(RE::Actor::BOOL_FLAGS::kEssential));
		}

		// Reset skill gain multiplier since there are no living players in the party now.
		ModifyXPPerSkillLevelMult(false);
		// Teardown the session afterward.
		TeardownCoopSession(true);

		// If all else STILL fails, and it usually does, as a final failsafe, 
		// reload the most recent save after a short period of time.
		// This is making me go insane.
		if (auto saveLoadManager = RE::BGSSaveLoadManager::GetSingleton(); saveLoadManager) 
		{
			std::jthread reloadTask
			(
				[]() 
				{
					const auto& glob = GlobalCoopData::GetSingleton();
					auto ui = RE::UI::GetSingleton();
					auto p1 = RE::PlayerCharacter::GetSingleton();
					auto saveLoadManager = RE::BGSSaveLoadManager::GetSingleton(); 
					// If players are still alive or any singletons are invalid, return early.
					if (glob.livingPlayers > 0 || !ui || !p1 || !saveLoadManager)
					{
						return;
					}
					
					// NOTE:
					// Commented out for now.
					// Actually kills P1, but directly modifyign P1's health AV may cause a crash.

					const float maxSecsToWait = 5.0f;
					float secsWaited = 0.0f;
					float secsSinceKillTask = 1.0f;
					SteadyClock::time_point loadWaitTP = SteadyClock::now();
					SteadyClock::time_point killTaskWaitTP = SteadyClock::now();
					// Wait at most 5 seconds without a loading screen opening
					// before loading the most recent save.
					// Does not matter if P1 is flagged as dead or not.
					while ((secsWaited < maxSecsToWait) && 
						   (!ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) && 
						   (!glob.loadingASave))
					{
						// Attempt to kill P1 and force a reload every second.
						if (secsSinceKillTask >= 1.0f)
						{
							killTaskWaitTP = SteadyClock::now();
							// Set health to 1 and then to 0 to trigger death.
							p1->RestoreActorValue
							(
								RE::ACTOR_VALUE_MODIFIER::kDamage, 
								RE::ActorValue::kHealth,
								1.0f - p1->GetActorValue(RE::ActorValue::kHealth)
							);
							p1->RestoreActorValue
							(
								RE::ACTOR_VALUE_MODIFIER::kDamage,
								RE::ActorValue::kHealth, 
								-1.0f
							);
						}
						
						secsWaited = Util::GetElapsedSeconds(loadWaitTP);
						secsSinceKillTask = Util::GetElapsedSeconds(killTaskWaitTP);
					}

					// Force a reload if P1 is still not dead and the Loading Menu has not opened.
					if (secsWaited >= maxSecsToWait) 
					{
						SPDLOG_DEBUG
						(
							"[GLOB EXT] ReloadTask: "
							"Loading most recent save game after {} seconds.", 
							secsWaited
						);
						saveLoadManager->LoadMostRecentSaveGame();
					}

					SPDLOG_DEBUG
					(
						"[GLOB EXT] ReloadTask: Now waiting for the game to reload the last save. "
						"Co-op session active: {}, p1 dead: {}, loading menu open: {}, {}.",
						glob.coopSessionActive,
						p1->IsDead(),
						ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME),
						glob.loadingASave
					);
				}
			);

			reloadTask.detach();
		}
		else
		{
			// Delayed async check to make sure P1 dies to trigger the LoadingMenu,
			// since the game still fails to reload at times.
			// P1 is set as killed above, but then is sometimes alive 
			// when checked later via console command (?).
			// Last ditch attempt to force a reload if the save manager isn't available.

			std::jthread killTask
			(
				[]() 
				{
					const auto& glob = GlobalCoopData::GetSingleton();
					auto ui = RE::UI::GetSingleton();
					auto p1 = RE::PlayerCharacter::GetSingleton();
					if (!ui || !p1 || glob.livingPlayers > 0)
					{
						return;
					}

					while ((!glob.coopSessionActive) && 
						   (!glob.loadingASave) && 
						   (!ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)))
					{
						// Our NotifyAnimationGraph() hook for P1 will attempt to kill P1 
						// once no other players are alive.
						p1->NotifyAnimationGraph("GetUpBegin");
						std::this_thread::sleep_for
						(
							std::chrono::seconds(static_cast<long long>(*g_deltaTimeRealTime))
						);
					}

					SPDLOG_DEBUG
					(
						"[GLOB EXT] KillTask: Waiting for P1 to die. "
						"Co-op session active: {}, loading a save: {}, loading menu open: {}. "
						"Full reset: {}, reset game: {}, reload content: {}.",
						glob.coopSessionActive,
						glob.loadingASave,
						ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME),
						RE::Main::GetSingleton()->fullReset, 
						RE::Main::GetSingleton()->resetGame,
						RE::Main::GetSingleton()->reloadContent
					);
				}
			);

			killTask.detach();
		}
	}

	void GlobalCoopData::CopyPlayerData(const std::unique_ptr<CopyPlayerDataRequestInfo>& a_info)
	{
		// Copy over player data from co-op player to P1.
		// What's copied is dependent on both the requested menu 
		// and if the menu is opening or closing.

		auto ui = RE::UI::GetSingleton(); 
		if (!ui)
		{
			return;
		}

		auto requestingPlayer = Util::GetActorPtrFromHandle(a_info->requestingPlayerHandle);
		if (!requestingPlayer)
		{
			return;
		}

		auto playerIndex = GetCoopPlayerIndex(requestingPlayer.get());
		if (playerIndex == -1)
		{
			return;
		}

		SPDLOG_DEBUG("[GLOB] CopyPlayerData: Request to copy player data for {} on {} of {}.",
			requestingPlayer->GetName(),
			a_info->shouldImport ? "opening" : "closing",
			a_info->menuName);

		auto& glob = GetSingleton();
		const auto menuNameHash = Hash(a_info->menuName);
		const auto& p = glob.coopPlayers[playerIndex];
		// Prevent saving when co-op player data is copied onto P1.
		// If one menu on the stack has its 'kAllowSaving' flag unset, saving is disabled,
		// so modify the HUD's allow saving flag, 
		// since it is almost always open during gameplay.
		if (a_info->shouldImport)
		{
			if (auto hud = ui->GetMenu<RE::HUDMenu>(); hud)
			{
				hud->menuFlags.reset(RE::UI_MENU_FLAGS::kAllowSaving);
			}
		}

		// Must have Maxsu2017's awesome 'Hero Menu Enhanced' mod installed:
		// https://www.nexusmods.com/enderalspecialedition/mods/563
		if (menuNameHash == Hash(ENHANCED_HERO_MENU))
		{
			if (a_info->shouldImport)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Enderal Hero Menu: Should copy over AVs and name."
				);
				if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import Name.");
					CopyOverActorBaseData
					(
						requestingPlayer.get(), a_info->shouldImport, true, false, false
					);
					glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kName);
				}

				if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import AVs.");
					CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kSkillsAndHMS);
				}
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Enderal Hero Menu: Should restore AVs and name."
				);
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Export Name.");
					CopyOverActorBaseData
					(
						requestingPlayer.get(), a_info->shouldImport, true, false, false
					);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kName);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Export AVs.");
					CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
				}
			}
		}
		else if (menuNameHash == Hash(RE::BarterMenu::MENU_NAME))
		{
			if (a_info->shouldImport)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Barter Menu: Should copy over inventory on import."
				);
				if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import Inventory.");
					CopyOverInventories(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kInventory);
				}
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Barter Menu: Should copy back inventory on export."
				);
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Export Inventory.");
					CopyOverInventories(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kInventory);
				}
			}
		}
		else if (menuNameHash == Hash(RE::ContainerMenu::MENU_NAME))
		{
			// Sync shared perks/skills before copying over/restoring both.
			SyncSharedPerks();
			SyncSharedSkillAVs();

			// Copy AVs, name, and perk list.
			if (a_info->shouldImport) 
			{
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Container Menu: "
					"Should copy over AVs, name, carryweight, and perk list."
				);
				if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kCarryWeight))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import Carryweight.");
					CopyOverActorBaseData
					(
						requestingPlayer.get(), a_info->shouldImport, false, false, true
					);
					glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kCarryWeight);
				}

				if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkList))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import Perk list.");
					CopyOverPerkLists(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kPerkList);
				}

				if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import AVs.");
					CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kSkillsAndHMS);
				}
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Container Menu: "
					"Should restore AVs, name, carryweight, and perk list."
				);
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kCarryWeight))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Export Carryweight.");
					CopyOverActorBaseData
					(
						requestingPlayer.get(), a_info->shouldImport, false, false, true
					);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kCarryWeight);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkList))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Export Perk List.");
					CopyOverPerkLists(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kPerkList);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Export AVs.");
					CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
				}
			}
		}
		else if (menuNameHash == Hash(RE::CraftingMenu::MENU_NAME))
		{
			// For now, the entire inventory is copied over to P1.
			// TODO: 
			// Don't copy the entire inventory every time.
			// Only copy categories of items dependent on 
			// the crafting menu's linked furniture type.
			if (a_info->shouldImport)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Crafting Menu: Should copy over inventory on import."
				);
				if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import Inventory.");
					CopyOverInventories(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kInventory);
				}
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Crafting Menu: Should copy back inventory on export."
				);
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Export Inventory.");
					CopyOverInventories(requestingPlayer.get(), a_info->shouldImport);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kInventory);
				}
			}
		}
		else if (menuNameHash == Hash(RE::FavoritesMenu::MENU_NAME))
		{
			if (a_info->shouldImport)
			{
				// Import this player's favorited forms before the menu opens.
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Favorites Menu: Should import {}'s favorites to P1.",
					requestingPlayer.get()->GetName()
				);
				if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kFavorites))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import Favorites to P1.");
					p->em->ImportCoopFavorites(false);
					glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kFavorites);
				}
			}
			else
			{
				// Revert changes to P1's favorites if the favorites menu is closing.
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Favorites Menu: "
					"Should remove {}'s favorites from P1 and re-favorite P1's cached favorites.", 
					requestingPlayer.get()->GetName()
				);
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kFavorites))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 Favorites.");
					p->em->RestoreP1Favorites(false);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kFavorites);
				}
			}
		}
		else if (menuNameHash == Hash(RE::MagicMenu::MENU_NAME))
		{
			if (a_info->shouldImport)
			{
				// Import this player's favorited magic before the menu opens.
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Magic Menu: Should import {}'s favorites to P1.", 
					requestingPlayer.get()->GetName()
				);
				if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kFavorites))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import Favorites to P1.");
					p->em->ImportCoopFavorites(true);
					glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kFavorites);
				}
			}
			else
			{
				// Revert changes to P1's magic favorites if the magic menu is closing.
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: Magic Menu: "
					"Should remove {}'s favorites from P1 and re-favorite P1's cached favorites.",
					requestingPlayer.get()->GetName()
				);
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kFavorites))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 Favorites.");
					p->em->RestoreP1Favorites(true);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kFavorites);
				}
			}
		}
		else if (menuNameHash == Hash(RE::StatsMenu::MENU_NAME))
		{
			// Adjust perk data and prepare for import/export on opening/closing of LevelUp menu.
			// Copying done in this subroutine.
			// Don't adjust perk data if Enderal is installed.
			if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
			{
				SPDLOG_DEBUG
				(
					"[GLOB] CopyPlayerData: "
					"Adjust perk data for {} before entering the Stats Menu.", 
					requestingPlayer->GetName()
				);
				AdjustPerkDataForCompanionPlayer(requestingPlayer.get(), a_info->shouldImport);
			}
		}
		else if (menuNameHash == Hash(RE::TrainingMenu::MENU_NAME))
		{
			// Dialogue NPC is trainer/vendor.
			bool isTrainer = false;
			if (a_info->assocForm)
			{
				auto asActor = a_info->assocForm->As<RE::Actor>(); 
				if (asActor && asActor->GetActorBase())
				{
					auto npcClass = asActor->GetActorBase()->npcClass; 
					if (npcClass && npcClass->data.maximumTrainingLevel != 0)
					{
						SPDLOG_DEBUG
						(
							"[GLOB] CopyPlayerData: Dialogue NPC {} is a trainer: {} to level {}.",
							asActor->GetName(), 
							*npcClass->data.teaches, 
							npcClass->data.maximumTrainingLevel
						);
						isTrainer = true;
					}
				}
			}

			if (isTrainer)
			{
				// Copy over AVs.
				if (a_info->shouldImport)
				{
					SPDLOG_DEBUG
					(
						"[GLOB] CopyPlayerData: Trainer: Should copy over AVs on import."
					);
					if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
					{
						SPDLOG_DEBUG("[GLOB] CopyPlayerData: Import AVs.");
						CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kSkillsAndHMS);
					}
				}
				else
				{
					SPDLOG_DEBUG
					(
						"[GLOB] CopyPlayerData: Trainer: Should copy back AVs on export."
					);
					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
					{
						SPDLOG_DEBUG("[GLOB] CopyPlayerData: Export AVs.");
						CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
					}
				}
			}
		}

		// Clear all copied data flags on export if no copyable-data menus are open.
		// All other menus besides the copy data request menu are closed.
		bool supportedMenusClosed = std::find_if
		(
			COPY_PLAYER_DATA_MENU_NAMES.begin(), COPY_PLAYER_DATA_MENU_NAMES.end(),
			[ui, &a_info](const std::string_view& a_menuName) 
			{
				return a_menuName != a_info->menuName && ui->IsMenuOpen(a_menuName);
			}
		) == COPY_PLAYER_DATA_MENU_NAMES.end();
		SPDLOG_DEBUG
		(
			"[GLOB] CopyPlayerData: Should import: {}, supported menus closed: {}.",
			a_info->shouldImport, supportedMenusClosed
		);
		// Failsafe if multiple menus close before a single copy-data export task is run here.
		// Ensure all P1's data is restored based off the previously-imported data types.
		if ((!a_info->shouldImport) && (supportedMenusClosed || !glob.coopSessionActive))
		{
			SPDLOG_DEBUG
			(
				"[GLOB] CopyPlayerData: All supported menus closed. "
				"Uncleared data types on export: 0x{:X}. Clearing now.", 
				*glob.copiedPlayerDataTypes
			);
			if (*glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone)
			{
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kCarryWeight))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 Carryweight.");
					CopyOverActorBaseData(requestingPlayer.get(), false, false, false, true);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kCarryWeight);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kFavorites))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 Favorites.");
					p->em->RestoreP1Favorites(menuNameHash == Hash(RE::MagicMenu::MENU_NAME));
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kFavorites);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 Inventory.");
					CopyOverInventories(requestingPlayer.get(), false);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kInventory);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 Name.");
					CopyOverActorBaseData(requestingPlayer.get(), false, true, false, false);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kName);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkList))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 Perk List.");
					CopyOverPerkLists(requestingPlayer.get(), false);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kPerkList);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkTree))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 Perk Tree.");
					CopyOverPerkTrees(requestingPlayer.get(), false);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kPerkTree);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kRaceName))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 Race Name.");
					CopyOverActorBaseData(requestingPlayer.get(), false, false, true, false);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kRaceName);
				}

				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
				{
					SPDLOG_DEBUG("[GLOB] CopyPlayerData: Restore P1 AVs.");
					CopyOverAVs(requestingPlayer.get(), false);
					glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
				}
			}

			glob.copiedPlayerDataTypes = CopyablePlayerDataTypes::kNone;
		}

		// Re-enable saving after P1's data is restored.
		if (!a_info->shouldImport)
		{
			if (auto hud = ui->GetMenu<RE::HUDMenu>(); hud)
			{
				hud->menuFlags.set(RE::UI_MENU_FLAGS::kAllowSaving);
			}
		}
	}

	void GlobalCoopData::CopyOverActorBaseData
	(
		RE::Actor* a_coopActor,
		const bool& a_shouldImport,
		bool&& a_name,
		bool&& a_raceName, 
		bool&& a_carryWeight
	)
	{
		// Import the give player's name/race name/carryweight to P1 
		// or restore previously saved values to P1.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor) 
		{
			return;
		}

		if (a_shouldImport)
		{
			if (a_name)
			{
				// Save P1 and companion player names
				// before setting P1's full name to the companion player's.
				RE::BSFixedString p1Name = p1->GetDisplayFullName();
				RE::BSFixedString coopCompanionName = a_coopActor->GetDisplayFullName();
				glob.p1ExchangeableData->name = p1Name;
				glob.coopCompanionExchangeableData->name = coopCompanionName;
				auto base = p1->GetObjectReference();
				auto fullName = base ? base->As<RE::TESFullName>() : nullptr; 
				if (fullName)
				{
					fullName->fullName = glob.coopCompanionExchangeableData->name;
				}
			}

			if (a_raceName)
			{
				// Save P1 and co-op player race names 
				// before setting P1's race name to the companion player's.
				std::string_view p1RaceName = p1->GetRace()->fullName;
				std::string_view coopCompanionRaceName = a_coopActor->GetRace()->fullName;
				glob.p1ExchangeableData->raceName = p1RaceName;
				glob.coopCompanionExchangeableData->raceName = coopCompanionRaceName;
				if (auto race = p1->GetRace(); race)
				{
					race->fullName = glob.coopCompanionExchangeableData->raceName;
				}
			}

			if (a_carryWeight) 
			{
				// Save carryweight AV and AV mods before swapping the two.
				glob.coopCompanionExchangeableData->carryWeightAVData = 
				{ 
					a_coopActor->GetActorValue(RE::ActorValue::kCarryWeight),
					a_coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight),
					a_coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight
					),
					a_coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight
					),
					a_coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight
					)
				};
				glob.p1ExchangeableData->carryWeightAVData = {
					p1->GetActorValue(RE::ActorValue::kCarryWeight),
					p1->GetBaseActorValue(RE::ActorValue::kCarryWeight),
					p1->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight
					),
					p1->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight
					),
					p1->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight
					)
				};

				p1->SetActorValue
				(
					RE::ActorValue::kCarryWeight, 
					glob.coopCompanionExchangeableData->carryWeightAVData[0]
				);
				p1->SetBaseActorValue
				(
					RE::ActorValue::kCarryWeight, 
					glob.coopCompanionExchangeableData->carryWeightAVData[1]
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, 
					RE::ActorValue::kCarryWeight, 
					glob.coopCompanionExchangeableData->carryWeightAVData[2] - 
					glob.p1ExchangeableData->carryWeightAVData[2]
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, 
					RE::ActorValue::kCarryWeight, 
					glob.coopCompanionExchangeableData->carryWeightAVData[3] - 
					glob.p1ExchangeableData->carryWeightAVData[3]
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, 
					RE::ActorValue::kCarryWeight, 
					glob.coopCompanionExchangeableData->carryWeightAVData[4] - 
					glob.p1ExchangeableData->carryWeightAVData[4]
				);
			}
		}
		else
		{
			// Restore full name and/or race name.
			if (a_name)
			{
				auto base = p1->GetObjectReference();
				auto fullName = base ? base->As<RE::TESFullName>() : nullptr; 
				if (fullName)
				{
					fullName->fullName = glob.p1ExchangeableData->name;
				}
			}

			if (a_raceName)
			{
				if (auto race = p1->GetRace(); race)
				{
					race->fullName = glob.p1ExchangeableData->raceName;
				}
			}

			if (a_carryWeight)
			{
				// Swap saved carryweight AV and AV mods.
				p1->SetActorValue
				(
					RE::ActorValue::kCarryWeight, glob.p1ExchangeableData->carryWeightAVData[0]
				);
				p1->SetBaseActorValue
				(
					RE::ActorValue::kCarryWeight, glob.p1ExchangeableData->carryWeightAVData[1]
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, 
					RE::ActorValue::kCarryWeight, 
					glob.p1ExchangeableData->carryWeightAVData[2] - 
					p1->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight
					)
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, 
					RE::ActorValue::kCarryWeight, 
					glob.p1ExchangeableData->carryWeightAVData[3] - 
					p1->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight
					)
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary,
					RE::ActorValue::kCarryWeight, 
					glob.p1ExchangeableData->carryWeightAVData[4] - 
					p1->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight
					)
				);
			}
		}
	}

	void GlobalCoopData::CopyOverAVs
	(
		RE::Actor* a_coopActor, const bool& a_shouldImport, const bool& a_shouldCopyChanges
	)
	{
		// Copy over actor values (HMS and skills) between the companion player and P1.
		// Can also copy over changed values back to the companion player,
		// in addition to restoring P1's saved values when a menu closes.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor)
		{
			return;
		}

		if (a_shouldImport)
		{
			// Save P1 AV and AV mods on entry.
			glob.p1ExchangeableData->hmsBaseAVs =
			{
				p1->GetBaseActorValue(RE::ActorValue::kHealth),
				p1->GetBaseActorValue(RE::ActorValue::kMagicka),
				p1->GetBaseActorValue(RE::ActorValue::kStamina)
			};

			glob.p1ExchangeableData->hmsAVs = 
			{
				p1->GetActorValue(RE::ActorValue::kHealth),
				p1->GetActorValue(RE::ActorValue::kMagicka),
				p1->GetActorValue(RE::ActorValue::kStamina)
			};

			std::array<float, 3> tempHealthMods
			{
				p1->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth
				),
				p1->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
				),
				p1->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth
				)
			};
			std::array<float, 3> tempMagickaMods = 
			{
				p1->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka
				),
				p1->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
				),
				p1->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
				)
			};
			std::array<float, 3> tempStaminaMods = 
			{
				p1->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina
				),
				p1->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina
				),
				p1->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina
				)
			};

			// Temporary (buff/debuff) and permanent (ex. modav) changes to the HMS actor values.
			glob.p1ExchangeableData->hmsAVMods = 
			{
				tempHealthMods, tempMagickaMods, tempStaminaMods
			};

			glob.coopCompanionExchangeableData->hmsBaseAVs = 
			{
				a_coopActor->GetBaseActorValue(RE::ActorValue::kHealth),
				a_coopActor->GetBaseActorValue(RE::ActorValue::kMagicka),
				a_coopActor->GetBaseActorValue(RE::ActorValue::kStamina)
			};

			glob.coopCompanionExchangeableData->hmsAVs = 
			{
				a_coopActor->GetActorValue(RE::ActorValue::kHealth),
				a_coopActor->GetActorValue(RE::ActorValue::kMagicka),
				a_coopActor->GetActorValue(RE::ActorValue::kStamina)
			};

			tempHealthMods =
			{
				a_coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth
				),
				a_coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
				),
				a_coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth
				)
			};
			tempMagickaMods = 
			{
				a_coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka
				),
				a_coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
				),
				a_coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
				)
			};
			tempStaminaMods = 
			{
				a_coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina
				),
				a_coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina
				),
				a_coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina
				)
			};

			glob.coopCompanionExchangeableData->hmsAVMods = 
			{
				tempHealthMods, tempMagickaMods, tempStaminaMods
			};

			SPDLOG_DEBUG("[GLOB] CopyOverAVs: Setting P1's Health base/normal AV to {}, {}.",
				glob.coopCompanionExchangeableData->hmsBaseAVs[0], 
				glob.coopCompanionExchangeableData->hmsAVs[0]);

			p1->SetActorValue
			(
				RE::ActorValue::kHealth, glob.coopCompanionExchangeableData->hmsAVs[0]
			);
			p1->SetBaseActorValue
			(
				RE::ActorValue::kHealth, glob.coopCompanionExchangeableData->hmsBaseAVs[0]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, 
				RE::ActorValue::kHealth, 
				glob.coopCompanionExchangeableData->hmsAVMods[0][1] - 
				glob.p1ExchangeableData->hmsAVMods[0][1]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent,
				RE::ActorValue::kHealth, 
				glob.coopCompanionExchangeableData->hmsAVMods[0][2] - 
				glob.p1ExchangeableData->hmsAVMods[0][2]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage, 
				RE::ActorValue::kHealth, 
				glob.coopCompanionExchangeableData->hmsAVMods[0][0] - 
				glob.p1ExchangeableData->hmsAVMods[0][0]
			);
			
			SPDLOG_DEBUG("[GLOB] CopyOverAVs: Setting P1's Magicka base/normal AV to {}, {}.",
				glob.coopCompanionExchangeableData->hmsBaseAVs[1], 
				glob.coopCompanionExchangeableData->hmsAVs[1]);

			p1->SetActorValue
			(
				RE::ActorValue::kMagicka, glob.coopCompanionExchangeableData->hmsAVs[1]
			);
			p1->SetBaseActorValue
			(
				RE::ActorValue::kMagicka, glob.coopCompanionExchangeableData->hmsBaseAVs[1]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, 
				RE::ActorValue::kMagicka,
				glob.coopCompanionExchangeableData->hmsAVMods[1][1] - 
				glob.p1ExchangeableData->hmsAVMods[1][1]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent, 
				RE::ActorValue::kMagicka, 
				glob.coopCompanionExchangeableData->hmsAVMods[1][2] - 
				glob.p1ExchangeableData->hmsAVMods[1][2]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage, 
				RE::ActorValue::kMagicka, 
				glob.coopCompanionExchangeableData->hmsAVMods[1][0] - 
				glob.p1ExchangeableData->hmsAVMods[1][0]
			);
			
			SPDLOG_DEBUG("[GLOB] CopyOverAVs: Setting P1's Stamina base/normal AV to {}, {}.",
				glob.coopCompanionExchangeableData->hmsBaseAVs[2],
				glob.coopCompanionExchangeableData->hmsAVs[2]);

			p1->SetActorValue
			(
				RE::ActorValue::kStamina, glob.coopCompanionExchangeableData->hmsAVs[2]
			);
			p1->SetBaseActorValue
			(
				RE::ActorValue::kStamina, glob.coopCompanionExchangeableData->hmsBaseAVs[2]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary,
				RE::ActorValue::kStamina,
				glob.coopCompanionExchangeableData->hmsAVMods[2][1] - 
				glob.p1ExchangeableData->hmsAVMods[2][1]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent, 
				RE::ActorValue::kStamina, 
				glob.coopCompanionExchangeableData->hmsAVMods[2][2] - 
				glob.p1ExchangeableData->hmsAVMods[2][2]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage,
				RE::ActorValue::kStamina,
				glob.coopCompanionExchangeableData->hmsAVMods[2][0] -
				glob.p1ExchangeableData->hmsAVMods[2][0]
			);

			// Skills next.
			auto currentAV = RE::ActorValue::kNone;
			for (auto i = 0; i < Skill::kTotal; ++i)
			{
				// Ignore shared skill AVs, since these are already synced.
				currentAV = SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
				if (SHARED_SKILL_AVS_SET.contains(currentAV))
				{
					continue;
				}

				glob.coopCompanionExchangeableData->skillAVs[i] = 
				(
					a_coopActor->GetBaseActorValue(currentAV)
				);
				glob.coopCompanionExchangeableData->skillAVMods[0][i] = 
				(
					a_coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, currentAV
					)
				);
				glob.coopCompanionExchangeableData->skillAVMods[1][i] = 
				(
					a_coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, currentAV
					)
				);
				glob.coopCompanionExchangeableData->skillAVMods[2][i] = 
				(
					a_coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, currentAV
					)
				);
				glob.p1ExchangeableData->skillAVs[i] = p1->GetBaseActorValue(currentAV);
				glob.p1ExchangeableData->skillAVMods[0][i] = 
				(
					p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, currentAV)
				);
				glob.p1ExchangeableData->skillAVMods[1][i] = 
				(
					p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, currentAV)
				);
				glob.p1ExchangeableData->skillAVMods[2][i] = 
				(
					p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, currentAV)
				);

				SPDLOG_DEBUG
				(
					"[GLOB] CopyOverAVs: Setting P1's {} base AV to {}, was {}. "
					"Setting temp modifiers to ({}, {}, {}), "
					"were ({}, {}, {}), diffs: ({}, {}, {}).",
					Util::GetActorValueName(currentAV), 
					glob.coopCompanionExchangeableData->skillAVs[i], 
					glob.p1ExchangeableData->skillAVs[i], 
					glob.coopCompanionExchangeableData->skillAVMods[0][i],
					glob.coopCompanionExchangeableData->skillAVMods[1][i],
					glob.coopCompanionExchangeableData->skillAVMods[2][i],
					glob.p1ExchangeableData->skillAVMods[0][i],
					glob.p1ExchangeableData->skillAVMods[1][i],
					glob.p1ExchangeableData->skillAVMods[2][i],
					glob.coopCompanionExchangeableData->skillAVMods[0][i] - 
					glob.p1ExchangeableData->skillAVMods[0][i],
					glob.coopCompanionExchangeableData->skillAVMods[1][i] -
					glob.p1ExchangeableData->skillAVMods[1][i],
					glob.coopCompanionExchangeableData->skillAVMods[2][i] - 
					glob.p1ExchangeableData->skillAVMods[2][i]
				);

				p1->SetBaseActorValue(currentAV, glob.coopCompanionExchangeableData->skillAVs[i]);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, 
					currentAV, 
					glob.coopCompanionExchangeableData->skillAVMods[1][i] - 
					glob.p1ExchangeableData->skillAVMods[1][i]
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, 
					currentAV, 
					glob.coopCompanionExchangeableData->skillAVMods[2][i] - 
					glob.p1ExchangeableData->skillAVMods[2][i]
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, 
					currentAV, 
					glob.coopCompanionExchangeableData->skillAVMods[0][i] -
					glob.p1ExchangeableData->skillAVMods[0][i]
				);
			}
		}
		else
		{
			std::vector<float> newExportedBaseAVs = 
			{
				p1->GetBaseActorValue(RE::ActorValue::kHealth),
				p1->GetBaseActorValue(RE::ActorValue::kMagicka),
				p1->GetBaseActorValue(RE::ActorValue::kStamina)
			};

			std::vector<float> newExportedAVs = 
			{
				p1->GetActorValue(RE::ActorValue::kHealth),
				p1->GetActorValue(RE::ActorValue::kMagicka),
				p1->GetActorValue(RE::ActorValue::kStamina)
			};

			// Restore saved P1 AVs, AV mods.
			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverAVs: Resetting P1's Health base/normal AV to {}, {}, "
				"{}'s base/normal AV to {}, {}.",
				glob.p1ExchangeableData->hmsBaseAVs[0], 
				glob.p1ExchangeableData->hmsAVs[0],
				a_coopActor->GetName(), 
				newExportedBaseAVs[0], 
				newExportedAVs[0]
			);

			p1->SetActorValue(RE::ActorValue::kHealth, glob.p1ExchangeableData->hmsAVs[0]);
			p1->SetBaseActorValue(RE::ActorValue::kHealth, glob.p1ExchangeableData->hmsBaseAVs[0]);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary,
				RE::ActorValue::kHealth, 
				glob.p1ExchangeableData->hmsAVMods[0][1] - 
				glob.coopCompanionExchangeableData->hmsAVMods[0][1]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent, 
				RE::ActorValue::kHealth,
				glob.p1ExchangeableData->hmsAVMods[0][2] -
				glob.coopCompanionExchangeableData->hmsAVMods[0][2]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage,
				RE::ActorValue::kHealth, 
				glob.p1ExchangeableData->hmsAVMods[0][0] -
				glob.coopCompanionExchangeableData->hmsAVMods[0][0]
			);

			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverAVs: Resetting P1's Magicka base/normal AV to {}, {}, "
				"{}'s base/normal AV to {}, {}.",
				glob.p1ExchangeableData->hmsBaseAVs[1],
				glob.p1ExchangeableData->hmsAVs[1],
				a_coopActor->GetName(), 
				newExportedBaseAVs[1],
				newExportedAVs[1]
			);

			p1->SetActorValue(RE::ActorValue::kMagicka, glob.p1ExchangeableData->hmsAVs[1]);
			p1->SetBaseActorValue
			(
				RE::ActorValue::kMagicka, glob.p1ExchangeableData->hmsBaseAVs[1]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, 
				RE::ActorValue::kMagicka, 
				glob.p1ExchangeableData->hmsAVMods[1][1] - 
				glob.coopCompanionExchangeableData->hmsAVMods[1][1]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent,
				RE::ActorValue::kMagicka, 
				glob.p1ExchangeableData->hmsAVMods[1][2] - 
				glob.coopCompanionExchangeableData->hmsAVMods[1][2]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage,
				RE::ActorValue::kMagicka, 
				glob.p1ExchangeableData->hmsAVMods[1][0] - 
				glob.coopCompanionExchangeableData->hmsAVMods[1][0]
			);

			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverAVs: Resetting P1's Stamina base/normal AV to {}, {}, "
				"{}'s base/normal AV to {}, {}.",
				glob.p1ExchangeableData->hmsBaseAVs[2],
				glob.p1ExchangeableData->hmsAVs[2],
				a_coopActor->GetName(), 
				newExportedBaseAVs[2], 
				newExportedAVs[2]
			);

			p1->SetActorValue(RE::ActorValue::kStamina, glob.p1ExchangeableData->hmsAVs[2]);
			p1->SetBaseActorValue
			(
				RE::ActorValue::kStamina, glob.p1ExchangeableData->hmsBaseAVs[2]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary,
				RE::ActorValue::kStamina, 
				glob.p1ExchangeableData->hmsAVMods[2][1] - 
				glob.coopCompanionExchangeableData->hmsAVMods[2][1]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent, 
				RE::ActorValue::kStamina, 
				glob.p1ExchangeableData->hmsAVMods[2][2] - 
				glob.coopCompanionExchangeableData->hmsAVMods[2][2]
			);
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage,
				RE::ActorValue::kStamina, 
				glob.p1ExchangeableData->hmsAVMods[2][0] - 
				glob.coopCompanionExchangeableData->hmsAVMods[2][0]
			);
			
			// Skills next.
			auto currentAV = RE::ActorValue::kNone;
			for (auto i = 0; i < Skill::kTotal; ++i)
			{
				currentAV = SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
				// Skip shared AVs, since they will not change.
				if (SHARED_SKILL_AVS_SET.contains(currentAV))
				{
					continue;
				}

				if (a_shouldCopyChanges)
				{
					float newAV = p1->GetBaseActorValue(currentAV);
					if (newAV == glob.coopCompanionExchangeableData->skillAVs[i])
					{
						a_coopActor->SetBaseActorValue(currentAV, newAV);
						if (glob.serializablePlayerData.contains(a_coopActor->formID))
						{
							auto& data = glob.serializablePlayerData[a_coopActor->formID];
							SPDLOG_DEBUG
							(
								"[GLOB] CopyOverAVs: {}'s {} skill inc went from {} to {}.",
								a_coopActor->GetName(), 
								Util::GetActorValueName(currentAV),
								data->skillLevelIncreasesList[i],
								data->skillLevelIncreasesList[i] + 
								newAV - 
								glob.coopCompanionExchangeableData->skillAVs[i]
							);
							data->skillLevelIncreasesList[i] += 
							(
								newAV - glob.coopCompanionExchangeableData->skillAVs[i]
							);
						}
					}
				}

				SPDLOG_DEBUG
				(
					"[GLOB] CopyOverAVs: Resetting P1's {} AV to {}, was {}. "
					"Setting temp modifiers back to ({}, {}, {}), "
					"were copied from {} as ({}, {}, {}), diffs: ({}, {}, {}).",
					Util::GetActorValueName(currentAV),
					glob.p1ExchangeableData->skillAVs[i],
					glob.coopCompanionExchangeableData->skillAVs[i],
					glob.p1ExchangeableData->skillAVMods[0][i],
					glob.p1ExchangeableData->skillAVMods[1][i],
					glob.p1ExchangeableData->skillAVMods[2][i],
					a_coopActor->GetName(),
					glob.coopCompanionExchangeableData->skillAVMods[0][i],
					glob.coopCompanionExchangeableData->skillAVMods[1][i],
					glob.coopCompanionExchangeableData->skillAVMods[2][i],
					glob.p1ExchangeableData->skillAVMods[0][i] - 
					glob.coopCompanionExchangeableData->skillAVMods[0][i],
					glob.p1ExchangeableData->skillAVMods[1][i] - 
					glob.coopCompanionExchangeableData->skillAVMods[1][i],
					glob.p1ExchangeableData->skillAVMods[2][i] -
					glob.coopCompanionExchangeableData->skillAVMods[2][i]
				);

				p1->SetBaseActorValue(currentAV, glob.p1ExchangeableData->skillAVs[i]);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, 
					currentAV, 
					glob.p1ExchangeableData->skillAVMods[1][i] - 
					glob.coopCompanionExchangeableData->skillAVMods[1][i]
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary,
					currentAV, 
					glob.p1ExchangeableData->skillAVMods[2][i] - 
					glob.coopCompanionExchangeableData->skillAVMods[2][i]
				);
				p1->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, 
					currentAV,
					glob.p1ExchangeableData->skillAVMods[0][i] - 
					glob.coopCompanionExchangeableData->skillAVMods[0][i]
				);
			}
		}
	}

	void GlobalCoopData::CopyOverInventories(RE::Actor* a_coopActor, const bool& a_shouldImport) 
	{
		// Exchange inventories temporarily between P1 and a companion player.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor)
		{
			return;
		}

		// IMPORTANT NOTE:
		// Adding the object to a companion player after removing it from P1, 
		// instead of removing the item from P1 directly to the companion player
		// does not flag the companion player's inventory as changed 
		// and does not trigger the game's equip calculations, 
		// which normally clear out the player's currently equipped gear
		// and force the equip manager to re-equip the companion player's hand forms.
		// Always remove then add any transferred items.

		int8_t pIndex = GetCoopPlayerIndex(a_coopActor->GetHandle());
		const auto& p = glob.coopPlayers[pIndex];
		auto transferP1InventoryToRefr = 
		[p1](RE::TESObjectREFR* a_toRefr) 
		{
			auto inventory = p1->GetInventory();
			for (const auto& [boundObj, entry] : inventory)
			{
				// Do not transfer equipped or party-wide items, such as gold.
				// Don't want to allow other players to sell P1's equipped items while bartering.
				if ((!boundObj || entry.first <= 0 || Util::IsPartyWideItem(boundObj)) || 
					(entry.second && entry.second->IsWorn()))
				{
					continue;
				}

				SPDLOG_DEBUG("[GLOB] CopyOverInventories: Moving x{} {} from P1 to {}.", 
					entry.first, boundObj->GetName(), a_toRefr->GetName());

				p1->RemoveItem
				(
					boundObj, entry.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
				);
				a_toRefr->AddObjectToContainer(boundObj, nullptr, entry.first, a_toRefr);
			}

			if (auto invChanges = p1->GetInventoryChanges(); invChanges && invChanges->entryList) 
			{
				auto invCounts = p1->GetInventoryCounts();
				for (auto invChangesEntry : *invChanges->entryList) 
				{
					// Invalid entry.
					if (!invChangesEntry || !invChangesEntry->object) 
					{
						continue;
					}

					auto obj = invChangesEntry->object;
					// Equipped by P1 or a party-wide item, so skip.
					if (Util::IsPartyWideItem(obj) || invChangesEntry->IsWorn()) 
					{
						continue;
					}

					auto count = invCounts.contains(obj) ? invCounts.at(obj) : 0;
					// Does not have one.
					if (count <= 0) 
					{
						continue;
					}

					SPDLOG_DEBUG("[GLOB] CopyOverInventories: Moving x{} {} from P1 to {}.", 
						count, obj->GetName(), a_toRefr->GetName());
					p1->RemoveItem(obj, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					a_toRefr->AddObjectToContainer(obj, nullptr, count, a_toRefr);
				}
			}
		};

		auto transferRefrInventoryToP1 =
		[p1](RE::TESObjectREFR* a_fromRefr)
		{
			auto inventory = a_fromRefr->GetInventory();
			for (auto& [boundObj, entry] : inventory)
			{
				// Do not transfer the companion player's equipped items to P1.
				if ((!boundObj || entry.first <= 0) || (entry.second && entry.second->IsWorn()))
				{
					continue;
				}
				
				SPDLOG_DEBUG("[GLOB] CopyOverInventories: Moving x{} {} from {} to P1.", 
					entry.first, boundObj->GetName(), a_fromRefr->GetName());	
				a_fromRefr->RemoveItem
				(
					boundObj, entry.first, RE::ITEM_REMOVE_REASON::kStoreInTeammate, nullptr, p1
				);
			}

			auto invChanges = a_fromRefr->GetInventoryChanges(); 
			if (invChanges && invChanges->entryList)
			{
				auto invCounts = a_fromRefr->GetInventoryCounts();
				for (auto invChangesEntry : *invChanges->entryList)
				{
					// Invalid entry.
					if (!invChangesEntry || !invChangesEntry->object)
					{
						continue;
					}

					auto obj = invChangesEntry->object;
					// Do not transfer the companion player's equipped items to P1.
					if (invChangesEntry->IsWorn())
					{
						continue;
					}

					auto count = invCounts.contains(obj) ? invCounts.at(obj) : 0;
					// Does not have one.
					if (count <= 0)
					{
						continue;
					}
					
					
					SPDLOG_DEBUG("[GLOB] CopyOverInventories: Moving x{} {} from {} to P1.", 
						count, obj->GetName(), a_fromRefr->GetName());
					a_fromRefr->RemoveItem
					(
						obj, count, RE::ITEM_REMOVE_REASON::kStoreInTeammate, nullptr, p1
					);
				}
			}
		};

		const auto& coopP1 = glob.coopPlayers[glob.player1CID];
		if (a_shouldImport)
		{
			auto p1StorageChestRefr = glob.coopInventoryChests[coopP1->playerID];
			if (!p1StorageChestRefr || !p1StorageChestRefr.get()) 
			{
				return;
			}

			// Use chest inventory as temporary storage for P1's inventory items. 
			// Clear it out first.
			if (auto chestInvChanges = p1StorageChestRefr->GetInventoryChanges(); chestInvChanges)
			{
				chestInvChanges->RemoveAllItems
				(
					p1StorageChestRefr.get(), nullptr, false, false, false
				);
			}

			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverInventories: IMPORT: Move all P1 items to storage chest."
			);
			
			// From P1 to storage chest.
			transferP1InventoryToRefr(p1StorageChestRefr.get());

			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverInventories: IMPORT: Move all co-op companion items to P1."
			);

			// From co-op player to P1.
			transferRefrInventoryToP1(a_coopActor);

			p1->OnArmorActorValueChanged();
			auto invChanges = p1->GetInventoryChanges();
			if (invChanges)
			{
				invChanges->armorWeight = invChanges->totalWeight;
				invChanges->totalWeight = -1.0f;
				p1->equippedWeight = -1.0f;
			}

			// Update Barter Menu list after swapping inventories.
			if (auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::BarterMenu::MENU_NAME))
			{
				ui->GetMenu<RE::BarterMenu>()->itemList->Update();
			}
		}
		else
		{
			auto p1StorageChestRefr = glob.coopInventoryChests[coopP1->playerID];
			if (!p1StorageChestRefr || !p1StorageChestRefr.get())
			{
				return;
			}

			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverInventories: EXPORT: Move all P1 items to co-op companion."
			);

			// Transfer P1's current items back to the companion player.
			transferP1InventoryToRefr(a_coopActor);

			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverInventories: EXPORT: Move all P1 items from storage chest to P1."
			);

			// Transfer P1's original items back to P1 via the storage chest.
			transferRefrInventoryToP1(p1StorageChestRefr.get());

			p1->OnArmorActorValueChanged();
			auto invChanges = p1->GetInventoryChanges();
			if (invChanges)
			{
				invChanges->armorWeight = invChanges->totalWeight;
				invChanges->totalWeight = -1.0f;
				p1->equippedWeight = -1.0f;
			}
		}

		p->em->EquipFists();
	}

	void GlobalCoopData::CopyOverPerkLists(RE::Actor* a_coopActor, const bool& a_shouldImport)
	{
		// Copy over all the given player's perks to P1
		// or copy back P1's saved perks to P1.
		// NOTE: 
		// Unlocked perks lists do NOT get modified and are simply imported or restored.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor)
		{
			return;
		}

		auto& p1SerializedData = glob.serializablePlayerData.at(p1->formID);
		auto& coopPlayerSerializedData = glob.serializablePlayerData.at(a_coopActor->formID);

		// Import:
		// First, add all perks unlocked by the companion player to P1.
		// Then remove all P1's non-shared perks 
		// that are also NOT unlocked by the companion player.
		// Export:
		// First, add all perks originally unlocked by P1.
		// Then remove all P1's non-shared perks that were not saved as unlocked.
		auto modifyP1PerksList =
		[p1, &a_shouldImport, &p1SerializedData, &coopPlayerSerializedData]
		(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_coopPlayer) 
		{
			if (!a_node)
			{
				return;
			}

			auto perk = a_node->perk;
			// Use the serialized unlocked perks lists for both players.
			const auto& p1UnlockedPerksSet = p1SerializedData->GetUnlockedPerksSet();
			const auto& coopPlayerUnlockedPerksSet = 
			(
				coopPlayerSerializedData->GetUnlockedPerksSet()
			);
			// Create stack of perks in this tree.
			// Add P1/companion player perks while populating this stack
			// to add them in order from lowest rank to highest.
			std::stack<RE::BGSPerk*> perkStack;
			uint32_t perkIndex = 0;
			while (perk)
			{
				if ((a_shouldImport && coopPlayerUnlockedPerksSet.contains(perk)) ||
					(!a_shouldImport && p1UnlockedPerksSet.contains(perk)))
				{
					Util::Player1AddPerk(perk, -1);
				}

				perkStack.push(perk);
				perk = perk->nextPerk;
				++perkIndex;
			}

			// Use created stack to remove perks from highest rank to lowest.
			while (!perkStack.empty())
			{
				// Don't remove shared skill perks from P1, 
				// since they should carry over after the companion player exits the menu.
				bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
				if (auto perkToRemove = perkStack.top(); perkToRemove && !shared)
				{
					// Remove all perks the companion player does not have on import,
					// and all perks P1 does not have on export.
					if ((a_shouldImport && !coopPlayerUnlockedPerksSet.contains(perkToRemove)) ||
						(!a_shouldImport && !p1UnlockedPerksSet.contains(perkToRemove))) 
					{
						Util::Player1RemovePerk(perkToRemove);
					}
				}

				perkStack.pop();
			}
		};

		Util::TraverseAllPerks(a_coopActor, modifyP1PerksList);
	}

	void GlobalCoopData::CopyOverPerkTrees(RE::Actor* a_coopActor, const bool& a_shouldImport)
	{
		// Copy over all companion player-unlocked perks from the game's vanilla perk tree, 
		// or restore P1's original perk tree perks.

		// NOTE: Unlocked perks lists DO get modified on import and restore.
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor)
		{
			return;
		}

		auto& p1SerializedData = glob.serializablePlayerData.at(p1->formID);
		auto& coopPlayerSerializedData = glob.serializablePlayerData.at(a_coopActor->formID);

		auto adjustSkillXP = 
		[p1, a_coopActor, &a_shouldImport, &glob, &p1SerializedData, &coopPlayerSerializedData]() 
		{
			if (a_shouldImport)
			{
				// Copy over skill XP, ignore shared skills.
				auto currentAV = RE::ActorValue::kNone;
				for (auto i = 0; i < Skill::kTotal; ++i)
				{
					currentAV = glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
					if (glob.SHARED_SKILL_AVS_SET.contains(currentAV))
					{
						continue;
					}

					SPDLOG_DEBUG
					(
						"[GLOB] CopyOverPerkTrees Import: AdjustSkillXP: Getting lock. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
					{
						std::unique_lock<std::mutex> lock(glob.p1SkillXPMutex);
						SPDLOG_DEBUG
						(
							"[GLOB] CopyOverPerkTrees Import: AdjustSkillXP: "
							"Lock obtained. (0x{:X})",
							std::hash<std::jthread::id>()(std::this_thread::get_id()));

						// Save P1's current XP, level, and level threshold for this skill 
						// and then import the companion player's corresponding data.

						// XP
						p1SerializedData->skillXPList[i] = p1->skills->data->skills[i].xp;
						p1->skills->data->skills[i].xp = coopPlayerSerializedData->skillXPList[i];

						// Level.
						p1SerializedData->skillLevelsOnMenuEntry[i] = 
						(
							p1->skills->data->skills[i].level
						);
						p1->skills->data->skills[i].level = 
						(
							coopPlayerSerializedData->skillBaseLevelsList[i] + 
							coopPlayerSerializedData->skillLevelIncreasesList[i]
						);

						// Level threshold.
						// Source: https://en.uesp.net/wiki/Skyrim:Leveling#Skill_XP
						// SkillLevelThreshold = ImproveMult * (CurrentLevel)^1.95 + ImproveOffset
						auto actorValueList = RE::ActorValueList::GetSingleton(); 
						if (actorValueList)
						{
							float skillCurveExp = 1.95f;
							auto valueOpt = Util::GetGameSettingFloat("fSkillUseCurve");
							if (valueOpt.has_value())
							{
								skillCurveExp = valueOpt.value();
							}

							auto avInfo = actorValueList->actorValues[!currentAV];
							const auto p1 = RE::PlayerCharacter::GetSingleton(); 
							if (avInfo && avInfo->skill)
							{
								auto avSkillInfo = avInfo->skill;
								float newThreshold = 
								(
									avSkillInfo->improveMult * 
									powf(p1->skills->data->skills[i].level, skillCurveExp) + 
									avSkillInfo->improveOffset
								);
								p1SerializedData->skillLevelThresholdsOnMenuEntry[i] = 
								(
									p1->skills->data->skills[i].levelThreshold	
								);
								p1->skills->data->skills[i].levelThreshold = newThreshold;
							}
						}

						SPDLOG_DEBUG
						(
							"[GLOB] CopyOverPerkTrees Import: AdjustSkillXP: "
							"Saved skill {}'s XP ({}) for P1. "
							"{}'s XP ({}) was imported. "
							"Level changed from {} to {}."
							"XP threshold changed from {} to {}.",
							Util::GetActorValueName
							(
								glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i))
							),
							p1SerializedData->skillXPList[i],
							a_coopActor->GetName(),
							coopPlayerSerializedData->skillXPList[i],
							p1SerializedData->skillLevelsOnMenuEntry[i],
							p1->skills->data->skills[i].level,
							p1SerializedData->skillLevelThresholdsOnMenuEntry[i],
							p1->skills->data->skills[i].levelThreshold
						);
					}
				}
			}
			else
			{
				// Restore skill XP, ignore shared skills.
				auto currentAV = RE::ActorValue::kNone;
				for (auto i = 0; i < Skill::kTotal; ++i)
				{
					currentAV = glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
					if (glob.SHARED_SKILL_AVS_SET.contains(currentAV))
					{
						continue;
					}

					SPDLOG_DEBUG
					(
						"[GLOB] CopyOverPerkTrees Export: AdjustSkillXP: Getting lock. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
					{
						std::unique_lock<std::mutex> lock(glob.p1SkillXPMutex);
						SPDLOG_DEBUG
						(
							"[GLOB] CopyOverPerkTrees Export: AdjustSkillXP: "
							"Lock obtained. (0x{:X})", 
							std::hash<std::jthread::id>()(std::this_thread::get_id())
						);

						// Restore the serializable XP value, level, and level threshold, 
						// all of which we cached on import.
						p1->skills->data->skills[i].xp = p1SerializedData->skillXPList[i];
						p1->skills->data->skills[i].level = 
						(
							p1SerializedData->skillLevelsOnMenuEntry[i]
						);
						p1->skills->data->skills[i].levelThreshold = 
						(
							p1SerializedData->skillLevelThresholdsOnMenuEntry[i]
						);
						SPDLOG_DEBUG
						(
							"[GLOB] CopyOverPerkTrees Export: AdjustSkillXP: "
							"P1's skill {}'s XP ({}), level ({}), "
							"and level threshold ({}) were restored.",
							Util::GetActorValueName
							(
								glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i))
							),
							p1->skills->data->skills[i].xp,
							p1->skills->data->skills[i].level,
							p1->skills->data->skills[i].levelThreshold
						);
					}
				}
			}
		};

		// REMOVE when done debugging.
		// For debug purposes only.
		auto checkPerkTree = 
		[p1, &a_shouldImport](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) 
		{
			if (!a_node)
			{
				return;
			}

			auto perk = a_node->perk;
			uint32_t perkIndex = 0;
			while (perk)
			{
				if (p1->HasPerk(perk) || Util::Player1PerkListHasPerk(perk)) 
				{
					SPDLOG_DEBUG
					(
						"[GLOB] CopyOverPerkTrees {}: CHECK: "
						"{} has perk #{} {} (0x{:X}) "
						"(assigned: {}, in singleton list: {})",
						a_shouldImport ? "Import" : "Export", 
						a_actor->GetName(), 
						perkIndex,
						perk->GetName(),
						perk->formID,
						p1->HasPerk(perk), 
						Util::Player1PerkListHasPerk(perk)
					);
				}

				perk = perk->nextPerk;
				++perkIndex;
			}
		};

		// Set unlocked perks for P1/companion player on import,
		// and for the companion player on export.
		auto setUnlockedPerks = 
		[p1, &glob, &a_shouldImport, &p1SerializedData, &coopPlayerSerializedData]
		(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_coopPlayer) 
		{
			if (!a_node)
			{
				return;
			}

			auto perk = a_node->perk;
			uint32_t perkIndex = 0;
			while (perk)
			{
				// Selected perks do not get added to the P1 glob list 
				// while the level up menu is open (?)
				// Have to use native func check here as a result.
				bool succ = false;
				bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
				bool nativeFuncP1HasPerk = p1->HasPerk(perk);
				bool nativeFuncCoopPlayerHasPerk = a_coopPlayer->HasPerk(perk);
				bool singletonListHasPerk = Util::Player1PerkListHasPerk(perk);
				if (nativeFuncP1HasPerk || singletonListHasPerk)
				{
					if (a_shouldImport) 
					{
						succ = p1SerializedData->InsertUnlockedPerk(perk);
						// Ensure P1 singleton perk list and actor perk list are in sync on import.
						if (nativeFuncP1HasPerk != singletonListHasPerk)
						{
							// I think I'm going insane, but sometimes the has-perk booleans 
							// are not equal in the comparison above 
							// but both print as 'true' below (???).
							SPDLOG_DEBUG
							(
								"[GLOB] ERR: CopyOverPerkTrees {}: {}: SetUnlockedPerks: "
								"Perk check inconsistency ({} != {}). Adding {} (0x{:X}).",
								a_shouldImport ? "Import" : "Export",
								p1->GetName(), 
								nativeFuncP1HasPerk,
								singletonListHasPerk,
								perk->GetName(), 
								perk->formID
							);
							Util::Player1AddPerk(perk, -1);
						}
					}
					else
					{
						// Save all unlocked perks 
						// to companion player's unlocked perks list on export.
						succ = coopPlayerSerializedData->InsertUnlockedPerk(perk);

						// Since the companion player may have unlocked new shared perks 
						// before exiting the Stats Menu, also add any new shared perks 
						// to P1's unlocked perks list, 
						// which should have remained untouched since importing.
						if (shared) 
						{
							p1SerializedData->InsertUnlockedPerk(perk);
						}
					}

					SPDLOG_DEBUG
					(
						"[GLOB] CopyOverPerkTrees {}: {}: SetUnlockedPerks: "
						"P1 has perk #{} {} (0x{:X}) "
						"(assigned: {}, in singleton list: {}). SUCC: {}.",
						a_shouldImport ? "Import" : "Export", 
						p1->GetName(),
						perkIndex, 
						perk->GetName(), 
						perk->formID,
						p1->HasPerk(perk), 
						Util::Player1PerkListHasPerk(perk),
						succ
					);

					// No duplicates!
					if (shared && succ)
					{
						// Increment on-import shared perks count if P1 has the perk.
						if (a_shouldImport)
						{
							++glob.importUnlockedSharedPerksCount;
						}
						else
						{
							// Increment on-export shared perks count 
							// if the perk is unlocked on menu exit.
							++glob.exportUnlockedSharedPerksCount;
						}
					}
				}
					
				// Add unlocked perk to companion player's unlocked perks list on entry.
				if (a_shouldImport && nativeFuncCoopPlayerHasPerk) 
				{
					SPDLOG_DEBUG
					(
						"[GLOB] CopyOverPerkTrees {}: {}: SetUnlockedPerks: "
						"Has perk #{} {} (0x{:X}).",
						a_shouldImport ? "Import" : "Export", 
						a_coopPlayer->GetName(),
						perkIndex, 
						perk->GetName(),
						perk->formID
					);

					succ = coopPlayerSerializedData->InsertUnlockedPerk(perk);
				}

				perk = perk->nextPerk;
				++perkIndex;
			}
		};

		// First, add all perks unlocked by the companion player to P1.
		// Then remove all P1's non-shared perks 
		// that are also NOT unlocked by the companion player.
		auto addCompanionPlayerPerksOnImport =
		[p1, &coopPlayerSerializedData](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_coopPlayer)
		{
			if (!a_node)
			{
				return;
			}

			auto perk = a_node->perk;
			const auto& coopPlayerUnlockedPerksSet = 
			(
				coopPlayerSerializedData->GetUnlockedPerksSet()
			);
			// Create stack of perks in this tree.
			// Add companion player perks while populating this stack
			// to add them in order from lowest rank to highest.
			std::stack<RE::BGSPerk*> perkStack;
			uint32_t perkIndex = 0;
			while (perk)
			{
				if (coopPlayerUnlockedPerksSet.contains(perk)) 
				{
					Util::Player1AddPerk(perk, -1);
				}

				perkStack.push(perk);
				perk = perk->nextPerk;
				++perkIndex;
			}

			// Use created stack to remove perks from highest rank to lowest.
			while (!perkStack.empty())
			{
				// Don't remove shared skill perks from P1, 
				// since they should carry over 
				// after the co-op companion player exits the Stats Menu.
				// Remove non-shared perks that the companion player has not unlocked
				// but P1 has unlocked.
				bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
				auto perkToRemove = perkStack.top(); 
				if (perkToRemove && !shared && !coopPlayerUnlockedPerksSet.contains(perkToRemove))
				{
					Util::Player1RemovePerk(perkToRemove);
				}

				perkStack.pop();
			}
		};

		// Add cached unlocked perks to P1 and the companion player on menu exit.
		// Remove any other perks.
		auto updatePlayerPerksOnExport =
		[p1, &p1SerializedData, &coopPlayerSerializedData]
		(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_coopPlayer)
		{
			if (!a_node)
			{
				return;
			}

			auto perk = a_node->perk;
			const auto& p1UnlockedPerksSet = p1SerializedData->GetUnlockedPerksSet();
			const auto& coopPlayerUnlockedPerksSet = 
			(
				coopPlayerSerializedData->GetUnlockedPerksSet()
			);
			// Create stack of perks in this tree.
			// Add perks to players while populating this stack
			// to add them in order from lowest rank to highest.
			std::stack<RE::BGSPerk*> perkStack;
			uint32_t perkIndex = 0;
			while (perk)
			{
				if (p1UnlockedPerksSet.contains(perk)) 
				{
					Util::Player1AddPerk(perk, -1);
				}

				if (coopPlayerUnlockedPerksSet.contains(perk)) 
				{
					Util::ChangePerk(a_coopPlayer, perk, true);
				}

				perkStack.push(perk);
				perk = perk->nextPerk;
				++perkIndex;
			}

			// Use created stack to remove perks from highest rank to lowest.
			while (!perkStack.empty())
			{
				// For both players, remove any shared perks that weren't saved as unlocked.
				if (auto perkToRemove = perkStack.top(); perkToRemove)
				{
					if (!p1UnlockedPerksSet.contains(perkToRemove)) 
					{
						Util::Player1RemovePerk(perkToRemove);
					}

					if (!coopPlayerUnlockedPerksSet.contains(perkToRemove)) 
					{
						Util::ChangePerk(a_coopPlayer, perkToRemove, false);
					}
				}

				perkStack.pop();
			}
		};

		if (a_shouldImport)
		{
			// Adjust skill XP first.
			adjustSkillXP();
			// Clear unlocked perks list before updating.
			p1SerializedData->ClearUnlockedPerks();
			coopPlayerSerializedData->ClearUnlockedPerks();
			// Reset on-import unlocked shared perks count.
			glob.importUnlockedSharedPerksCount = 0;
			// Set unlocked perks for both players 
			// and construct set of companion perks to import to P1.
			Util::TraverseAllPerks(a_coopActor, setUnlockedPerks);
			// Add companion player's perks to P1 and remove all other perks from P1.
			Util::TraverseAllPerks(a_coopActor, addCompanionPlayerPerksOnImport);

			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverPerkTrees Import: "
				"{} has {} unlocked perks, {} has {} unlocked perks. "
				"Shared perks count: {}.",
				p1->GetName(),
				p1SerializedData->GetUnlockedPerksList().size(),
				a_coopActor->GetName(),
				coopPlayerSerializedData->GetUnlockedPerksList().size(),
				glob.importUnlockedSharedPerksCount
			);

			// REMOVE when done debugging.
			Util::TraverseAllPerks(a_coopActor, checkPerkTree);
		}
		else
		{
			// REMOVE when done debugging.
			Util::TraverseAllPerks(a_coopActor, checkPerkTree);

			// Adjust skill XP first.
			adjustSkillXP();
			// Clear out old unlocked perks list for the companion player before updating.
			coopPlayerSerializedData->ClearUnlockedPerks();
			// Reset on-export unlocked shared perks count.
			glob.exportUnlockedSharedPerksCount = 0;
			// Set new unlocked perks list for the companion player.
			Util::TraverseAllPerks(a_coopActor, setUnlockedPerks);
			// Add back all P1's original perks cached when entering.
			Util::TraverseAllPerks(a_coopActor, updatePlayerPerksOnExport);

			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverPerkTrees Export: "
				"{} has {} unlocked perks, {} has {} unlocked perks. "
				"Shared perks count: {}.", 
				p1->GetName(),
				p1SerializedData->GetUnlockedPerksList().size(),
				a_coopActor->GetName(),
				coopPlayerSerializedData->GetUnlockedPerksList().size(),
				glob.exportUnlockedSharedPerksCount
			);

			// Update unlocked shared perks count for this player.
			coopPlayerSerializedData->sharedPerksUnlocked += max
			(
				0, 
				glob.exportUnlockedSharedPerksCount - glob.importUnlockedSharedPerksCount
			);

			SPDLOG_DEBUG
			(
				"[GLOB] CopyOverPerkTrees Export: {} has personally unlocked {} shared perks.",
				a_coopActor->GetName(), coopPlayerSerializedData->sharedPerksUnlocked
			);
		}
	}

	void GlobalCoopData::CopyOverCoopPlayerData
	(
		const bool a_shouldImport,
		const RE::BSFixedString a_menuName,
		RE::ActorHandle a_requestingPlayerHandle,
		RE::TESForm* a_assocForm
	)
	{
		// Construct a data copy request with the given info and then perform the request.

		SPDLOG_DEBUG
		(
			"[GLOB] CopyOverPlayerDataTask: {}: "
			"menu name: {}, requesting player: {}, associated form: {}",
			a_shouldImport ? "Import" : "Export", 
			a_menuName,
			Util::HandleIsValid(a_requestingPlayerHandle) ?
			a_requestingPlayerHandle.get()->GetName() : 
			"NONE",
			a_assocForm ? a_assocForm->GetName() : "NONE"
		);

		auto info = std::make_unique<CopyPlayerDataRequestInfo>
		(
			a_shouldImport, a_menuName, a_requestingPlayerHandle, a_assocForm
		);

		// Copy data here.
		CopyPlayerData(info);
	}

	void GlobalCoopData::PromptForPlayer1CIDTask()
	{
		// Debug option:
		// Assign linked controller ID for P1 via a prompt to press a certain button.
		// Workaround until finding direct way of accessing P1's controller's XInput index. 
		
		auto ui = RE::UI::GetSingleton();
		if (!ui)
		{
			return;
		}

		XINPUT_STATE inputState;
		ZeroMemory(&inputState, sizeof(XINPUT_STATE));
		uint8_t cid = 0;
		uint8_t activeControllers = 0;
		while (cid < ALYSLC_MAX_PLAYER_COUNT)
		{
			if (XInputGetState(cid, &inputState) == ERROR_SUCCESS)
			{
				++activeControllers;
			}

			++cid;
		}
		
		auto& glob = GetSingleton();
		std::array<bool, ALYSLC_MAX_PLAYER_COUNT> pauseBindPressed = 
		{
			false, false, false, false 
		};

		if (activeControllers <= 1) 
		{
			Util::AddSyncedTask
			(
				[]() 
				{ 
					RE::DebugMessageBox
					(
						"[ALYSLC] Please connect at least 2 controllers before starting co-op."
					); 
				}
			);

			return;
		}
		
		bool messageBoxOpen = false;
		bool requestToOpenMessagePrompt = false;
		bool shouldSetP1CID = false;
		float waitTime = 0.0f;
		while (!shouldSetP1CID && ui)
		{
			if (!requestToOpenMessagePrompt && !ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME))
			{
				requestToOpenMessagePrompt = true;
				Util::AddSyncedTask
				(
					[]() 
					{ 
						auto gSettings = RE::GameSettingCollection::GetSingleton();
						if (!gSettings)
						{
							return;
						}
						RE::CreateMessage
						(
							"[ALYSLC] Player 1: "
							"Please press the 'Pause' or 'Journal Menu' button "
							"to set your controller ID for co-op.", 
							nullptr,
							0, 
							4, 
							10, 
							gSettings->GetSetting("sBack")->GetString(),
							nullptr
						);
					}
				);			
			}
			else if (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME))
			{
				messageBoxOpen = true;
				auto userEvents = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				uint32_t pauseMask = GAMEPAD_MASK_START;
				if (userEvents && controlMap) 
				{
					pauseMask = controlMap->GetMappedKey
					(
						userEvents->journal, RE::INPUT_DEVICE::kGamepad
					);
				}

				cid = 0;
				while (cid < ALYSLC_MAX_PLAYER_COUNT)
				{
					if (XInputGetState(cid, &inputState) == ERROR_SUCCESS)
					{
						// Should set once pause bind is released.
						if ((inputState.Gamepad.wButtons & pauseMask) == pauseMask)
						{
							// Is pressed but not released yet.
							pauseBindPressed[cid] = true;
						}
						else if ((inputState.Gamepad.wButtons & pauseMask) == 0 && 
								 pauseBindPressed[cid])
						{
							// Set now since the bind is released.
							shouldSetP1CID = true;
							break;
						}
					}

					++cid;
				}

				if (shouldSetP1CID) 
				{
					std::this_thread::sleep_for(0.1s);
					auto msgQ = RE::UIMessageQueue::GetSingleton();
					if (msgQ)
					{
						// Close prompt messagebox.
						Util::AddSyncedTask
						(
							[msgQ]() 
							{
								msgQ->AddMessage
								(
									RE::MessageBoxMenu::MENU_NAME, 
									RE::UI_MESSAGE_TYPE::kForceHide, 
									nullptr
								); 
							}
						);
					}

					float waitSecs = 0.0f;
					while (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) && waitSecs < 2.0f)
					{
						std::this_thread::sleep_for(0.1s);
						waitSecs += 0.1f;
					}

					std::this_thread::sleep_for(0.1s);
					// Show result message box.
					Util::AddSyncedTask
					(
						[&cid]() 
						{ 
							RE::DebugMessageBox
							(
								fmt::format
								(
									"[ALYSLC] Player 1 has been assigned controller ID {}.", cid
								).c_str()
							); 
						}
					);
					glob.player1CID = cid;
				}
			}
			else
			{
				// Was already opened and now closed, so we can exit.
				if (messageBoxOpen)
				{
					break;
				}

				// Wait at most 5 seconds for the MessageBox menu to open.
				std::this_thread::sleep_for(0.1s);
				waitTime += 0.1f;
				if (waitTime > 5.0f)
				{
					break;
				}
			}
		}
	}

	void GlobalCoopData::ResetPlayer1AndCameraTask()
	{
		// Debug option:
		// Reset changes made to P1 and pause the co-op camera, 
		// reverting back to the default TP camera.

		auto& glob = GetSingleton();
		const auto& coopP1 = glob.coopPlayers[glob.player1CID];
		if (!coopP1->isActive)
		{
			return;
		}

		// Stop P1 managers.
		coopP1->RequestStateChange(ManagerState::kAwaitingRefresh);
		SteadyClock::time_point waitStartTP = SteadyClock::now();
		float secsWaited = 0.0f;
		// 1 second failsafe.
		while (coopP1->currentState != ManagerState::kAwaitingRefresh && secsWaited < 1.0f)
		{
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
			std::this_thread::sleep_for
			(
				std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f))
			);
		}

		// Stop camera manager.
		glob.cam->SetWaitForToggle(true);
		glob.cam->ToggleCoopCamera(false);
		waitStartTP = SteadyClock::now();
		secsWaited = 0.0f;
		while (glob.cam->currentState != ManagerState::kPaused && secsWaited < 1.0f)
		{
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
			std::this_thread::sleep_for
			(
				std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f))
			);
		}

		// Stop menu input manager.
		glob.mim->ToggleCoopPlayerMenuMode(-1);
		waitStartTP = SteadyClock::now();
		secsWaited = 0.0f;
		while (glob.cam->currentState != ManagerState::kPaused && secsWaited < 1.0f)
		{
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
			std::this_thread::sleep_for
			(
				std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f))
			);
		}
	}

	void GlobalCoopData::RespecPlayerTask(const int32_t a_controllerID)
	{
		// Prompt the player given by the CID to press the 'Start' button on their controller
		// to confirm their intentions to respec their character.
		// Then, reset their HMS AVs and perk data 
		// and remove all shared perks from all active players.

		if (a_controllerID <= -1 || a_controllerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}

		auto& glob = GetSingleton();
		const auto& p = glob.coopPlayers[a_controllerID];
		SPDLOG_DEBUG("[GLOB] RespecPlayerTask: {}.", p->coopActor->GetName());

		auto ui = RE::UI::GetSingleton();
		if (!ui)
		{
			return;
		}

		XINPUT_STATE inputState{ };
		bool confirmedRespec = false;
		bool listeningForPauseBindPress = true;
		bool messageBoxOpen = false;
		bool pauseBindPressed = false;
		bool requestToOpenMessagePrompt = false;
		float waitTime = 0.0f;
		while (!confirmedRespec && ui)
		{
			if (!requestToOpenMessagePrompt && !ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME))
			{
				requestToOpenMessagePrompt = true;
				Util::AddSyncedTask
				(
					[]() 
					{ 
						auto gSettings = RE::GameSettingCollection::GetSingleton();
						if (!gSettings)
						{
							return;
						}
						RE::CreateMessage
						(
							"[ALYSLC] Are you sure that you'd like to respec your character?\n\n"
							"Health, magicka, and stamina will be reset to their default values,\n"
							"and all unlocked perks will be removed for this player, "
							"along with all shared perks from all players.\n"
							"Any removed perks will have their perk points refunded.\n\n"
							"Please press the 'Pause' or 'Journal Menu' button to confirm.", 
							nullptr,
							0, 
							4, 
							10, 
							gSettings->GetSetting("sBack")->GetString(),
							nullptr
						);
					}
				);			
			}
			else if (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME))
			{
				messageBoxOpen = true;
				auto userEvents = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				uint32_t pauseMask = GAMEPAD_MASK_START;
				if (userEvents && controlMap) 
				{
					pauseMask = controlMap->GetMappedKey
					(
						userEvents->journal, RE::INPUT_DEVICE::kGamepad
					);
				}

				ZeroMemory(&inputState, sizeof(XINPUT_STATE));
				if (XInputGetState(a_controllerID, &inputState) == ERROR_SUCCESS)
				{
					// Should set once pause bind is released.
					if ((inputState.Gamepad.wButtons & pauseMask) == pauseMask)
					{
						// Is pressed but not released yet.
						pauseBindPressed = true;
					}
					else if ((inputState.Gamepad.wButtons & pauseMask) == 0 && pauseBindPressed)
					{
						SPDLOG_DEBUG
						(
							"[GLOB] RespecPlayerTask: Released pause bind, respec confirmed."
						);
						// Set now since the bind is released.
						confirmedRespec = true;
						break;
					}
				}
			}
			else
			{
				// Was already opened and now closed, so we can exit.
				if (messageBoxOpen)
				{
					break;
				}

				// Wait at most 5 seconds for the MessageBox menu to open.
				std::this_thread::sleep_for(0.1s);
				waitTime += 0.1f;
				if (waitTime > 5.0f)
				{
					break;
				}
				else
				{
					SPDLOG_DEBUG("[GLOB] RespecPlayerTask: Waiting ({}s).", waitTime);
				}
			}
		}
		
		std::this_thread::sleep_for(0.1s);
		auto msgQ = RE::UIMessageQueue::GetSingleton();
		if (msgQ)
		{
			// Close prompt messagebox.
			Util::AddSyncedTask
			(
				[msgQ]() 
				{
					msgQ->AddMessage
					(
						RE::MessageBoxMenu::MENU_NAME, 
						RE::UI_MESSAGE_TYPE::kForceHide, 
						nullptr
					); 
				}
			);
		}

		float waitSecs = 0.0f;
		while (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) && waitSecs < 2.0f)
		{
			std::this_thread::sleep_for(0.1s);
			waitSecs += 0.1f;
		}

		std::this_thread::sleep_for(0.1s);

		if (confirmedRespec) 
		{
			// Perform respec and show result message box.
			Util::AddSyncedTask
			(
				[&p]() 
				{ 
					GlobalCoopData::PerformPlayerRespec(p->coopActor.get());
					RE::DebugMessageBox
					(
						fmt::format
						(
							"[ALYSLC] Respec successful!\n"
							"Open up the perk tree to level up again and choose perks."
						).c_str()
					); 
				}
			);
		}
	}

	void GlobalCoopData::RestartCoopCameraTask()
	{
		// Debug option: 
		// Pause and then resume the co-op camera.

		auto& glob = GetSingleton();
		// Stop the co-op camera.
		glob.cam->ToggleCoopCamera(false);
		SteadyClock::time_point waitStartTP = SteadyClock::now();
		float secsWaited = 0.0f;
		// 1 second failsafe.
		while (glob.cam->currentState != ManagerState::kPaused && secsWaited < 1.0f)
		{
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
			std::this_thread::sleep_for
			(
				std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f))
			);
		}

		// Start the camera manager again.
		glob.cam->ToggleCoopCamera(true);
	}	

	void GlobalCoopData::RescaleHMS(RE::Actor* a_playerActor, const float& a_baseLevel)
	{
		// Rescale the player's health, magicka, and stamina AVs
		// to the serialized base values + increments.
		
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_playerActor || !glob.serializablePlayerData.contains(a_playerActor->formID)) 
		{
			return;
		}

		SPDLOG_DEBUG("[GLOB] RescaleHMS: {}: base level: {}.", 
			a_playerActor->GetName(), a_baseLevel);

		const auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
		/*
		if (a_playerActor == p1) 
		{
			// If P1 has leveled up at least once 
			// and has a recorded increase to their health, magicka, or stamina,
			// the serialized data is not the default saved data 
			// and the HMS AVs can be set to these saved values.
			bool useSerializedValues =
			(
				(a_playerActor->GetLevel() > 1) && 
				(
					data->hmsPointIncreasesList[0] > 0.0f || 
					data->hmsPointIncreasesList[1] > 0.0f || 
					data->hmsPointIncreasesList[2] > 0.0f
				)
			);
			if (useSerializedValues)
			{
				a_playerActor->SetBaseActorValue
				(
					RE::ActorValue::kHealth,
					data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0]
				);

				a_playerActor->SetBaseActorValue
				(
					RE::ActorValue::kMagicka, 
					data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1]
				);
				
				a_playerActor->SetBaseActorValue
				(
					RE::ActorValue::kStamina, 
					data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2]
				);

				SPDLOG_DEBUG
				(
					"[GLOB] RescaleHMS: Resetting P1's base HMS AVs to "
					"({}, {}, {}) -> ({} + {}, {} + {}, {} + {})",
					data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0],
					data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1],
					data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2],
					data->hmsBasePointsList[0], data->hmsPointIncreasesList[0],
					data->hmsBasePointsList[1], data->hmsPointIncreasesList[1],
					data->hmsBasePointsList[2], data->hmsPointIncreasesList[2]
				);
			}
		}
		else
		*/
		{
			// Has recorded level up.
			if (a_baseLevel != 0) 
			{
				a_playerActor->SetBaseActorValue
				(
					RE::ActorValue::kHealth, 
					data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0]
				);
				SPDLOG_DEBUG
				(
					"[GLOB] RescaleHMS: {}'s health AV at base level {} is {}. "
					"Health inc: {}, setting health to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[0],
					data->hmsPointIncreasesList[0],
					data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0]
				);

				a_playerActor->SetBaseActorValue
				(
					RE::ActorValue::kMagicka, 
					data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1]
				);
				SPDLOG_DEBUG
				(
					"[GLOB] RescaleHMS: {}'s magicka AV at base level {} is {}. "
					"Magicka inc: {}, setting magicka to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[1],
					data->hmsPointIncreasesList[1],
					data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1]
				);

				a_playerActor->SetBaseActorValue
				(
					RE::ActorValue::kStamina, 
					data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2]
				);
				SPDLOG_DEBUG
				(
					"[GLOB] RescaleHMS: {}'s stamina AV at base level {} is {}. "
					"Stamina inc: {}, setting stamina to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[2],
					data->hmsPointIncreasesList[2],
					data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2]
				);
			}
			else if (a_playerActor->GetRace() && a_playerActor->GetActorBase())
			{
				// Before first level up, use sum of the race's starting HMS AVs 
				// and the actor base's HMS offsets.
				data->hmsBasePointsList[0] = 
				(
					a_playerActor->race->data.startingHealth + 
					a_playerActor->GetActorBase()->actorData.healthOffset
				);
				data->hmsBasePointsList[1] =
				(
					a_playerActor->race->data.startingMagicka + 
					a_playerActor->GetActorBase()->actorData.magickaOffset
				);
				data->hmsBasePointsList[2] = 
				(
					a_playerActor->race->data.startingStamina +
					a_playerActor->GetActorBase()->actorData.staminaOffset
				);
				SPDLOG_DEBUG
				(
					"[GLOB] RescaleHMS: {} has not leveled up in co-op yet. "
					"Scaling HMS AVs down to their base values: {}, {}, {}.",
					a_playerActor->GetName(),
					data->hmsBasePointsList[0],
					data->hmsBasePointsList[1],
					data->hmsBasePointsList[2]
				);

				a_playerActor->SetBaseActorValue
				(
					RE::ActorValue::kHealth, data->hmsBasePointsList[0]
				);
				SPDLOG_DEBUG
				(
					"[GLOB] RescaleHMS: {}'s health AV at base level {} is {}. "
					"Health inc: {}, setting health to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[0],
					data->hmsPointIncreasesList[0],
					data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0]
				);

				a_playerActor->SetBaseActorValue
				(
					RE::ActorValue::kMagicka, data->hmsBasePointsList[1]
				);
				SPDLOG_DEBUG
				(
					"[GLOB] RescaleHMS: {}'s magicka AV at base level {} is {}. "
					"Magicka inc: {}, setting magicka to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[1],
					data->hmsPointIncreasesList[1],
					data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1]
				);

				a_playerActor->SetBaseActorValue
				(
					RE::ActorValue::kStamina, data->hmsBasePointsList[2]
				);
				SPDLOG_DEBUG
				(
					"[GLOB] RescaleHMS: {}'s stamina AV at base level {} is {}. "
					"Stamina inc: {}, setting stamina to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[2],
					data->hmsPointIncreasesList[2],
					data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2]
				);
			}
		}
	}

	void GlobalCoopData::RescaleSkillAVs(RE::Actor* a_playerActor)
	{
		// Rescale the player's skill AVs to the serialized base values + increments.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto& glob = GetSingleton();
		if (!p1 || !a_playerActor || !glob.serializablePlayerData.contains(a_playerActor->formID))
		{
			return;
		}
		
		SPDLOG_DEBUG("[GLOB] RescaleSkillAVs: {}.", a_playerActor->GetName());

		const auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
		Skill currentSkill = Skill::kTotal;
		RE::ActorValue currentAV = RE::ActorValue::kNone;
		for (auto i = 0; i < Skill::kTotal; ++i)
		{
			currentSkill = static_cast<Skill>(i);
			if (!SKILL_TO_AV_MAP.contains(currentSkill))
			{
				continue;
			}

			currentAV = glob.SKILL_TO_AV_MAP.at(currentSkill);
			SPDLOG_DEBUG
			(
				"[GLOB] RescaleSkillAVs: base: {}, current: {}, modifiers (d, p, t): {}, {}, {}.",
				a_playerActor->GetBaseActorValue(currentAV),
				a_playerActor->GetActorValue(currentAV),
				a_playerActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, currentAV),
				a_playerActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, currentAV
				),
				a_playerActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, currentAV
				)
			);

			if (SHARED_SKILL_AVS_SET.contains(currentAV))
			{
				// If shared, get the highest level for this AV among all co-op players, 
				// active or inactive, and set this player's AV to that level.
				auto value = GlobalCoopData::GetHighestSharedAVLevel(currentAV); 
				if (value != -1.0f)
				{
					// Since the base value is set directly and synced for each player,
					// we do not keep track of individual increases to shared AVs (set to 0).
					data->skillBaseLevelsList[i] = value;
					data->skillLevelIncreasesList[i] = 0.0f;
					a_playerActor->SetBaseActorValue(currentAV, value);
					SPDLOG_DEBUG("[GLOB] RescaleSkillAVs: Set {}'s SHARED skill AV {} to {}.",
						a_playerActor->GetName(), 
						Util::GetActorValueName(currentAV),
						value);
				}
			}
			else
			{
				// Add recorded skill increases on top of the serialized base skill level
				// to get new level for this skill.
				a_playerActor->SetBaseActorValue
				(
					currentAV, data->skillBaseLevelsList[i] + data->skillLevelIncreasesList[i]
				);
				SPDLOG_DEBUG
				(
					"[GLOB] RescaleSkillAVs: {}'s INDEP skill AV {} at base level {} is {} + {}. "
					"Set to {} ({}).",
					a_playerActor->GetName(),
					Util::GetActorValueName(currentAV),
					data->firstSavedLevel,
					data->skillBaseLevelsList[i],
					data->skillLevelIncreasesList[i],
					data->skillBaseLevelsList[i] + data->skillLevelIncreasesList[i],
					Settings::bStackCoopPlayerSkillAVAutoScaling ? 
					"AUTO-SCALING STACKS" : 
					"AUTO-SCALING DOES NOT STACK"
				);
			}
		}
	}

	void GlobalCoopData::ResetPerkData(RE::Actor* a_playerActor)
	{
		// Remove all perks from this player, remove shared perks from
		// all players, and reset all shared perk-related serialized data.
		// Any removed perks have their perk points refunded.
		// Done to allow the given player to fully respec and to prevent players 
		// from retaining perks that their new corresponding skill levels 
		// may no longer allow them to unlock if their base skill AVs have changed
		// (eg. perk requires level 50, but player's skill level decreased to 40).

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (!p1 || !a_playerActor || !glob.serializablePlayerData.contains(a_playerActor->formID))
		{
			return;
		}
		
		SPDLOG_DEBUG("[GLOB] ResetPerkData: {}", a_playerActor->GetName());

		auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
		// If P1 has not leveled up at least once, no player will have selected perks,
		// so return early.
		if (p1->GetLevel() == 1)
		{
			return;
		}

		// Players start with 3 perk points at level 1 if using Requiem.
		data->availablePerkPoints = static_cast<uint32_t>
		(
			(
				ALYSLC::RequiemCompat::g_requiemInstalled ? 
				p1->GetLevel() + 2 :
				p1->GetLevel() - 1
			) * 
			Settings::fPerkPointsPerLevelUp + 
			Settings::uFlatPerkPointsIncrease
		);
		data->extraPerkPoints = 0;
		data->prevTotalUnlockedPerks = 0;
		data->usedPerkPoints = 0;

		// Remove all shared perks for the passed in player if they aren't the respeccing player
		// and all perks for the respeccing player.
		// Save the set of removed shared perks for later.
		std::set<RE::BGSPerk*> perksRemoved{ };
		auto removePerks = 
		[p1, a_playerActor, &perksRemoved](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) 
		{
			if (!a_node)
			{
				return;
			}

			bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
			if (!shared && a_actor != a_playerActor) 
			{
				return;
			}

			auto perk = a_node->perk;
			// Must remove perks from highest rank to lowest,
			// so we build a stack for this tree,
			// which will result in traversing the tree
			// in the correct order when popping off the perks.
			std::stack<RE::BGSPerk*> perkStack;
			uint32_t perkIndex = 0;
			while (perk)
			{
				perkStack.push(perk);
				perk = perk->nextPerk;
				++perkIndex;
			}

			while (!perkStack.empty())
			{
				if (auto perkToRemove = perkStack.top(); perkToRemove)
				{
					if (a_actor == p1) 
					{
						bool succ = Util::Player1RemovePerk(perkToRemove);
						if (succ) 
						{
							SPDLOG_DEBUG
							(
								"[GLOB] ResetPerkData. "
								"Removed {} perk {} (0x{:X}) from p1's perks list.",
								shared ? "shared" : "unique",
								perkToRemove->GetName(), 
								perkToRemove->formID
							);
						}

						perksRemoved.insert(perkToRemove);
					}
					else
					{
						bool succ = Util::ChangePerk(a_actor, perkToRemove, false);
						if (succ) 
						{
							SPDLOG_DEBUG
							(
								"[GLOB] ResetPerkData. "
								"Removing {} perk {} (0x{:X}) from {}'s perks list.",
								shared ? "shared" : "unique",
								perkToRemove->GetName(), 
								perkToRemove->formID, a_actor->GetName()
							);
						}

						perksRemoved.insert(perkToRemove);
					}
				}

				perkStack.pop();
			}
		};

		RE::Actor* playerActor = nullptr;
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			for (auto& [fid, data] : glob.serializablePlayerData)
			{
				// P1 always has an FID of 0x14.
				if (fid == 0x14)
				{
					playerActor = p1;
				}
				else
				{
					playerActor = dataHandler->LookupForm<RE::Actor>
					(
						fid & 0x00FFFFFF, PLUGIN_NAME
					);
				}

				if (!playerActor)
				{
					continue;
				}

				// Remove perks as needed.
				perksRemoved.clear();
				Util::TraverseAllPerks(playerActor, removePerks);
				// New unlocked perk points list to set.
				std::vector<RE::BGSPerk*> newUnlockedPerks{ };
				if (fid == a_playerActor->formID)
				{
					SPDLOG_DEBUG
					(
						"[GLOB] ResetPerkData: "
						"{} has {} unlocked perks before perk removal. Will now have 0.", 
						playerActor->GetName(), data->GetUnlockedPerksList().size()
					);
					// Clear all of the respeccing player's serialized unlocked perks.
					// The respeccing player should have no perks at all after this iteration.
					data->ClearUnlockedPerks();
				}
				else
				{
					const auto& unlockedPerks = data->GetUnlockedPerksList();
					SPDLOG_DEBUG
					(
						"[GLOB] ResetPerkData: "
						"{} has {} unlocked perks before shared perk removal.", 
						playerActor->GetName(), unlockedPerks.size()
					);
					// No unlocked perks, so nothing to remove.
					if (unlockedPerks.size() == 0) 
					{
						continue;
					}
				
					// Construct new unlocked perks list with shared perks removed.
					for (auto perk : unlockedPerks)
					{
						if (perksRemoved.contains(perk))
						{
							continue;
						}
						
						newUnlockedPerks.emplace_back(perk);
					}
				}

				// Set the new perks list after removal of all or all shared perks.
				data->SetUnlockedPerks(newUnlockedPerks);
				// No player will have any shared perks.
				data->sharedPerksUnlocked = 0;
				SPDLOG_DEBUG
				(
					"[GLOB] ResetPerkData: "
					"{} now has {} unlocked perks after perk removal.", 
					playerActor->GetName(), data->GetUnlockedPerksList().size()
				);
			}
		}

		SPDLOG_DEBUG
		(
			"[GLOB] ResetPerkData: "
			"Adjust all players' perk counts after shared perk removal."
		);
		AdjustAllPlayerPerkCounts();
	}

	void GlobalCoopData::ResetToBaseHealthMagickaStamina(RE::Actor* a_playerActor)
	{
		// Resets the given player's health/magicka/stamina actor values to their initial values,
		// undoing all serialized progress to these AVs.
		
		auto& glob = GetSingleton();
		if (!a_playerActor || 
			!glob.globalDataInit || 
			!glob.serializablePlayerData.contains(a_playerActor->formID))
		{
			return;
		}
		
		SPDLOG_DEBUG("[GLOB] ResetToBaseHealthMagickaStamina: {}", a_playerActor->GetName());
		const auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
		data->hmsPointIncreasesList.fill(0.0f);
		RescaleHMS(a_playerActor, data->firstSavedLevel);
	}

	bool GlobalCoopData::TriggerAVAutoScaling(RE::Actor* a_playerActor, bool&& a_updateBaseAVs) 
	{
		// Force the game to scale all players' AVs by spoofing a level up 
		// and then de-leveling back to the original level.
		// Can optionally update the serialized base AVs for the given player(s)
		// after dipping to the first saved level or after returning to the original level.
		// Auto scale for all players if no player is given.
		// 
		// Preconditions:
		// 1. Player actor who changed classes is *gasp* actually a player actor,
		// or nullptr if all players must have their AVs auto-scaled (no class change),
		// 2. P1 is valid,
		// 3. Serializable data contains data for player actor, if one is given.
		// Returns true if successful or if no auto-scaling was necessary.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return false;
		}

		SPDLOG_DEBUG("[GLOB] TriggerAVAutoScaling: {}{}. Update base AVs: {}.", 
			a_playerActor ? "Player Changed Class/Race: " : "All Active Players",
			a_playerActor ? a_playerActor->GetName() : "",
			a_updateBaseAVs);

		// If not updating all player's base AV levels or stacking auto-scaling on top 
		// of all players' skill level increments, 
		// auto-scale all AVs and then set the new base AVs, if necessary.
		if (!a_updateBaseAVs || Settings::bStackCoopPlayerSkillAVAutoScaling)
		{
			// Just modify P1's level by 1, up or down,
			// and then reset to the original level to trigger auto-scaling.
			const auto scriptFactory = 
			(
				RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
			);
			const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
			if (!script)
			{
				return false;
			}

			auto p1Level = p1->GetLevel();
			uint16_t targetLevel = p1Level < UINT16_MAX ? p1Level + 1 : p1Level - 1;
			uint16_t savedLevel = p1Level;
			float savedXP = p1->skills->data->xp;

			SPDLOG_DEBUG
			(
				"[GLOB] TriggerAVAutoScaling: Before inc/dec: current XP, threshold: {}, {}, "
				"current level: {}, target level: {}.",
				p1->skills->data->xp, p1->skills->data->levelThreshold, p1Level, targetLevel
			);

			p1->skills->data->xp = 0.0f;
			// Set to target level.
			script->SetCommand("SetLevel " + std::to_string(targetLevel));
			script->CompileAndRun(p1);
			// Set to original level.
			script->SetCommand("SetLevel " + std::to_string(savedLevel));
			script->CompileAndRun(p1);
			// Restore XP.
			p1->skills->data->xp = savedXP;
			// Cleanup.
			delete script;

			SPDLOG_DEBUG
			(
				"[GLOB] TriggerAVAutoScaling: After inc/dec: current XP, threshold: {}, {}, "
				"current level: {}.",
				p1->skills->data->xp, p1->skills->data->levelThreshold, p1Level
			);

			// Update base AVs when playing Enderal,
			// since companion players' base AVs start at 5, instead of 15.
			if (a_updateBaseAVs || ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
			{
				SPDLOG_DEBUG
				(
					"[GLOB] TriggerAVAutoScaling: Update base AVs for all players. "
					"Enderal: {}, STACKED scaling: {}.",
					ALYSLC::EnderalCompat::g_enderalSSEInstalled, 
					Settings::bStackCoopPlayerSkillAVAutoScaling
				);

				// Set base Skill AV levels to newly-scaled ones.
				for (const auto& p : glob.coopPlayers)
				{
					if (!p->isActive || 
						!p->coopActor || 
						!glob.serializablePlayerData.contains(p->coopActor->formID))
					{
						continue;
					}

					auto& data = glob.serializablePlayerData.at(p->coopActor->formID);
					data->skillBaseLevelsList = Util::GetActorSkillLevels(p->coopActor.get());
				}
			}

			return true;
		}
		else
		{
			// Otherwise, set the given player/all players' base AVs 
			// to the auto-scaled levels at their first saved level.
			// The player(s)' skill level increments will then be applied 
			// on top of these new base AV levels,
			// instead of the auto-scaled levels at their current level.
			// 
			// KNOWN MAJOR ISSUE: 
			// The world sometimes does not scale down to the dip level in time
			// before the new base skill AV levels are set below,
			// meaning the current level's skill AVs will be set 
			// as the base skill AV levels instead.
			// This means that the skill level increments will not stack properly.
			// Temporary workaround which has its own issues: 
			// do not update the base skill levels if this occurs.

			auto autoScaleAndSetBaseAVs = 
			[p1, &glob](RE::Actor* a_playerActor) 
			{
				if (!a_playerActor)
				{
					return false;
				}

				const auto scriptFactory = 
				(
					RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
				);
				const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
				if (!script)
				{
					return false;
				}

				auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
				// Dip down to the player's first saved level and update base skill AVs, 
				// not the player's HMS AVs which will remain unchanged
				// and reflect the values that the player invested in them.
				// May add full respec option eventually.

				uint16_t savedP1Level = p1->GetLevel();
				// If P1 is still at level 1, move up one level to trigger auto-scaling instead.
				// Should not be 0, but due to my bad code, here's a failsafe.
				if (data->firstSavedLevel == 0)
				{
					data->firstSavedLevel = savedP1Level > 1 ? savedP1Level - 1 : savedP1Level;
						
					SPDLOG_DEBUG
					(
						"[GLOB] TriggerAVAutoScaling: "
						"First saved level for {} is 0. Set to {} now.", 
						a_playerActor->GetName(), data->firstSavedLevel
					);
				}
				
				
				// NOTE: First saved level is guaranteed to be >= 1 here.
				if (data->firstSavedLevel < savedP1Level)
				{
					// P1's level is >= 2.

					SPDLOG_DEBUG
					(
						"[GLOB] TriggerAVAutoScaling: Dip to update base skill AVs for {}. "
						"Assign to dipped-level base AVs list. "
						"Current level: {}, level to dip to: {}.",
						a_playerActor->GetName(), savedP1Level, data->firstSavedLevel
					);

					float savedXP = p1->skills->data->xp;
					p1->skills->data->xp = 0.0f;
					// Scale down.
					script->SetCommand("SetLevel " + std::to_string(data->firstSavedLevel));
					script->CompileAndRun(p1);
					
					// KNOWN MAJOR ISSUE: See above.
					if (auto newLevel = p1->GetLevel(); newLevel != data->firstSavedLevel)
					{
						SPDLOG_DEBUG
						(
							"[GLOB] ERR: TriggerAVAutoScaling: "
							"Dip level ({}) not reached before setting new base skill AVs for {}. "
							"Current level is {}. Not setting base skill actor values this time.",
							data->firstSavedLevel, a_playerActor->GetName(), newLevel
						);
					}
					else
					{
						// Update base skill AVs.
						SPDLOG_DEBUG
						(
							"[GLOB] TriggerAVAutoScaling: Dip level ({}) reached. "
							"Setting new base skill actor values for {}.",
							data->firstSavedLevel, a_playerActor->GetName()
						);
							
						data->skillBaseLevelsList = Util::GetActorSkillLevels(a_playerActor);
					}

					SPDLOG_DEBUG
					(
						"[GLOB] TriggerAVAutoScaling: Update base skill AVs for {}. "
						"After dip: current XP, threshold: {}, {}, "
						"current player levels: {}, {}, target level: {}.",
						a_playerActor->GetName(), 
						p1->skills->data->xp, 
						p1->skills->data->levelThreshold,
						p1->GetLevel(), 
						a_playerActor->GetLevel(),
						data->firstSavedLevel
					);
						
					// Restore original P1 level.
					script->SetCommand("SetLevel " + std::to_string(savedP1Level));
					script->CompileAndRun(p1);
					// Restore XP.
					p1->skills->data->xp = savedXP;

					SPDLOG_DEBUG
					(
						"[GLOB] TriggerAVAutoScaling: Update base skill AVs for {}. "
						"After dip: current XP, threshold: {}, {}, current player levels: {}, {}.",
						a_playerActor->GetName(),
						p1->skills->data->xp,
						p1->skills->data->levelThreshold,
						p1->GetLevel(),
						a_playerActor->GetLevel()
					);
				}
				else if (data->firstSavedLevel == savedP1Level)
				{
					// Player is at the same level as P1.
					// Scale up/down only one level to trigger auto-scaling.
					uint16_t targetLevel = 
					(
						savedP1Level < UINT16_MAX ? savedP1Level + 1 : savedP1Level - 1
					);
						
					SPDLOG_DEBUG
					(
						"[GLOB] TriggerAVAutoScaling: {}: "
						"Assign to current skill AVs after returning to saved P1 level: {}, "
						"first saved level: {}, target level: {}.",
						a_playerActor->GetName(), savedP1Level, data->firstSavedLevel, targetLevel
					);

					float savedXP = p1->skills->data->xp;
					p1->skills->data->xp = 0.0f;
					// Dip 1 level.
					script->SetCommand("SetLevel " + std::to_string(targetLevel));
					script->CompileAndRun(p1);
						
					SPDLOG_DEBUG
					(
						"[GLOB] TriggerAVAutoScaling: Update base skill AVs for {}. "
						"Before inc/dec: current XP, threshold: {}, {}, "
						"current level: {}, target level: {}.",
						a_playerActor->GetName(), 
						p1->skills->data->xp,
						p1->skills->data->levelThreshold, 
						p1->GetLevel(), 
						targetLevel
					);
						
					// Scale back up.
					script->SetCommand("SetLevel " + std::to_string(savedP1Level));
					script->CompileAndRun(p1);
					// Restore XP.
					p1->skills->data->xp = savedXP;

					SPDLOG_DEBUG
					(
						"[GLOB] TriggerAVAutoScaling: Update base skill AVs for {}. "
						"After inc/dec: current XP, threshold: {}, {}, current level: {}.",
						a_playerActor->GetName(),
						p1->skills->data->xp, 
						p1->skills->data->levelThreshold,
						p1->GetLevel()
					);

					// KNOWN MAJOR ISSUE: See above.
					if (auto newLevel = p1->GetLevel(); newLevel != savedP1Level)
					{
						SPDLOG_DEBUG
						(
							"[GLOB] ERR: TriggerAVAutoScaling: "
							"Original P1 level ({}) not reached before setting "
							"new base skill actor values for {}. Current level is {}. "
							"Not setting base skill actor values this time.",
							savedP1Level, a_playerActor->GetName(), newLevel
						);
					}
					else
					{
						SPDLOG_DEBUG
						(
							"[GLOB] TriggerAVAutoScaling: Original level ({}) reached. "
							"Setting base skill actor values for {}.",
							savedP1Level, a_playerActor->GetName()
						);
							
						// Update base skill AVs at the current level.
						// Should only differ from pre-dip levels 
						// if the given player changed their class.
						data->skillBaseLevelsList = Util::GetActorSkillLevels(a_playerActor);
					}
				}
				else
				{
					// Should never happen. But alert me if it does. Thanks.
					SPDLOG_ERROR
					(
						"[GLOB] ERR: TriggerAVAutoScaling: "
						"P1's level ({}) is below the target dip level ({}) for {}. "
						"Do not change base skill AVs.",
						savedP1Level, data->firstSavedLevel, a_playerActor->GetName()
					);
				}

				// Cleanup.
				delete script;
				return true;
			};

			if (a_playerActor)
			{
				bool succ = autoScaleAndSetBaseAVs(a_playerActor);
				if (!succ)
				{
					SPDLOG_DEBUG
					(
						"[GLOB] ERR: TriggerAVAutoScaling: "
						"Could not modify P1's level with console command. No rescaling possible."
					);
					return false;
				}

				// Reset all perks if the player's base AVs have changed.
				if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
				{
					// On class/race change, the player's base AVs have changed
					// and therefore their shared skill AV levels may not be high enough
					// for the currently unlocked shared perks, so reset all shared perks
					// for all players and also remove all perks for the given player,
					// allowing them to respec based on their new stats.
					ResetPerkData(a_playerActor);
				}
			}
			else
			{
				// Auto scale AVs for all companion players.
				for (const auto& p : glob.coopPlayers)
				{
					if (!p->isActive || p->isPlayer1)
					{
						continue;
					}

					autoScaleAndSetBaseAVs(p->coopActor.get());
				}
			}

			return true;
		}

		
	}

	//=============================================================================================

	void GlobalCoopData::ContactListener::ContactPointCallback
	(
		const RE::hkpContactPointEvent& a_event
	)
	{
		auto& glob = GetSingleton();
		if (!glob.coopSessionActive) 
		{
			return;
		}

		if (!a_event.bodies[0] || !a_event.bodies[1])
		{
			return;
		}

		if (!a_event.firstCallbackForFullManifold) 
		{
			return;
		}

		if (!a_event.contactPoint)
		{
			return;
		}

		// Find collidable and handle for each colliding body.
		auto collidableA = a_event.bodies[0]->GetCollidable();
		auto collidableB = a_event.bodies[1]->GetCollidable();
		RE::ObjectRefHandle handleA{};
		RE::ObjectRefHandle handleB{};
		if (!collidableA || !collidableB)
		{
			return;
		}

		auto refrA = RE::TESHavokUtilities::FindCollidableRef(*collidableA);
		auto refrB = RE::TESHavokUtilities::FindCollidableRef(*collidableB);
		if (refrA)
		{
			handleA = refrA->GetHandle();
		}

		if (refrB)
		{
			handleB = refrB->GetHandle();
		}

		bool oneRefrIsManaged = false;
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive || !p->IsRunning())
			{
				continue;
			}
			
			bool handlesValid = Util::HandleIsValid(handleA) && Util::HandleIsValid(handleB);
			if (handlesValid)
			{
				// Only one of the two colliding refrs is handled 
				// by a player's reference manipulation manager,
				// meaning it was dropped or thrown 
				// and hit another unmanaged object.
				bool refrAManaged = p->tm->rmm->IsManaged(handleA, false);
				bool refrBManaged = p->tm->rmm->IsManaged(handleB, false);
				oneRefrIsManaged = 
				(
					(refrAManaged && !refrBManaged) ||
					(!refrAManaged && refrBManaged)
				);
				if (!oneRefrIsManaged)
				{
					continue;
				}

				// Save the FIDs for the colliding refrs 
				// and queue this event for handling later 
				// by the player's reference manipulation manager.
				// Want to spend as little time in this callback as possible 
				// to prevent havok-related slowdowns.
				const auto fidPair = 
				(
					std::pair<RE::FormID, RE::FormID>
					(
						handleA.get().get()->formID, handleB.get().get()->formID
					)
				);

				SPDLOG_DEBUG("[GLOB] ContactPointCallback. {}: Getting lock. (0x{:X})", 
					p->coopActor->GetName(), 
					std::hash<std::jthread::id>()(std::this_thread::get_id()));
				{
					std::unique_lock<std::mutex> lock(p->tm->rmm->contactEventsQueueMutex);

					// Already collided, so do not queue this event for handling.
					if (p->tm->rmm->collidedRefrFIDPairs.contains(fidPair))
					{
						continue;
					}
				
					SPDLOG_DEBUG
					(
						"[GLOB] ContactPointCallback. {}: "
						"{} (0x{:X}) collided with {} (0x{:X}).", 
						p->coopActor->GetName(), 
						Util::HandleIsValid(handleA) ? handleA.get()->GetName() : "NONE",
						Util::HandleIsValid(handleA) ? handleA.get()->formID : 0xDEAD,
						Util::HandleIsValid(handleB) ? handleB.get()->GetName() : "NONE",
						Util::HandleIsValid(handleB) ? handleB.get()->formID : 0xDEAD
					);

					p->tm->rmm->collidedRefrFIDPairs.emplace(fidPair);			
					p->tm->rmm->queuedReleasedRefrContactEvents.emplace_back
					(
						std::make_unique<TargetingManager::HavokContactEventInfo>
						(
							handleA, 
							handleB,
							RE::hkRefPtr<RE::hkpRigidBody>(a_event.bodies[0]),
							RE::hkRefPtr<RE::hkpRigidBody>(a_event.bodies[1]),
							a_event.contactPoint->position,
							a_event.separatingVelocity ? *a_event.separatingVelocity : 0.0f
						)
					);
				}
			}
			else
			{
				// SPECIAL CASE:
				// If at least one handle for an object is not valid,
				// it means that object has no associated refr,
				// but we will still queue the event 
				// if the other object is a managed actor.
				// This will occur when a thrown actor hits a 3D object
				// like a terrain or navmesh block.
				// Will handle damage for the thrown actor 
				// in the player's reference manipuation manager.
				oneRefrIsManaged = 
				(
					(
						handleA && handleA.get() && handleA.get().get() && 
						handleA.get()->As<RE::Actor>() && 
						p->tm->rmm->IsManaged(handleA, false)
					) ||
					(
						handleB && handleB.get() && handleB.get().get() && 
						handleB.get()->As<RE::Actor>() && 
						p->tm->rmm->IsManaged(handleB, false)
					)
				);
				if (!oneRefrIsManaged)
				{
					continue;
				}

				auto collidingRefrPtr = 
				(
					Util::HandleIsValid(handleA) ?
					Util::GetRefrPtrFromHandle(handleA) :
					Util::GetRefrPtrFromHandle(handleB)
				);
				const auto fidPair = std::pair<RE::FormID, RE::FormID>
				(
					collidingRefrPtr->formID, 0x0
				);

				SPDLOG_DEBUG("[GLOB] ContactPointCallback. {}: Getting lock. (0x{:X})", 
					p->coopActor->GetName(), 
					std::hash<std::jthread::id>()(std::this_thread::get_id()));
				{
					std::unique_lock<std::mutex> lock(p->tm->rmm->contactEventsQueueMutex);

					// Already collided with an object without an associated refr,
					// so do not queue event.
					if (p->tm->rmm->collidedRefrFIDPairs.contains(fidPair))
					{
						continue;
					}
					
					SPDLOG_DEBUG
					(
						"[GLOB] ContactPointCallback. {}: "
						"{} (0x{:X}) collided with {} (0x{:X}).", 
						p->coopActor->GetName(), 
						Util::HandleIsValid(handleA) ? handleA.get()->GetName() : "NO REFR",
						Util::HandleIsValid(handleA) ? handleA.get()->formID : 0xDEAD,
						Util::HandleIsValid(handleB) ? handleB.get()->GetName() : "NO REFR",
						Util::HandleIsValid(handleB) ? handleB.get()->formID : 0xDEAD
					);

					p->tm->rmm->collidedRefrFIDPairs.emplace(fidPair);		
					p->tm->rmm->queuedReleasedRefrContactEvents.emplace_back
					(
						std::make_unique<TargetingManager::HavokContactEventInfo>
						(
							handleA, 
							handleB,
							RE::hkRefPtr<RE::hkpRigidBody>(a_event.bodies[0]),
							RE::hkRefPtr<RE::hkpRigidBody>(a_event.bodies[1]),
							a_event.contactPoint->position,
							a_event.separatingVelocity ? *a_event.separatingVelocity : 0.0f
						)
					);
				}
			}
		}
	}
}

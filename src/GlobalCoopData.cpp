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
		glob.supportedMenuOpen.store(false);
		glob.activePlayers = 0;
		glob.exportUnlockedSharedPerksCount = 0;
		glob.importUnlockedSharedPerksCount = 0;
		glob.livingPlayers = 0;
		glob.prevMenuCID = -1;
		glob.menuCID = -1;
		glob.p1SavedPerkCount = 0;
		glob.player1CID = -1;
		glob.quickLootReqCID = -1;
		glob.reqQuickLootContainerHandle = RE::ObjectRefHandle();
		// Time points.
		glob.lastCoopCompanionSkillLevelsCheckTP =
		glob.lastSupportedMenusClosedTP =
		glob.lastXPThresholdCheckTP = SteadyClock::now();
		// Set global entities and lists.
		glob.player1RefAlias = a_player1RefAlias;
		glob.player1Actor = RE::ActorPtr(RE::PlayerCharacter::GetSingleton());
		glob.coopEntityBlacklist.clear();
		glob.coopEntityBlacklistFIDSet.clear();
		glob.coopInventoryChests.clear();
		glob.coopPackages.clear();
		glob.coopPackageFormlists.clear();
		glob.castingGlobVars.clear();
		glob.placeholderSpells.clear();
		glob.placeholderSpellsSet.clear();
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			// Actors that are blacklisted from selection via targeting.
			glob.coopEntityBlacklist.emplace_back(RE::PlayerCharacter::GetSingleton());
			// Co-op companion player actors.
			glob.coopEntityBlacklist.emplace_back(dataHandler->LookupForm<RE::Actor>(0x22FD, PLUGIN_NAME));
			glob.coopEntityBlacklist.emplace_back(dataHandler->LookupForm<RE::Actor>(0x22FE, PLUGIN_NAME));
			glob.coopEntityBlacklist.emplace_back(dataHandler->LookupForm<RE::Actor>(0x22FF, PLUGIN_NAME));

			for (const auto& blacklistedActorPtr : glob.coopEntityBlacklist)
			{
				if (blacklistedActorPtr && blacklistedActorPtr.get())
				{
					glob.coopEntityBlacklistFIDSet.insert(blacklistedActorPtr->formID);
				}
				else
				{
					logger::error("[GLOB] ERR: InitializeGlobalCoopData: co-op entity blacklist actor is invalid. Cannot insert.");
				}
			}

			// One inventory chest per player.
			glob.coopInventoryChests.emplace_back(dataHandler->LookupForm<RE::TESObjectREFR>(0x13674, PLUGIN_NAME));
			glob.coopInventoryChests.emplace_back(dataHandler->LookupForm<RE::TESObjectREFR>(0x13675, PLUGIN_NAME));
			glob.coopInventoryChests.emplace_back(dataHandler->LookupForm<RE::TESObjectREFR>(0x13676, PLUGIN_NAME));
			glob.coopInventoryChests.emplace_back(dataHandler->LookupForm<RE::TESObjectREFR>(0x13677, PLUGIN_NAME));

			// Packages for co-op companion player actors.
			// (Default, combat override, ranged attack packages, special interaction) per player.
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4B4, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4B5, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4B3, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x9E17, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4B9, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4B6, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4BC, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x9E18, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4BA, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4B7, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4BD, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x9E19, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4BB, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4B8, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x4D4BE, PLUGIN_NAME));
			glob.coopPackages.emplace_back(dataHandler->LookupForm<RE::TESPackage>(0x9E1A, PLUGIN_NAME));

			// Package formlists for each player that hold the co-op packages above when they are added.
			// (Default, combat override) for each player.
			glob.coopPackageFormlists.emplace_back(dataHandler->LookupForm<RE::BGSListForm>(0xA509, PLUGIN_NAME));
			glob.coopPackageFormlists.emplace_back(dataHandler->LookupForm<RE::BGSListForm>(0x9FA3, PLUGIN_NAME));
			glob.coopPackageFormlists.emplace_back(dataHandler->LookupForm<RE::BGSListForm>(0x1B7DD, PLUGIN_NAME));
			glob.coopPackageFormlists.emplace_back(dataHandler->LookupForm<RE::BGSListForm>(0x1B7DC, PLUGIN_NAME));
			glob.coopPackageFormlists.emplace_back(dataHandler->LookupForm<RE::BGSListForm>(0x208D2, PLUGIN_NAME));
			glob.coopPackageFormlists.emplace_back(dataHandler->LookupForm<RE::BGSListForm>(0x208D3, PLUGIN_NAME));
			glob.coopPackageFormlists.emplace_back(dataHandler->LookupForm<RE::BGSListForm>(0x24ED3, PLUGIN_NAME));
			glob.coopPackageFormlists.emplace_back(dataHandler->LookupForm<RE::BGSListForm>(0x24ED2, PLUGIN_NAME));

			// Global variables that indicate whether a co-op companion player is trying to cast
			// a spell/shout using the LH, RH, 2H, dual, or voice slots.
			// Currently, dual casting is not functional.
			// Order: LH, RH, 2H, Dual, Shout, Voice (same as cast package indexing enum).
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x1E87F, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x1E883, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1D7, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x4D4AE, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1DB, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1DF, PLUGIN_NAME));

			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x1E880, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x1E884, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1D8, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x4D4AF, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1DC, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1E0, PLUGIN_NAME));

			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x1E881, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x1E885, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1D9, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x4D4B0, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1DD, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1E1, PLUGIN_NAME));

			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x25998, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x25999, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1DA, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x4D4B1, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1DE, PLUGIN_NAME));
			glob.castingGlobVars.emplace_back(dataHandler->LookupForm<RE::TESGlobal>(0x5B1E2, PLUGIN_NAME));

			// Other global variables.
			glob.summoningMenuOpenGlob = dataHandler->LookupForm<RE::TESGlobal>(0x11A76, PLUGIN_NAME);
			glob.werewolfTransformationGlob = dataHandler->LookupForm<RE::TESGlobal>(0x2EA9A, "Enderal - Forgotten Stories.esm");

			// Placeholder shouts that hold copied data from existing shouts.
			// Allows co-op companion player actors to cast different shouts through their ranged attack package.
			// For each player.
			glob.placeholderShouts.emplace_back(dataHandler->LookupForm<RE::TESShout>(0x5B1D3, PLUGIN_NAME));
			glob.placeholderShouts.emplace_back(dataHandler->LookupForm<RE::TESShout>(0x5B1D4, PLUGIN_NAME));
			glob.placeholderShouts.emplace_back(dataHandler->LookupForm<RE::TESShout>(0x5B1D5, PLUGIN_NAME));
			glob.placeholderShouts.emplace_back(dataHandler->LookupForm<RE::TESShout>(0x5B1D6, PLUGIN_NAME));

			// Placeholder spells that hold copied data from existing spells.
			// Allows co-op companion player actors to cast different spells through their ranged attack package.
			// Currently supported: (LH, RH) spells.
			// Order: (LH, RH, 2H, Voice) for each player.
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x1BD44, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x1BD48, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x58A2A, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x5B1CB, PLUGIN_NAME));

			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x1BD45, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x1BD4B, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x58A2C, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x5B1CD, PLUGIN_NAME));

			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x1BD4A, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x1BD4E, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x58A2E, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x5B1CF, PLUGIN_NAME));

			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x2599A, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x2599C, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x58A30, PLUGIN_NAME));
			glob.placeholderSpells.emplace_back(dataHandler->LookupForm<RE::SpellItem>(0x5B1D1, PLUGIN_NAME));

			for (auto spell : glob.placeholderSpells)
			{
				glob.placeholderSpellsSet.emplace(spell);
			}

			// Magic effects.
			glob.tarhielsGaleEffect = ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled ? dataHandler->LookupForm<RE::EffectSetting>(0x10C68, "Paragliding.esp") : nullptr;

			// Movement types.
			glob.paraglidingMT = ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled ? dataHandler->LookupForm<RE::BGSMovementType>(0x33D1, "Paragliding.esp") : nullptr;

			// Shaders.
			glob.activateHighlightShader = dataHandler->LookupForm<RE::TESEffectShader>(0x3262B, PLUGIN_NAME);
			glob.dragonHolesShader = RE::TESForm::LookupByID<RE::TESEffectShader>(0x4CEC8);
			glob.dragonSoulAbsorbShader = RE::TESForm::LookupByID<RE::TESEffectShader>(0x280C0);
			glob.ghostFXShader = RE::TESForm::LookupByID<RE::TESEffectShader>(0x64D67);

			// Spells.
			glob.tarhielsGaleSpell = ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled ? dataHandler->LookupForm<RE::SpellItem>(0x10C67, "Paragliding.esp") : nullptr;

			// Factions.
			glob.coopCompanionFaction = dataHandler->LookupForm<RE::TESFaction>(0x53AF5, PLUGIN_NAME);

			// REMOVE when done debugging.
			for (auto i = 0; i < glob.coopEntityBlacklist.size(); i++)
			{
				logger::debug("[GLOB] InitializeGlobalCoopData: Blacklisted actor in list at index {} has form ID: 0x{:X}", i,
					glob.coopEntityBlacklist[i] ? glob.coopEntityBlacklist[i]->GetFormID() : 0xDEAD);
			}
			for (const auto& fid : glob.coopEntityBlacklistFIDSet)
			{
				logger::debug("[GLOB] InitializeGlobalCoopData: Blacklisted actor in set has form ID: 0x{:X}", fid);
			}
			for (auto i = 0; i < glob.coopPackages.size(); i++)
			{
				logger::debug("[GLOB] InitializeGlobalCoopData: Package at index {} has form ID: 0x{:X}", i,
					glob.coopPackages[i] ? glob.coopPackages[i]->GetFormID() : 0xDEAD);
			}
			for (auto i = 0; i < glob.coopPackageFormlists.size(); i++)
			{
				logger::debug("[GLOB] InitializeGlobalCoopData: Package formlist at index {} has form ID: 0x{:X}", i,
					glob.coopPackageFormlists[i] ? glob.coopPackageFormlists[i]->GetFormID() : 0xDEAD);
			}
			for (auto i = 0; i < glob.castingGlobVars.size(); ++i)
			{
				logger::debug("[GLOB] InitializeGlobalCoopData: Glob var at index {} has form ID: 0x{:X}", i,
					glob.castingGlobVars[i] ? glob.castingGlobVars[i]->GetFormID() : 0xDEAD);
			}
			for (auto i = 0; i < glob.placeholderSpells.size(); ++i)
			{
				logger::debug("[GLOB] InitializeGlobalCoopData: Placeholder spell at index {} has form ID: 0x{:X}", i,
					glob.placeholderSpells[i] ? glob.placeholderSpells[i]->GetFormID() : 0xDEAD);
			}
			for (auto i = 0; i < glob.placeholderShouts.size(); ++i)
			{
				logger::debug("[GLOB] InitializeGlobalCoopData: Placeholder shout at index {} has form ID: 0x{:X}", i,
					glob.placeholderShouts[i] ? glob.placeholderShouts[i]->GetFormID() : 0xDEAD);
			}

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

		if (glob.bothHandsEquipSlot && glob.eitherHandEquipSlot && glob.leftHandEquipSlot && glob.rightHandEquipSlot && glob.voiceEquipSlot)
		{
			logger::debug("[GLOB] InitializeGlobalCoopData: Got all equip slots: 2H: 0x{:X}, Either: 0x{:X}, LH: 0x{:X}, RH: 0x{:X}, Voice: 0x{:X}.",
				glob.bothHandsEquipSlot->formID,
				glob.eitherHandEquipSlot->formID,
				glob.leftHandEquipSlot->formID,
				glob.rightHandEquipSlot->formID,
				glob.voiceEquipSlot->formID);
		}
		else
		{
			logger::critical("[GLOB] ERR: InitializeGlobalCoopData: Could not get all equip slots. Got both hands: {}, either hand: {}, left hand: {}, right hand: {}, voice: {}.",
				(bool)glob.bothHandsEquipSlot,
				(bool)glob.eitherHandEquipSlot,
				(bool)glob.leftHandEquipSlot,
				(bool)glob.rightHandEquipSlot,
				(bool)glob.voiceEquipSlot);
		}

		// NPC keyword.
		glob.npcKeyword = RE::TESForm::LookupByID<RE::BGSKeyword>(0x13794);
		// Get all weapon type (aside from Bound Arrow) keywords by ID.
		// Cannot insert by weapon_type since warhammer is not included as its own type.
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

		for (auto i = 0; i < glob.weapTypeKeywordsList.size(); ++i)
		{
			if (auto keyword = glob.weapTypeKeywordsList[i]; keyword)
			{
				logger::debug("[GLOB] InitializeGlobalCoopData: Got keyword: {} at index {}.", keyword->formEditorID, i);
			}
			else
			{
				logger::critical("[GLOB] InitializeGlobalCoopData: Could not get keyword at index: {}.", i);
			}
		}

		//=================
		// Base game forms.
		//=================
		// NEC = Not Enderal Compatible
		// Art Objects:
		glob.paraglideIndicatorEffect1 = RE::TESForm::LookupByEditorID<RE::BGSArtObject>("FXWispParticleAttachObject");
		glob.paraglideIndicatorEffect2 = RE::TESForm::LookupByEditorID<RE::BGSArtObject>("CallOfValorTargetFX01");
		glob.reviveDragonSoulEffect = RE::TESForm::LookupByID<RE::BGSArtObject>(0x2E6AA);
		glob.reviveHealingEffect = RE::TESForm::LookupByID<RE::BGSArtObject>(0x3F810);
		// Bound objects.
		// 1H dummy weapon for Enderal, 2H fists for Skyrim.
		glob.fists = RE::TESForm::LookupByID<RE::TESBoundObject>(0x1F4);
		if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
		{
			// Formlists:
			glob.shoutVarSpellsFormList = RE::TESForm::LookupByID<RE::BGSListForm>(0x167D9);  // NEC
			// Perks:
			glob.assassinsBladePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58211);	 // NEC
			glob.backstabPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58210);  // NEC
			glob.criticalChargePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0xCB406);
			glob.deadlyAimPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x1036F0);	 // NEC
			glob.dualCastingAlterationPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153CD);	// NEC
			glob.dualCastingConjurationPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153CE);	 // NEC
			glob.dualCastingDestructionPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153CF);	 // NEC
			glob.dualCastingIllusionPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153D0);	 // NEC
			glob.dualCastingRestorationPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x153D1);	 // NEC
			glob.greatCriticalChargePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0xCB407);
			glob.powerBashPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58F67);
			glob.quickShotPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x105F19);	 // NEC
			glob.shieldChargePerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x58F6A);  // NEC
			glob.sneakRollPerk = RE::TESForm::LookupByID<RE::BGSPerk>(0x105F23);
			// Globals:
			glob.craftingPointsGlob = nullptr;
			glob.learningPointsGlob = nullptr;
			glob.memoryPointsGlob = nullptr;
			glob.playerLevelGlob = nullptr;
			glob.werewolfTransformationGlob = nullptr;
		}
		else
		{
			glob.shoutVarSpellsFormList = nullptr;	 // NEC
			// Perks:
			glob.assassinsBladePerk = nullptr;	 // NEC
			glob.backstabPerk = nullptr;		 // NEC
			glob.criticalChargePerk = nullptr;
			glob.deadlyAimPerk = nullptr;				 // NEC
			glob.dualCastingAlterationPerk = nullptr;	 // NEC
			glob.dualCastingConjurationPerk = nullptr;	 // NEC
			glob.dualCastingDestructionPerk = nullptr;	 // NEC
			glob.dualCastingIllusionPerk = nullptr;	 // NEC
			glob.dualCastingRestorationPerk = nullptr;	 // NEC
			glob.greatCriticalChargePerk = nullptr;
			glob.powerBashPerk = nullptr;
			glob.quickShotPerk = nullptr;	   // NEC
			glob.shieldChargePerk = nullptr;  // NEC
			glob.sneakRollPerk = nullptr;
			// Globals:
			glob.craftingPointsGlob = RE::TESForm::LookupByEditorID<RE::TESGlobal>("Handwerkspunkte"sv);
			glob.learningPointsGlob = RE::TESForm::LookupByEditorID<RE::TESGlobal>("Lernpunkte"sv);
			glob.memoryPointsGlob = RE::TESForm::LookupByEditorID<RE::TESGlobal>("TalentPoints"sv);
			glob.playerLevelGlob = RE::TESForm::LookupByEditorID<RE::TESGlobal>("PlayerLevel"sv);

			if (glob.craftingPointsGlob)
			{
				glob.savedCraftingPoints = glob.craftingPointsGlob->value;
			}
			else
			{
				logger::error("[GLOB] ERR: InitializeGlobalCoopData: Could not get crafting points global.");
			}

			if (glob.learningPointsGlob)
			{
				glob.savedLearningPoints = glob.learningPointsGlob->value;
			}
			else
			{
				logger::error("[GLOB] ERR: InitializeGlobalCoopData: Could not get learning points global.");
			}

			if (glob.memoryPointsGlob)
			{
				glob.savedMemoryPoints = glob.memoryPointsGlob->value;
			}
			else
			{
				logger::error("[GLOB] ERR: InitializeGlobalCoopData: Could not get memory points global.");
			}
		}

		// Get all selectable level up perks.
		SELECTABLE_PERKS.clear();
		if (const auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
		{
			auto getSelectablePerks =
				[](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
					auto perk = a_node->perk;
					while (perk)
					{
						SELECTABLE_PERKS.insert(perk);
						perk = perk->nextPerk;
					}
				};

			Util::TraverseAllPerks(p1, getSelectablePerks);
		}

		// Assign killmoves.
		AssignSkeletonSpecificKillmoves();
		AssignGenericKillmoves();

		// Initialize managers, holders, and other global data members and wrap in smart pointers.
		glob.cam								= std::make_unique<CameraManager>();
		glob.cdh								= std::make_unique<ControllerDataHolder>();
		glob.mim								= std::make_unique<MenuInputManager>();
		glob.moarm								= std::make_unique<MenuOpeningActionRequestsManager>();
		glob.contactListener					= std::make_unique<ContactListener>();
		glob.copyDataReqInfo					= std::make_unique<CopyPlayerDataRequestInfo>();
		glob.coopCompanionExchangeableData		= std::make_unique<ExchangeablePlayerData>();
		glob.p1ExchangeableData					= std::make_unique<ExchangeablePlayerData>();
		glob.lastP1MeleeUseSkillCallArgs		= std::make_unique<LastP1MeleeUseSkillCallArgs>();
		glob.paFuncsHolder						= std::make_unique<PlayerActionFunctionsHolder>();
		glob.paInfoHolder						= std::make_unique<PlayerActionInfoHolder>();
		glob.taskRunner							= std::make_unique<TaskRunner>();

		// Register havok callback.
		if (auto precisionAPI1 = ALYSLC::PrecisionCompat::g_precisionAPI1; precisionAPI1)
		{
			logger::debug("[GLOB] InitializeGlobalCoopData: About to register Precision pre-physics step callback.");
			precisionAPI1->AddPrePhysicsStepCallback(SKSE::GetPluginHandle(), [](RE::bhkWorld* a_world) { GlobalCoopData::HavokPrePhysicsStep(a_world); });
			logger::debug("[GLOB] InitializeGlobalCoopData: Registered Precision pre-physics step callback.");
		}
		else
		{
			logger::critical("[GLOB] ERR: InitializeGlobalCoopData: Could not get Precision API to register Havok pre-physics step callback.");
		}

		// Create inactive co-op players.
		std::generate(glob.coopPlayers.begin(), glob.coopPlayers.end(), []() { return std::make_shared<CoopPlayer>(); });

		// Done initializing.
		glob.globalDataInit = true;
	}

	//===============================================================================================================================================

	void GlobalCoopData::AddSkillXP(const int32_t& a_cid, RE::ActorValue a_skillAV, const float& a_baseXP)
	{
		// Add skill XP for the co-op actor.
		// Precondition: co-op actor is not P1.
		// Source for leveling formulas: https://en.uesp.net/wiki/Skyrim:Leveling
		// 
		// XP to level up = Skill Improve Mult * (level-1)^1.95 + Skill Improve Offset, Cost(0) = 0
		// Skill XP awarded = Skill Use Mult * (base XP * skill specific multipliers) + Skill Use Offset
		// "skill specific multipliers" are not accounted for and left as 1.0.

		auto& glob = GetSingleton();
		// Enderal has no usage-based skill levelling.
		if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled && a_cid > -1) 
		{
			const auto& p = glob.coopPlayers[a_cid];
			if (auto actorValueList = RE::ActorValueList::GetSingleton(); actorValueList && p->coopActor && !glob.SHARED_SKILL_AVS_SET.contains(a_skillAV))
			{
				float skillCurveExp = 1.95f;
				auto valueOpt = Util::GetGameSettingFloat("fSkillUseCurve");
				if (valueOpt.has_value())
				{
					skillCurveExp = valueOpt.value();
				}

				auto avInfo = actorValueList->actorValues[!a_skillAV];
				if (const auto p1 = RE::PlayerCharacter::GetSingleton(); p1 && avInfo && avInfo->skill)
				{
					auto avSkillInfo = avInfo->skill;
					const auto& skill = glob.AV_TO_SKILL_MAP.at(a_skillAV);
					float xpInc = Settings::vfSkillXPMult[p->playerID] * (avSkillInfo->useMult * a_baseXP + avSkillInfo->offsetMult);

					// REMOVE
					logger::debug("[GLOB] AddSkillXP: {}: add skill XP for {}, base XP amount: {}, actual XP inc: {}.",
						p->coopActor->GetName(), Util::GetActorValueName(a_skillAV), a_baseXP, xpInc);
					const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
					logger::debug("[GLOB] AddSkillXP: {}: Lock: 0x{:X}.", p->coopActor->GetName(), hash);
					{
						std::unique_lock<std::mutex> skillXPLock(glob.skillXPMutexes[a_cid]);
						logger::debug("[GLOB] AddSkillXP: {}: Lock obtained: 0x{:X}.", p->coopActor->GetName(), hash);
						glob.serializablePlayerData.at(p->coopActor->formID)->skillXPList.at(skill) += xpInc;
					}
				}
			}
		}
	}

	void GlobalCoopData::AdjustAllPlayerPerkCounts()
	{
		logger::debug("[GLOB] AdjustAllPlayerPerkCounts");
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (!p1)
		{
			logger::error("[GLOB] ERR: AdjustAllPlayerPerkCounts: could not get player 1.");
			return;
		}

		// Adjust perk counts (used, available, extra, shared) for each player.
		// Used perk points = this player's total unlocked perks - total unlocked shared perks - extra perk points + this player's unlocked shared perks
		for (auto& [fid, data] : glob.serializablePlayerData)
		{
			const auto& unlockedPerksList = data->GetUnlockedPerksList();
			uint32_t maxPerkPointsFromLevel = p1->GetLevel() - 1;
			uint32_t totalUnlockedPerks = unlockedPerksList.size();
			RE::Actor* playerActor = nullptr;
			bool isP1 = fid == p1->formID;
			if (isP1)
			{
				playerActor = p1;
				logger::debug("[GLOB] AdjustAllPlayerPerkCounts: is player 1: {}, FID 0x{:X}, currentXP: {}, current level: {}, serialized number of unlocked perks: {}", 
					isP1, fid, p1->skills->data->xp, p1->GetLevel(), totalUnlockedPerks);

				// Ensure glob perk list matches the serialized one.
				for (auto i = 0; i < unlockedPerksList.size(); ++i)
				{
					auto perkToAdd = unlockedPerksList[i];
					bool alreadyAdded = std::any_of(p1->perks.begin(), p1->perks.end(),
						[p1, perkToAdd](RE::BGSPerk* a_perk) {
							return a_perk == perkToAdd;
						});

					if (!alreadyAdded)
					{
						Util::ChangeP1Perk(perkToAdd, true);
						//p1->perks.emplace_back(perkToAdd);
						logger::debug("[GLOB] AdjustAllPlayerPerkCounts. Re-adding {} to p1's perks list. New perk count: {}",
							perkToAdd->GetName(), p1->perks.size());
					}
				}

				totalUnlockedPerks = 0;
				for (auto perk : p1->perks)
				{
					if (glob.SELECTABLE_PERKS.contains(perk))
					{
						++totalUnlockedPerks;
					}
				}

				logger::debug("[GLOB] AdjustAllPlayerPerkCounts. Perk glob list gives total unlocked perks count of {}.", totalUnlockedPerks);
			}
			else
			{
				if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) 
				{
					if (auto playerForm = dataHandler->LookupForm(fid & 0x00FFFFFF, PLUGIN_NAME); playerForm && playerForm->As<RE::Actor>())
					{
						playerActor = playerForm->As<RE::Actor>();
					}
				}
			}

			if (playerActor)
			{
				const auto totalSharedPerksUnlocked = GetUnlockedSharedPerksCount();
				// Get unlocked shared perks count for player 1 by adding
				// all shared perk counts for each non-p1 player and then
				// subtracting this total from the total number of shared perks.
				auto coopCompanionSharedPerksUnlocked = 0;
				for (auto& [fid, data] : glob.serializablePlayerData)
				{
					if (fid != p1->formID)
					{
						if (data->sharedPerksUnlocked <= totalSharedPerksUnlocked) 
						{
							coopCompanionSharedPerksUnlocked += data->sharedPerksUnlocked;
						}
						else
						{
							// Error: this player has unlocked more shared perks than the saved total.
							// Clamp to total shared perks.
							logger::debug("[GLOB] ERR: AdjustAllPlayerPerkCounts: Player with FID 0x{:X} has {} unlocked shared perks on record, but the total is {}. Resetting to {}.",
								fid, data->sharedPerksUnlocked, totalSharedPerksUnlocked, totalSharedPerksUnlocked);
							data->sharedPerksUnlocked = totalSharedPerksUnlocked;
						}
					}
				}

				// Set number of shared perks unlocked for player 1 here.
				// Co-op companions' shared perk counts are updated after P1's perk tree is restored on menu exit.
				if (isP1)
				{
					data->sharedPerksUnlocked = max(0, static_cast<int32_t>(totalSharedPerksUnlocked - coopCompanionSharedPerksUnlocked));
				}

				// Extra perk points are points in excess with respect to how many points the player should have received from leveling
				// and from the shared perk points count:
				// Extra perk points = total unlocked perk count - number of shared perks NOT unlocked by this player - max unlocked perk count from leveling up.
				// Ensure never below 0, which can happen if the unlocked perks total hasn't been updated to reflect the current perk count state.
				// (eg. Perks added via console command outside of co-op. Please don't do this.)
				if (int32_t extraPerkPoints = totalUnlockedPerks - totalSharedPerksUnlocked + data->sharedPerksUnlocked - maxPerkPointsFromLevel; extraPerkPoints >= 0)
				{
					data->extraPerkPoints = extraPerkPoints;
					logger::debug("[GLOB] AdjustAllPlayerPerkCounts: {} has {} extra perks from external sources.",
						playerActor->GetName(), extraPerkPoints);
				}
				else
				{
					logger::debug("[GLOB] AdjustAllPlayerPerkCounts: {} has {} extra perks from external sources. Resetting to 0.",
						playerActor->GetName(), extraPerkPoints);
					data->extraPerkPoints = 0;
				}

				// Handle decreases in unlocked perks count during co-op.
				if (int32_t perkCountDec = data->prevTotalUnlockedPerks - totalUnlockedPerks; perkCountDec > 0)
				{
					data->extraPerkPoints = max(0, static_cast<int32_t>(data->extraPerkPoints - perkCountDec));
					logger::debug("[GLOB] AdjustAllPlayerPerkCounts: {} has {} extra perks after total perk count decrease of {} from {} to {}.",
						playerActor->GetName(), data->extraPerkPoints, perkCountDec, data->prevTotalUnlockedPerks, totalUnlockedPerks);
				}

				// Used perk points before clamp = total - extra - shared NOT unlocked by this player.
				int32_t rawUsedPerkPoints = totalUnlockedPerks - data->extraPerkPoints - totalSharedPerksUnlocked + data->sharedPerksUnlocked;
				// Clamp to [0, max total at level]
				data->usedPerkPoints = min(max(0, rawUsedPerkPoints), maxPerkPointsFromLevel);
				// Available = Max total for the current level - used total
				data->availablePerkPoints = max(0, maxPerkPointsFromLevel - data->usedPerkPoints);
				logger::debug("[GLOB] AdjustAllPlayerPerkCounts: {} has {}/{} unlocked perks, {} unlocked shared perks out of {} total unlocked ({} by co-op companions), max perk points from leveling at level {}: {}, extra perks: {}, for a total of {} used perk points. Result: {} available perk points.",
					playerActor->GetName(),
					unlockedPerksList.size(),
					totalUnlockedPerks,
					data->sharedPerksUnlocked,
					totalSharedPerksUnlocked,
					coopCompanionSharedPerksUnlocked,
					playerActor->GetLevel(),
					maxPerkPointsFromLevel,
					data->extraPerkPoints,
					data->usedPerkPoints,
					data->availablePerkPoints);
			}
			else
			{
				logger::error("[GLOB] ERR: AdjustAllPlayerPerkCounts: Could not get player form for FID 0xXX{:X}", fid & 0x00FFFFFF);
			}

			// Update previous count.
			data->prevTotalUnlockedPerks = data->GetUnlockedPerksList().size();
		}
	}

	void GlobalCoopData::AdjustBaseHMSData(RE::Actor* a_playerActor, const bool& a_shouldImport)
	{
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_playerActor) 
		{
			logger::debug("[GLOB] ERR: AdjustBaseHMSData: P1 is invalid: {}, player actor is invalid: {}.", (bool)!p1, (bool)!a_playerActor);
			return;
		}

		auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
		if (a_shouldImport)
		{
			// Save base HMS actor values on menu entry.
			// Co-op player AVs not imported to P1 yet.
			data->hmsBaseAVsOnMenuEntry[0] = a_playerActor->GetBaseActorValue(RE::ActorValue::kHealth);
			data->hmsBaseAVsOnMenuEntry[1] = a_playerActor->GetBaseActorValue(RE::ActorValue::kMagicka);
			data->hmsBaseAVsOnMenuEntry[2] = a_playerActor->GetBaseActorValue(RE::ActorValue::kStamina);
			logger::debug("[GLOB] AdjustBaseHMSData: {}'s base HMS values saved as {}, {}, {} ON ENTRY. First saved level: {}.",
				a_playerActor->GetName(),
				a_playerActor->GetBaseActorValue(RE::ActorValue::kHealth),
				a_playerActor->GetBaseActorValue(RE::ActorValue::kMagicka),
				a_playerActor->GetBaseActorValue(RE::ActorValue::kStamina),
				data->firstSavedLevel);
		}
		else
		{
			// Set co-op HMS skill increases based on p1's HMS AV changes in the Stats Menu.
			// Done before restoring P1's HMS values later.
			data->hmsPointIncreasesList[0] += p1->GetBaseActorValue(RE::ActorValue::kHealth) - data->hmsBaseAVsOnMenuEntry[0];
			data->hmsPointIncreasesList[1] += p1->GetBaseActorValue(RE::ActorValue::kMagicka) - data->hmsBaseAVsOnMenuEntry[1];
			data->hmsPointIncreasesList[2] += p1->GetBaseActorValue(RE::ActorValue::kStamina) - data->hmsBaseAVsOnMenuEntry[2];
			logger::debug("[GLOB] AdjustBaseHMSData: {}'s HMS AVs have increased by {}, {}, {} since initial leveling. {}, {}, {} since entering the level up menu.",
				a_playerActor->GetName(),
				data->hmsPointIncreasesList[0],
				data->hmsPointIncreasesList[1],
				data->hmsPointIncreasesList[2],
				p1->GetBaseActorValue(RE::ActorValue::kHealth) - data->hmsBaseAVsOnMenuEntry[0],
				p1->GetBaseActorValue(RE::ActorValue::kMagicka) - data->hmsBaseAVsOnMenuEntry[1],
				p1->GetBaseActorValue(RE::ActorValue::kStamina) - data->hmsBaseAVsOnMenuEntry[2]);
		}

		// Update serialized player level if it does not match the current one.
		if (const uint16_t currentLevel = a_playerActor->GetLevel(); currentLevel != data->level)
		{
			logger::debug("[GLOB] AdjustBaseHMSData: Levels do not match for {}: saved {} != current {}. Updating now.",
				a_playerActor->GetName(), data->level, currentLevel);
			data->level = currentLevel;
		}
	}

	// Source for leveling formulas: https://en.uesp.net/wiki/Skyrim:Leveling
	bool GlobalCoopData::AdjustInitialPlayer1PerkPoints(RE::Actor* a_playerActor)
	{
		// Adjust player 1's available perk points and trigger level up menus
		// as required to give the current player the number of perk points and level 
		// ups that they require.
		// Available perk points and modified with the P1 glob's perk points member.
		// Available level ups (opens LevelUp Menu) are modified by lowering P1's level 
		// temporarily by the requisite number of level ups and keeping XP constant.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_playerActor)
		{
			logger::debug("[GLOB] ERR: AdjustInitialPlayer1PerkPoints: P1 is invalid: {}, player actor is invalid: {}.", (bool)!p1, (bool)!a_playerActor);
			return false;
		}

		RE::FormID fid = {
			glob.serializablePlayerData.contains(a_playerActor->formID) ? a_playerActor->formID : 0
		};

		if (!fid) 
		{
			logger::debug("[GLOB] ERR: AdjustInitialPlayer1PerkPoints: Could not get serialized player FID for {}.", a_playerActor->GetName());
		}

		// Get HMS points increase per level up.
		uint32_t iAVDhmsLevelUp = 10;
		auto valueOpt = Util::GetGameSettingInt("iAVDhmsLevelUp");
		if (valueOpt.has_value())
		{
			iAVDhmsLevelUp = valueOpt.value();
		}

		const auto& data = glob.serializablePlayerData.at(fid);
		logger::debug("[GLOB] AdjustInitialPlayer1PerkPoints: {} (0x{:X}) entering perk point menu with {} available perk points. Perk points from player 1 glob: {}",
			a_playerActor->GetName(), fid, data->availablePerkPoints, p1->perkCount);

		// NOTE: This method of checking how many level ups the player has received will not work
		// if other mods are used that change the system by which player increase their HMS AVs.
		// Will also cause issues if modifying perks or HMS AVs while in the Stats Menu.
		// Get number of level ups used by dividing the sum of HMS increases by the number of points granted per level up.
		uint32_t hmsLevelUpsCount = 0;
		hmsLevelUpsCount = std::accumulate(data->hmsPointIncreasesList.begin(), data->hmsPointIncreasesList.end(), hmsLevelUpsCount) / iAVDhmsLevelUp;
		int32_t availableHMSLevelUps = max(0.0f, a_playerActor->GetLevel() - 1 - hmsLevelUpsCount);
		logger::debug("[GLOB] AdjustInitialPlayer1PerkPoints: {}'s level up count from HMS increases so far: {} (({} + {} + {}) / {}), level ups still available: {}. Available perk points: {}",
			a_playerActor->GetName(),
			hmsLevelUpsCount,
			data->hmsPointIncreasesList[0],
			data->hmsPointIncreasesList[1],
			data->hmsPointIncreasesList[2],
			iAVDhmsLevelUp,
			availableHMSLevelUps,
			data->availablePerkPoints);

		uint16_t playerLevel = p1->GetLevel();
		// Artificially drop P1's level (and consequently all active players' levels) to open the desired number of LevelUp menus.
		uint16_t targetDipLevel = playerLevel - availableHMSLevelUps;
		bool dipP1Level = targetDipLevel != playerLevel;

		// Adjust p1's glob perk points count and dip p1's level as necessary
		// to provide this player with the opportunity to level up their HMS actor values.
		// Also, add perk points without showing the HMS level up message box.
		if (availableHMSLevelUps == 0)
		{
			// No level ups, but ensure the player has the right number of perk points to spend.
			p1->perkCount = data->availablePerkPoints;
			logger::debug("[GLOB] AdjustInitialPlayer1PerkPoints: Enter: new, saved P1 perk count: {}, {}, num perks: {}",
				p1->perkCount, glob.p1SavedPerkCount, p1->perks.size());
		}
		else
		{
			// Additional perk points to add on top of the ones granted with each LevelUp Menu.
			int16_t perkPointsToAdd = data->availablePerkPoints - availableHMSLevelUps;
			logger::debug("[GLOB] AdjustInitialPlayer1PerkPoints: {} is attempting to access the level up menu, and has {} available perk points, with {} available HMS increase points. Adding {} perk points without HMS message box.",
				a_playerActor->GetName(), data->availablePerkPoints, availableHMSLevelUps, perkPointsToAdd);
			logger::debug("[GLOB] AdjustInitialPlayer1PerkPoints: New (added + from level ups): {}, ({} + {}), saved P1 perk count: {}, num perks: {}",
				perkPointsToAdd + playerLevel - targetDipLevel, perkPointsToAdd, playerLevel - targetDipLevel, glob.p1SavedPerkCount, p1->perks.size());

			p1->perkCount = perkPointsToAdd;
			if (dipP1Level) 
			{
				// Lower p1's level by the number of required level ups.
				const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
				if (const auto script = scriptFactory ? scriptFactory->Create() : nullptr; script)
				{
					float savedPlayerXP = p1->skills->data->xp;
					float savedPlayerXPThreshold = p1->skills->data->levelThreshold;

					logger::debug("[GLOB] AdjustInitialPlayer1PerkPoints: Before dip: current XP, threshold: {}, {}, current level: {}, target dip level to set: {}",
						savedPlayerXP, savedPlayerXPThreshold, playerLevel, targetDipLevel);

					float defMult = 25.0f;
					float defBase = 75.0f;
					float stepLevel = targetDipLevel;
					float thresholdAtLevel = (defBase + (defMult * stepLevel));
					float xpInc = 0.0f;
					while (stepLevel < playerLevel)
					{
						// Modify the level up threshold by the user-set threshold mult before adding up XP increments per level.
						thresholdAtLevel = Settings::fLevelUpXPThresholdMult * (defBase + (defMult * stepLevel));
						xpInc += thresholdAtLevel;
						// REMOVE
						logger::debug("[GLOB] AdjustInitialPlayer1PerkPoints: Step level {}: default threshold: {}, new threshold: {}, xpInc: {}.",
							stepLevel, (defBase + (defMult * stepLevel)), thresholdAtLevel, xpInc);
						++stepLevel;
					}

					// Set XP to pre-dip level + the XP increment needed to return to the pre-dip level from the post-dip level.
					// Since the XP level is over each level threshold from the post-dip level to the pre-dip level - 1,
					// the LevelUp menu will open the desired number of times.
					p1->skills->data->xp = savedPlayerXP + xpInc;
					script->SetCommand("SetLevel " + std::to_string(targetDipLevel));
					script->CompileAndRun(p1);
					delete script;

					logger::debug("[GLOB] AdjustInitialPlayer1PerkPoints: After dip: current XP, threshold: {}, {}, current level: {}, xpInc: {} from prev {}.",
						p1->skills->data->xp, p1->skills->data->levelThreshold, p1->GetLevel(), xpInc, savedPlayerXP);
				}
			}
		}

		return dipP1Level;
	}

	void GlobalCoopData::AdjustPerkDataForPlayer1(const bool& a_enteringMenu)
	{
		auto& glob = GetSingleton();
		const auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || glob.serializablePlayerData.empty()) 
		{
			logger::error("[GLOB] ERR: AdjustPerkDataForPlayer1: could not get player 1: {}, serialized data empty: {}.", 
				(bool)!p1, glob.serializablePlayerData.empty());
			return;
		}

		logger::debug("[GLOB] AdjustPerkDataForPlayer1: {} menu.", a_enteringMenu ? "Entering" : "Exiting");
		// Save unlocked perks for P1 first before adjustments are made.
		SaveUnlockedPerksForPlayer(p1);
		if (a_enteringMenu)
		{
			// Adjust perk counts before potentially copying data to P1.
			AdjustAllPlayerPerkCounts();
			AdjustBaseHMSData(p1, a_enteringMenu);

			// If player 1's level was lowered to spawn LevelUp menus, rescale co-op companions' AVs,
			// since the game will have auto-scaled them when P1's level changes.
			bool rescaleSkillAVsOnP1LevelDip = AdjustInitialPlayer1PerkPoints(p1);
			if (rescaleSkillAVsOnP1LevelDip)
			{
				logger::debug("[GLOB] AdjustPerkDataForPlayer1: About to rescale all companions' AVs after dipping P1's level to spawn level up menus.");
				RescaleActivePlayerAVs();
			}
		}
		else
		{
			// Save HMS changes for P1.
			AdjustBaseHMSData(p1, a_enteringMenu);
			// Trigger auto-scaling first to update base actor values to what they were at the player's first saved level in co-op.
			TriggerAVAutoScaling(nullptr, true);
			// Rescale HMS and skill AVs up from the new base actor values for all active players.
			RescaleActivePlayerAVs();
			// Sync changes to shared perks on menu exit.
			SyncSharedPerks();
			// Lastly, adjust perk counts.
			AdjustAllPlayerPerkCounts();
		}
	}

	void GlobalCoopData::AdjustPerkDataForCompanionPlayer(RE::Actor* a_playerActor, const bool& a_enteringMenu)
	{
		auto& glob = GetSingleton();
		const auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_playerActor || glob.serializablePlayerData.empty())
		{
			logger::error("[GLOB] ERR: AdjustPerkDataForCompanionPlayer: player 1 invalid: {}, companion player invalid:{}, serialized data empty: {}.",
				(bool)!p1, (bool)!a_playerActor, glob.serializablePlayerData.empty());
			return;
		}

		logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: Should {} {}'s perk data.",
			a_enteringMenu ? "import" : "export", a_playerActor->GetName());
		if (a_enteringMenu)
		{
			// Adjust perk counts before potentially copying data to P1.
			AdjustAllPlayerPerkCounts();
			// Trigger auto scaling first to update base actor values to their first-saved level equivalents.
			TriggerAVAutoScaling(nullptr, true);
			// Rescale from new base actor values before copying and chacking base actor value data.
			RescaleActivePlayerAVs();
			// Sync changes to shared perks before copying over co-op player's perk tree.
			SyncSharedPerks();

			// Dip P1's level, as necessary, to open the required number of LevelUp menus.
			bool rescaleSkillAVsOnP1LevelDip = AdjustInitialPlayer1PerkPoints(a_playerActor);
			// Rescale player AVs back to their saved values if P1's level was dipped, 
			// since all co-op companions have their AVs auto-scaled by the game when P1's level changes.
			// Copy the co-op companions AVs over to P1 only after we've rescaled.
			if (rescaleSkillAVsOnP1LevelDip)
			{
				logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: About to rescale all companions' AVs after dipping P1's level to spawn level up menus.");
				RescaleActivePlayerAVs();
			}

			// Copy AVs, then perk tree, then name and race name.
			logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: Stats Menu: Should copy AVs, perk tree, name, and race name on import.");
			if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkTree))
			{
				logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: Perk Tree.");
				CopyOverPerkTrees(a_playerActor, a_enteringMenu);
				glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kPerkTree);
			}

			if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
			{
				logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: Name.");
				CopyOverActorBaseData(a_playerActor, a_enteringMenu, true, false, false);
				glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kName);
			}

			if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kRaceName))
			{
				logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: Race Name.");
				CopyOverActorBaseData(a_playerActor, a_enteringMenu, false, true, false);
				glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kRaceName);
			}

			if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
			{
				logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: AVs.");
				CopyOverAVs(a_playerActor, a_enteringMenu);
				glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kSkillsAndHMS);
			}
;
			// Save HMS AVs on menu entry.
			AdjustBaseHMSData(a_playerActor, a_enteringMenu);
		}
		else
		{
			// Save HMS AVs on exit.
			AdjustBaseHMSData(a_playerActor, a_enteringMenu);
			// Restore name and race name, then AVs, and then perk tree.
			logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: Stats Menu: Should copy back name, race name, AVs, and perk tree on export.");
			if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
			{
				logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: Name.");
				CopyOverActorBaseData(a_playerActor, a_enteringMenu, true, false, false);
				glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kName);
			}

			if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kRaceName))
			{
				logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: Race Name.");
				CopyOverActorBaseData(a_playerActor, a_enteringMenu, false, true, false);
				glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kRaceName);
			}

			if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
			{
				logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: AVs.");
				CopyOverAVs(a_playerActor, a_enteringMenu);
				glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
			}

			if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkTree))
			{
				logger::debug("[GLOB] AdjustPerkDataForCompanionPlayer: Perk Tree.");
				CopyOverPerkTrees(a_playerActor, a_enteringMenu);
				glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kPerkTree);
			}

			// Trigger auto scaling first to update base actor values to their first-saved level equivalents.
			TriggerAVAutoScaling(nullptr, true);
			// Rescale HMS and skill AVs up from the new base actor values for all active players.
			RescaleActivePlayerAVs();
			// Sync changes to shared perks on menu exit.
			SyncSharedPerks();
			// Lastly, adjust perk counts.
			AdjustAllPlayerPerkCounts();
		}
	}

	void GlobalCoopData::AssignGenericKillmoves()
	{
		// Assign generic killmoves which are not directly linked to a skeleton type,
		// and further categorize by weapon type.
		logger::debug("[GLOB] AssignGenericKillmoves");
		auto& glob = GetSingleton();
		glob.genericKillmoveIdles = std::vector<std::vector<RE::TESIdleForm*>>(!KillmoveType::kTotal, std::vector<RE::TESIdleForm*>());

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

		// REMOVE when done debugging.
		uint32_t total = 0;
		for (auto i = 0; i < glob.genericKillmoveIdles.size(); ++i)
		{
			total += glob.genericKillmoveIdles[i].size();
		}

		logger::debug("[GLOB] AssignGenericKillmoves: Generic idle total: {}", total);
	}

	void GlobalCoopData::AssignSkeletonSpecificKillmoves()
	{
		// Assign killmoves based on skeleton type, and further categorize by weapon type.
		logger::debug("[GLOB] AssignSkeletonSpecificKillmoves");
		auto& glob = GetSingleton();

		// Traversing all forms.
		// Inefficient, I know, but necessary.
		const auto& allForms = RE::TESForm::GetAllForms();
		if (allForms.first)
		{
			allForms.second.get().LockForRead();
			auto comp = [](const std::string& a_left, const std::string& a_right) { 
				std::string left = a_left.c_str();
				Util::ToLowercase(left);
				std::string right = a_right.c_str();
				Util::ToLowercase(right);
				return strcmp(left.c_str(), right.c_str()) < 0; 
			};

			std::set<std::string, decltype(comp)> skeleNames;
			std::for_each(allForms.first->begin(), allForms.first->end(),
				[&glob, &skeleNames](const auto& a_formPair) {
					if (a_formPair.second && a_formPair.second->Is(RE::FormType::Race))
					{
						const auto asRace = a_formPair.second->As<RE::TESRace>();
						std::string skeleName = std::string();
						Util::GetSkeletonModelNameForRace(asRace, skeleName);
						if (!skeleName.empty()) 
						{
							skeleNames.insert(skeleName);
						}
					}
				});

			allForms.second.get().UnlockForRead();

			// Assign killmoves based on skeleton name and weapon type.
			uint32_t hash = 0;
			for (const auto& skeleName : skeleNames) 
			{
				hash = Hash(skeleName);
				glob.skeletonKillmoveIdlesMap.emplace(hash, std::vector<std::vector<RE::TESIdleForm*>>(!KillmoveType::kTotal, std::vector<RE::TESIdleForm*>()));
				auto& entry = glob.skeletonKillmoveIdlesMap.at(hash);
				switch (hash)
				{
				case ("bear"_h):
				{
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveBear"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveBearA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveBearB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveBearA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveBearB"),
					};

					entry[!KillmoveType::k2H] = {
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
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMWolfKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMWolfKillMoveB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveWolfA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveWolfB")
					};

					entry[!KillmoveType::k2H] = {
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
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveChaurusFlyer"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveChaurusFlyerKick"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveChaurusFlyerStomp")
					};

					entry[!KillmoveType::k2H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveChaurusFlyer"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWKillMoveChaurusFlyer")
					};

					entry[!KillmoveType::kGeneral] = { RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveChaurusFlyer") };

					break;
				}
				case ("dragon"_h):
				{
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMSword_KillMoveDragonRodeoSlash"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMSword_KillMoveDragonRodeoStabShort"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDragonRodeoSlash"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDragonRodeoStabShort")
					};

					entry[!KillmoveType::k2H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMDragonKillMoveSlash"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWDragonKillMoveRodeoSlash"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWDragonKillMoveSlash"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveDragonSlash"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveDragonRodeoSlash"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveDragonSlash")

					};

					entry[!KillmoveType::kGeneral] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveDragonBiteGrapple"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_KillMoveDragonBiteGrapple")
					};

					break;
				}
				case ("draugr"_h):
				{
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveDraugr"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveDraugrB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveDraugrShortA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveDraugrShortB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDraugrA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDraugrB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDraugrShortA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveDraugrShortB")
					};

					entry[!KillmoveType::k2H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HM_KillMoveDraugrA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HW_KillMoveDraugrA")
					};

					break;
				}
				case ("dwarvensteamcenturion"_h):
				{
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSteamCenturionA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSteamCenturionB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSteamCenturionA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSteamCenturionB")
					};

					entry[!KillmoveType::k2H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMSteamCenturionKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWSteamCenturionKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveSteamCenturionA")
					};

					break;
				}
				case ("falmer"_h):
				{
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveFalmer"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveFalmerA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveFalmerB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveFalmerC"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveFalmerA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveFalmerB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveFalmerC")
					};

					entry[!KillmoveType::k2H] = {
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
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSpiderSmashA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSpiderStabA")
					};

					entry[!KillmoveType::k2H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMSpiderKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWSpiderKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveSpiderSlamA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveSpiderSlamA")
					};

					break;
				}
				case ("giant"_h):
				{
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMGiantKillMove"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMGiantKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMGiantKillMoveB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMGiantKillMoveBleedOutA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveGiantA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveGiantB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveGiantBleedOutA")
					};

					entry[!KillmoveType::k2H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMGiantKillMove"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWGiantKillMove"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveGiantA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveGiantA"),
					};

					break;
				}
				case ("hagraven"_h):
				{
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveHagravenA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveHagravenB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveHagravenA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveHagravenB")
					};

					entry[!KillmoveType::k2H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMHagravennKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWHagravenKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveHagravenA")
					};

					break;
				}
				case ("sabrecat"_h):
				{
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSabreCatShortA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSabreCatShortB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveSabreCat"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HmKillMoveSabreCat"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSabreCat"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSabreCatShortA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSabreCatShortB")
					};

					entry[!KillmoveType::k2H] = {
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
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSprigganA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveSprigganB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSprigganA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveSprigganB")
					};

					entry[!KillmoveType::k2H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMSprigganKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWSprigganKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HMKillMoveSprigganA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_2HWKillMoveSprigganA")
					};

					break;
				}
				case ("troll"_h):
				{
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMTrollKillMoveA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMTrollKillMoveB"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HM_KillMoveTroll"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveTrollA"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("pa_1HMKillMoveTrollB")
					};

					entry[!KillmoveType::k2H] = {
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
					entry[!KillmoveType::k1H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("1HMKillMoveVampireBrute")
					};

					entry[!KillmoveType::k2H] = {
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HMKillMoveVampireBrute"),
						RE::TESForm::LookupByEditorID<RE::TESIdleForm>("2HWKillMoveVampireBrute")
					};

					entry[!KillmoveType::kGeneral] = { RE::TESForm::LookupByEditorID<RE::TESIdleForm>("KillMoveVampireBrute") };

					break;
				}
				default:
				{
					break;
				}
				}
			}

			// REMOVE when done debugging.
			uint32_t count = 0;
			uint32_t subCount = 0;
			for (const auto& skeleName : skeleNames)
			{
				hash = Hash(skeleName);
				const auto& killmovesList = glob.skeletonKillmoveIdlesMap.at(hash);
				for (const auto& subList : killmovesList)
				{
					subCount += subList.size();
				}

				count += subCount;
				subCount = 0;
			}

			logger::debug("[GLOB] AssignSkeletonSpecificKillmoves: {} total assigned skeletal killmoves.", count);
		}
	}

	bool GlobalCoopData::CanControlMenus(const int32_t& a_controllerID)
	{
		auto& glob = GetSingleton();
		if (a_controllerID > -1 && a_controllerID < MAX_PLAYER_COUNT) 
		{
			const auto& p = glob.coopPlayers[a_controllerID];
			if (!p->isActive) 
			{
				return false;
			}

			return glob.menuCID == a_controllerID || glob.menuCID == -1;
		}

		return false;
	}

	int8_t GlobalCoopData::GetCoopPlayerIndex(const RE::ActorPtr& a_actorPtr)
	{
		if (!a_actorPtr || !a_actorPtr.get())
		{
			return -1;
		}

		return GetCoopPlayerIndex(a_actorPtr.get());
	}

	int8_t GlobalCoopData::GetCoopPlayerIndex(RE::TESObjectREFR* a_refr)
	{
		if (!a_refr)
		{
			return -1;
		}

		auto& glob = GetSingleton();
		auto foundIter = std::find_if(glob.coopPlayers.begin(), glob.coopPlayers.end(),
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
		if (!a_refrPtr || !a_refrPtr.get())
		{
			return -1;
		}

		return GetCoopPlayerIndex(a_refrPtr.get());
	}

	int8_t GlobalCoopData::GetCoopPlayerIndex(const RE::FormID& a_formID)
	{
		auto& glob = GetSingleton();
		auto foundIter = std::find_if(glob.coopPlayers.begin(), glob.coopPlayers.end(),
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
		auto& glob = GetSingleton();
		if (a_refrHandle.get() && a_refrHandle.get()->IsHandleValid())
		{
			auto foundIter = std::find_if(glob.coopPlayers.begin(), glob.coopPlayers.end(),
				[a_refrHandle](const auto& a_p) 
				{ 
					return a_p->isActive && a_p->coopActor->GetHandle() == a_refrHandle; 
				}
			);
			if (foundIter != glob.coopPlayers.end())
			{
				return std::distance(glob.coopPlayers.begin(), foundIter);
			}
		}

		return -1;
	}

	float GlobalCoopData::GetHighestSharedAVLevel(const RE::ActorValue& a_av)
	{
		// Get highest AV level for the shared AV among all players, active or inactive.
		// -1 indicates that the AV should not be modified.
		// 
		// NOTE:
		// If we were to consider only active players, the highest skill level might decrease
		// if summoning a different set of players, which would mean that the party might not 
		// reach certain unlocked shared skill perks' required AV levels.
		// For example: 
		// Party 1: P1 and P2: Highest Lockpicking Level: 25 (P2), 
		// first Lockpicking perk 'Novice Locks' (required level 20) is unlocked, 
		// set all player Lockpicking levels to 25.
		// Party 2: P1 and P3: Highest Lockpicking Level: 17 (P1), set all player Lockpicking levels to 17.
		// 'Novice Locks' remains unlocked but its minimum level is not reached (17 < 25).
		// If P2 (now inactive) is also considered instead, the highest level will stay at 25,
		// and 'Novice Locks' level requirement is still met.

		auto& glob = GetSingleton();
		float highestAVAmount = -1.0f;
		const auto skill = AV_TO_SKILL_MAP.contains(a_av) ? AV_TO_SKILL_MAP.at(a_av) : Skill::kTotal;
		RE::Actor* playerActor = nullptr;
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			for (auto& [fid, data] : glob.serializablePlayerData)
			{
				// Player 1 FID is always 0x14.
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
				if (float value = data->skillBaseLevelsList[skill] + data->skillLevelIncreasesList[skill]; value > highestAVAmount)
				{
					logger::debug("[GLOB] GetHighestSharedAVLevel for {}: {} has the new highest AV amount: {}.", Util::GetActorValueName(a_av), playerActor->GetName(), value);
					highestAVAmount = value;
				}
			}
		}

		logger::debug("[GLOB] GetHighestSharedAVLevel for {}: {}.", Util::GetActorValueName(a_av), highestAVAmount);
		return highestAVAmount;
	}

	uint32_t GlobalCoopData::GetUnlockedSharedPerksCount()
	{
		// Precondition: all players have the same set of shared perks before calling this func.
		auto& glob = GetSingleton();
		const auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1) 
		{
			logger::error("[GLOB] ERR: GetUnlockedSharedPerksCount: player 1 is invalid: {}.", (bool)!p1);
			return 0;
		}

		// Same perk shows up multiple times in some trees. 
		// Do not count the same perk multiple times.
		std::set<RE::BGSPerk*> perksSet;
		std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> getSharedPerksCount =
			[p1, &glob, &perksSet](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
				if (auto perk = a_node->perk; perk)
				{
					bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
					if (shared)
					{
						while (perk)
						{
							// Each player will have the same shared perks, so simply check P1 for shared perks.
							// Not always in sync, so if either perk list says P1 has the perk, add it.
							bool nativeHasPerk = p1->HasPerk(perk);
							bool singletonListHasPerk = Util::Player1PerkListHasPerk(perk);
							if (nativeHasPerk || singletonListHasPerk)
							{
								logger::debug("[GLOB] GetUnlockedSharedPerksCount: has perk {} (0x{:X}): {}, {}", perk->GetName(), perk->formID, nativeHasPerk, singletonListHasPerk);
								perksSet.insert(perk);
							}

							perk = perk->nextPerk;
						}
					}
				}
			};

		Util::TraverseAllPerks(p1, getSharedPerksCount);
		logger::debug("[GLOB] GetUnlockedSharedPerksCount: total: {}", perksSet.size());
		return perksSet.size();
	}

	void GlobalCoopData::GivePartyWideItemsToP1()
	{
		// Transfer gold, lockpicks, keys, notes, and non-skill/level granting books to P1.
		// Gold and lockpicks are shared, since P1 effectively triggers the Lockpicking and Barter Menus,
		// even if another player is controlling these menus. Less prone to error if these common items
		// are kept on P1 where they can be used by all active players.
		//
		// Likewise, if a key, note, or book is required to open a door, progress a quest, or trigger an event,
		// it must be on P1's person at the time of activation, so having these items always in P1's inventory
		// is less of a hassle when trying to find a specific item.
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			logger::error("[GLOB] ERR: GivePartyWideItemsToP1: P1 is invalid.");
			return;
		}

		auto& glob = GetSingleton();
		for (const auto& p : glob.coopPlayers) 
		{
			if (p->isActive && !p->isPlayer1) 
			{
				auto inventory = p->coopActor->GetInventory();
				for (auto& [boundObj, entry] : inventory)
				{
					// Transfer to P1.
					if (Util::IsPartyWideItem(boundObj))
					{
						logger::debug("[GLOB] GivePartyWideItemsToP1: {}: giving {} {} to P1 from the player's inventory.", p->coopActor->GetName(), entry.first, boundObj->GetName());
						p->coopActor->RemoveItem(boundObj, entry.first, RE::ITEM_REMOVE_REASON::kStoreInTeammate, nullptr, p1);
					}
				}

				if (auto invChanges = p1->GetInventoryChanges(); invChanges && invChanges->entryList)
				{
					auto invCounts = p1->GetInventoryCounts();
					for (auto invChangesEntry : *invChanges->entryList)
					{
						if (invChangesEntry && invChangesEntry->object)
						{
							auto boundObj = invChangesEntry->object;
							auto count = invCounts.contains(boundObj) ? invCounts.at(boundObj) : 0;
							// Transfer to P1.
							if (Util::IsPartyWideItem(boundObj))
							{
								logger::debug("[GLOB] GivePartyWideItemsToP1: {}: giving {} {} to P1 from the player's inventory changes.", p->coopActor->GetName(), count, boundObj->GetName());
								p->coopActor->RemoveItem(boundObj, count, RE::ITEM_REMOVE_REASON::kStoreInTeammate, nullptr, p1);
							}
						}
					}
				}
			}
		}
	}

	void GlobalCoopData::HandlePlayerArmCollisions()
	{
		// Check for arm collisions for each player that is rotating their arms,
		// and handle any impacts (impulses, knockdowns, damage) with other actors.

		auto& glob = GetSingleton();
		if (!glob.coopSessionActive || !Settings::bRotateArmsWhenSheathed)
		{
			return;
		}

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive || (p->isPlayer1 && !glob.cam->IsRunning()))
			{
				continue;
			}

			bool isRotatingShoulders = p->pam->IsPerformingOneOf
			(
				InputAction::kRotateLeftShoulder, InputAction::kRotateRightShoulder
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
				CheckAndPerformArmCollisions(p);
			}
		}
	}

	void GlobalCoopData::HandleEnderalProgressionChanges()
	{
		// P1 level up:
		// Rescale all active companions' AVs.
		// Check for increments to crafting/learning/memory points and 
		// multiply these changes by the party-size as necessary.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !glob.globalDataInit || glob.serializablePlayerData.empty()) 
		{
			return;
		}

		if (glob.serializablePlayerData.contains(p1->formID)) 
		{
			const auto& p1Data = glob.serializablePlayerData.at(p1->formID);
			// Only rescale if P1 leveled up during a co-op session.
			// Using Enderal player level global which is unaffected by the SetLevel console command.
			// Want to ignore our false level ups when triggering AV auto-scaling.
			if (glob.playerLevelGlob) 
			{
				if (glob.coopSessionActive)
				{
					if (p1Data->level < glob.playerLevelGlob->value)
					{
						logger::debug("[GLOB] HandleEnderalProgressionChanges: P1 {} changed level from saved level {} to {} (game reported {}). Rescale all co-op companion AVs.",
							p1->GetName(), p1Data->level, glob.playerLevelGlob->value, p1->GetLevel());
						TriggerAVAutoScaling(nullptr, true);
						RescaleActivePlayerAVs();

						// Send message box menu request for P1 to gain control of the Enderal level up menu that opens post-levelup.
						glob.moarm->InsertRequest(glob.player1CID, InputAction::kActivate, SteadyClock::now(), RE::MessageBoxMenu::MENU_NAME);
					}
				}

				// Update level afterward.
				p1Data->level = glob.playerLevelGlob->value;

			}

			// Crafting points increase.
			if (glob.craftingPointsGlob)
			{
				// Only scale if earned during a co-op session.
				if (glob.coopSessionActive && Settings::bScaleCraftingPointsWithNumPlayers)
				{
					if (glob.craftingPointsGlob->value > glob.savedCraftingPoints)
					{
						float newCraftingPointsDelta = (glob.craftingPointsGlob->value - glob.savedCraftingPoints) * (float)glob.activePlayers;
						glob.craftingPointsGlob->value = glob.savedCraftingPoints + newCraftingPointsDelta;
						RE::DebugMessageBox(fmt::format("Gained {} Crafting Point(s) after party scaling.\nNew total: {}",
							newCraftingPointsDelta, glob.craftingPointsGlob->value).c_str());
					}
				}

				glob.savedCraftingPoints = glob.craftingPointsGlob->value;
			}

			// Learning points increase.
			if (glob.learningPointsGlob)
			{
				// Only scale if earned during a co-op session.
				if (glob.coopSessionActive && Settings::bScaleLearningPointsWithNumPlayers)
				{
					if (glob.learningPointsGlob->value > glob.savedLearningPoints)
					{
						float newLearningPointsDelta = (glob.learningPointsGlob->value - glob.savedLearningPoints) * (float)glob.activePlayers;
						glob.learningPointsGlob->value = glob.savedLearningPoints + newLearningPointsDelta;
						RE::DebugMessageBox(fmt::format("Gained {} Learning Point(s) after party scaling.\nNew total: {}",
							newLearningPointsDelta, glob.learningPointsGlob->value).c_str());
					}
				}

				glob.savedLearningPoints = glob.learningPointsGlob->value;
			}

			// Memory points increase.
			if (glob.memoryPointsGlob)
			{
				// Only scale if earned during a co-op session.
				if (glob.coopSessionActive && Settings::bScaleMemoryPointsWithNumPlayers)
				{
					if (glob.memoryPointsGlob->value > glob.savedMemoryPoints)
					{
						float newMemoryPointsDelta = (glob.memoryPointsGlob->value - glob.savedMemoryPoints) * (float)glob.activePlayers;
						glob.memoryPointsGlob->value = glob.savedMemoryPoints + newMemoryPointsDelta;
						RE::DebugMessageBox(fmt::format("Gained {} Memory Point(s) after party scaling.\nNew total: {}",
							newMemoryPointsDelta, glob.memoryPointsGlob->value).c_str());
					}
				}

				glob.savedMemoryPoints = glob.memoryPointsGlob->value;
				// Memory points count also stored in 'DragonSouls' AV.
				p1->SetActorValue(RE::ActorValue::kDragonSouls, glob.savedMemoryPoints);
			}

			// Werewolf transformation.
			// Global variable keeps track of any P1 transformations.
			if (glob.player1CID != -1) 
			{
				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
				if (!glob.coopPlayers[glob.player1CID]->isTransformed && glob.werewolfTransformationGlob->value == 1.0f) 
				{
					logger::debug("[GLOB] HandleEnderalProgressionChanges: P1 {} is now transformed into a werewolf. Current race: {}.", p1->GetName(), p1->race ? p1->race->GetName() : "NONE");
					for (auto effect : *coopP1->coopActor->GetActiveEffectList())
					{
						if ((effect->GetBaseObject()->formID & 0x00FFFFFF) == 0x29BA4)
						{
							logger::debug("[GLOB] HandleEnderalProgressionChanges: P1 {}'s werewolf transformation duration is {}s.",
								p1->GetName(), effect->duration);
							coopP1->secsMaxTransformationTime = effect->duration;
						}
					}

					coopP1->transformationTP = SteadyClock::now();
					coopP1->isTransformed = true;
				}
				else if (glob.coopPlayers[glob.player1CID]->isTransformed && glob.werewolfTransformationGlob->value == 0.0f)
				{
					logger::debug("[GLOB] HandleEnderalProgressionChanges: P1 {} is no longer transformed into a werewolf.", p1->GetName());
					coopP1->isTransformed = false;
				}
			}
		}

		// TODO: Attempt to find an event-driven method or hook to check for AV changes, instead of every second.
		// Restore skill AVs since the game will make changes to them after our rescaling on level up.
		// I haven't found a place to hook yet to listen for skill AV changes.
		if (Util::GetElapsedSeconds(glob.lastCoopCompanionSkillLevelsCheckTP) > 1.0f) 
		{
			//glob.lastCoopCompanionSkillLevelsCheckTP = SteadyClock::now();
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive && !p->isPlayer1 && glob.serializablePlayerData.contains(p->coopActor->formID))
				{
					auto& data = glob.serializablePlayerData.at(p->coopActor->formID);
					for (uint8_t i = 0; i < SKILL_ACTOR_VALUES_LIST.size(); ++i)
					{
						const auto& av = SKILL_ACTOR_VALUES_LIST[i];
						float currentValue = p->coopActor->GetBaseActorValue(av);
						float newValue = data->skillBaseLevelsList[i] + data->skillLevelIncreasesList[i];
						if (currentValue != newValue)
						{
							logger::debug("[GLOB] HandleEnderalProgressionChanges: {}: skill AV {} was set to {} when it should be set to {} ({} + {}).",
								p->coopActor->GetName(), std::format("{}", av), currentValue, newValue, data->skillBaseLevelsList[i], data->skillLevelIncreasesList[i]);
							p->coopActor->SetBaseActorValue(av, newValue);
						}
					}
				}
			}
		}
	}

	void GlobalCoopData::HavokPrePhysicsStep(RE::bhkWorld* a_world)
	{
		// Cache player arm and torso node rotations to restore later
		// in the NiNode UpdateDownwardPass() hook,
		// which will overwrite the game's changes to all handled player arm/torso nodes.

		auto& glob = GetSingleton();
		if (!glob.coopSessionActive)
		{
			return;
		}

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive || (p->isPlayer1 && !glob.cam->IsRunning()))
			{
				continue;
			}

			// Modified node rotation interferes with getup animation, and causes the player to spazz out,
			// so don't modify node rotations when the player's knock state is not normal.
			if (p->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal) 
			{
				if (Settings::bRotateArmsWhenSheathed)
				{
					// Sheathed weapons and in dialogue or not controlling menus.
					auto ui = RE::UI::GetSingleton();
					bool inDialogueOrNotControllingMenus = 
					{
						(glob.menuCID != p->controllerID) ||
						(ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
					};
					if (!p->coopActor->IsWeaponDrawn() && inDialogueOrNotControllingMenus)
					{
						bool isRotatingShoulders = p->pam->IsPerformingOneOf(InputAction::kRotateLeftShoulder, InputAction::kRotateRightShoulder);
						if (isRotatingShoulders)
						{
							RotateArmsAboutShoulders(p);
						}

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
							TwistAndBendArms(p);
						}
					}
					else if (!p->mm->nrm->lastSetArmLocalRotations.empty() || !p->mm->nrm->lastSetArmRotationInputs.empty())
					{
						// Reset limb rotations when weapons are out or if there are no stored rotations.
						p->mm->nrm->ResetLimbNodeOrientationData();
					}
				}

				// Don't adjust rotation if aim pitch has not been adjusted.
				if (p->mm->aimPitchAdjusted)
				{
					AdjustPlayerTorsoRotation(p);
				}
			}

			// Update flag signalling downward pass hook to restore cached node rotations to overwrite the game's changes.
			p->mm->nrm->restoreArmNodeRotations = 
			{
				Settings::bRotateArmsWhenSheathed &&
				!p->coopActor->IsWeaponDrawn() &&
				p->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
				!p->coopActor->actorState2.staggered &&
				!p->coopActor->IsSwimming() &&
				!p->mm->nrm->lastSetArmLocalRotations.empty() &&
				!p->mm->nrm->lastSetArmRotationInputs.empty()
			};

			// Game already handles P1 torso pitch nicely when aiming with 2H ranged weapons.
			bool p1IsAimingWith2HRangedWeap = 
			{ 
				(p->isPlayer1) &&
				(p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
				p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDraw ||
				p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn ||
				p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing ||
				p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleased ||
				p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowFollowThrough)
			};

			p->mm->nrm->restoreTorsoNodeRotations = 
			{
				!p1IsAimingWith2HRangedWeap &&
				p->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
				!p->coopActor->actorState2.staggered &&
				!p->coopActor->IsSwimming() &&
				!p->mm->nrm->lastSetTorsoLocalRotations.empty() &&
				!p->mm->nrm->lastSetTorsoRotationInputs.empty()
			};
		}
	}

	void GlobalCoopData::ImportUnlockedPerks(RE::Actor* a_coopActor)
	{
		// Import all serialized perks that the player has unlocked.

		logger::debug("[GLOB] ImportUnlockedPerks: {}", a_coopActor->GetName());
		auto& glob = GetSingleton();
		// Add saved perks to the player if they do not have them added already.
		if (glob.serializablePlayerData.contains(a_coopActor->formID))
		{
			bool isP1 = a_coopActor == RE::PlayerCharacter::GetSingleton();
			std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> removeAllPerks = 
			[&isP1, a_coopActor](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
				if (a_node)
				{
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
							// NOTE: Removing all perks, regardless of whether or not the Actor::HasPerk()
							// check returns true, since for some reason, the check will return false here for some
							// previously-added perk (via Util::ChangePerk()) on the current save,
							// and then return true later when importing perks in CopyOverPerkTree() after reloading the first save,
							// which should discard the added perk changes made before reloading. 
							// Possibly due to actor base perk changes time traveling across saves, irrespective of which save the changes were made in?
							// Either way, the bug has been hell to trace, so remove all perks just to be safe.
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
				}
			};

			// NOTE: Have to remove all perks first, since perks added to a co-op actor in one save are still present when loading an older save.
			Util::TraverseAllPerks(a_coopActor, removeAllPerks);
			const auto& data = glob.serializablePlayerData.at(a_coopActor->formID);
			const auto& unlockedPerksList = data->GetUnlockedPerksList();
			logger::debug("[GLOB] ImportUnlockedPerks: {} has {} unlocked perks serialized for this save file.", 
				a_coopActor->GetName(), unlockedPerksList.size());

			// Add any new animationevent-based perks, if needed.
			if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled && Settings::bAddAnimEventSkillPerks)
			{
				// Need to get perks if global data has not been initialized yet.
				if (!glob.globalDataInit) 
				{
					glob.assassinsBladePerk = RE::TESForm::LookupByID(0x58211)->As<RE::BGSPerk>();
					glob.backstabPerk = RE::TESForm::LookupByID(0x58210)->As<RE::BGSPerk>();
					glob.criticalChargePerk = RE::TESForm::LookupByID(0xCB406)->As<RE::BGSPerk>();
					glob.deadlyAimPerk = RE::TESForm::LookupByID(0x1036F0)->As<RE::BGSPerk>();
					glob.dualCastingAlterationPerk = RE::TESForm::LookupByID(0x153CD)->As<RE::BGSPerk>();
					glob.dualCastingConjurationPerk = RE::TESForm::LookupByID(0x153CE)->As<RE::BGSPerk>();
					glob.dualCastingDestructionPerk = RE::TESForm::LookupByID(0x153CF)->As<RE::BGSPerk>();
					glob.dualCastingIllusionPerk = RE::TESForm::LookupByID(0x153D0)->As<RE::BGSPerk>();
					glob.dualCastingRestorationPerk = RE::TESForm::LookupByID(0x153D1)->As<RE::BGSPerk>();
					glob.greatCriticalChargePerk = RE::TESForm::LookupByID(0xCB407)->As<RE::BGSPerk>();
					glob.powerBashPerk = RE::TESForm::LookupByID(0x58F67)->As<RE::BGSPerk>();
					glob.quickShotPerk = RE::TESForm::LookupByID(0x105F19)->As<RE::BGSPerk>();
					glob.shieldChargePerk = RE::TESForm::LookupByID(0x58F6A)->As<RE::BGSPerk>();
					glob.sneakRollPerk = RE::TESForm::LookupByID(0x105F23)->As<RE::BGSPerk>();
				}

				if (glob.criticalChargePerk && !data->HasUnlockedPerk(glob.criticalChargePerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.criticalChargePerk->GetName());
					data->InsertUnlockedPerk(glob.criticalChargePerk);
				}

				if (glob.dualCastingAlterationPerk && !data->HasUnlockedPerk(glob.dualCastingAlterationPerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.dualCastingAlterationPerk->GetName());
					data->InsertUnlockedPerk(glob.dualCastingAlterationPerk);
				}

				if (glob.dualCastingConjurationPerk && !data->HasUnlockedPerk(glob.dualCastingConjurationPerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.dualCastingConjurationPerk->GetName());
					data->InsertUnlockedPerk(glob.dualCastingConjurationPerk);
				}

				if (glob.dualCastingDestructionPerk && !data->HasUnlockedPerk(glob.dualCastingDestructionPerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.dualCastingDestructionPerk->GetName());
					data->InsertUnlockedPerk(glob.dualCastingDestructionPerk);
				}

				if (glob.dualCastingIllusionPerk && !data->HasUnlockedPerk(glob.dualCastingIllusionPerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.dualCastingIllusionPerk->GetName());
					data->InsertUnlockedPerk(glob.dualCastingIllusionPerk);
				}

				if (glob.dualCastingRestorationPerk && !data->HasUnlockedPerk(glob.dualCastingRestorationPerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.dualCastingRestorationPerk->GetName());
					data->InsertUnlockedPerk(glob.dualCastingRestorationPerk);
				}

				if (glob.greatCriticalChargePerk && !data->HasUnlockedPerk(glob.greatCriticalChargePerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.greatCriticalChargePerk->GetName());
					data->InsertUnlockedPerk(glob.greatCriticalChargePerk);
				}

				if (glob.powerBashPerk && !data->HasUnlockedPerk(glob.powerBashPerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.powerBashPerk->GetName());
					data->InsertUnlockedPerk(glob.powerBashPerk);
				}

				if (glob.shieldChargePerk && !data->HasUnlockedPerk(glob.shieldChargePerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.shieldChargePerk->GetName());
					data->InsertUnlockedPerk(glob.shieldChargePerk);
				}

				if (glob.sneakRollPerk && !data->HasUnlockedPerk(glob.sneakRollPerk))
				{
					logger::debug("[GLOB] ImportUnlockedPerks: {} does not have {} perk. Adding to unlocked perks list now.",
						a_coopActor->GetName(), glob.sneakRollPerk->GetName());
					data->InsertUnlockedPerk(glob.sneakRollPerk);
				}
			}

			// Add back all unlocked perks.
			for (const auto perk : unlockedPerksList)
			{
				logger::debug("[GLOB] ImportUnlockedPerks: Checking if {} has saved unlocked perk {} (0x{:X}) on init. Has perk: {}",
					a_coopActor->GetName(), perk->GetName(), perk->formID, a_coopActor->HasPerk(perk));
				logger::debug("[GLOB] ImportUnlockedPerks: {} does not have saved perk {} (0x{:X}), adding it now.",
					a_coopActor->GetName(), perk->GetName(), perk->formID);
				// NOTE: Adding all unlocked perks again, regardless of whether or not the Actor::HasPerk()
				// check returns true. Same reasoning as removing all perks above.
				if (isP1)
				{
					Util::Player1AddPerk(perk);
				}
				else
				{
					Util::ChangePerk(a_coopActor, perk, true);
				}
			}


			// REMOVE after debugging.
			/*
			std::function<void(RE::BGSSkillPerkTreeNode * a_node, RE::Actor * a_actor)> checkPerkTree = [](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
				if (a_node)
				{
					auto perk = a_node->perk;
					uint32_t perkIndex = 0;
					while (perk)
					{
						// Selected perks do not get added to the player 1 glob list while the level up menu is open?
						// Have to use native func check here as a result.
						if (a_actor->HasPerk(perk))
						{
							logger::debug("[GLOB] ImportUnlockedPerks: AFTER IMPORT: {} has perk #{} {} (0x{:X})",
								a_actor->GetName(), perkIndex, perk->GetName(), perk->formID);
						}

						perk = perk->nextPerk;
						++perkIndex;
					}
				}
			};

			Util::TraverseAllPerks(a_coopActor, checkPerkTree);
			*/
		}
	}

	bool GlobalCoopData::IsControllingMenus(const int32_t& a_controllerID)
	{
		auto& glob = GetSingleton();
		if (a_controllerID > -1 && a_controllerID < MAX_PLAYER_COUNT)
		{
			const auto& p = glob.coopPlayers[a_controllerID];
			if (!p->isActive)
			{
				return false;
			}

			return glob.menuCID == p->controllerID;
		}

		return false;
	}

	bool GlobalCoopData::IsCoopPlayer(const RE::ActorPtr& a_actorPtr)
	{
		if (!a_actorPtr || !a_actorPtr.get())
		{
			return false;
		}

		return IsCoopPlayer(a_actorPtr.get());
	}

	bool GlobalCoopData::IsCoopPlayer(RE::TESObjectREFR* a_refr)
	{
		auto& glob = GetSingleton();
		return std::any_of(glob.coopPlayers.begin(), glob.coopPlayers.end(),
			[a_refr](const auto& a_p) { return a_p->isActive && a_p->coopActor && a_p->coopActor.get() == a_refr; });
	}

	bool GlobalCoopData::IsCoopPlayer(const RE::TESObjectREFRPtr& a_refrPtr)
	{
		if (!a_refrPtr || !a_refrPtr.get())
		{
			return false;
		}

		return IsCoopPlayer(a_refrPtr.get());
	}

	bool GlobalCoopData::IsCoopPlayer(const RE::ObjectRefHandle& a_refrHandle)
	{
		auto& glob = GetSingleton();
		// Ensure refr is valid first.
		if (auto refrPtr = Util::GetRefrPtrFromHandle(a_refrHandle); refrPtr && refrPtr->IsHandleValid()) 
		{
			return std::any_of(glob.coopPlayers.begin(), glob.coopPlayers.end(),
				[a_refrHandle](const auto& a_p) { return a_p->isActive && a_p->coopActor && a_p->coopActor->GetHandle() == a_refrHandle; });
		}
		
		return false;
	}

	bool GlobalCoopData::IsCoopPlayer(const RE::FormID& a_formID)
	{
		auto& glob = GetSingleton();
		return std::any_of(glob.coopPlayers.begin(), glob.coopPlayers.end(),
			[a_formID](const auto& a_p) { return a_p->isActive && a_p->coopActor && a_p->coopActor->formID == a_formID; });
	}

	bool GlobalCoopData::IsNotControllingMenus(const int32_t& a_controllerID)
	{
		auto& glob = GetSingleton();
		if (a_controllerID > -1 && a_controllerID < MAX_PLAYER_COUNT)
		{
			const auto& p = glob.coopPlayers[a_controllerID];
			if (!p->isActive)
			{
				return true;
			}

			return glob.menuCID != p->controllerID;
		}

		return true;
	}

	bool GlobalCoopData::IsSupportedMenuOpen()
	{
		auto& glob = GetSingleton();
		if (const auto ui = RE::UI::GetSingleton(); ui) 
		{
			bool supportedMenuOpen = false;
			for (const auto& menuName : glob.SUPPORTED_MENU_NAMES)
			{
				if (ui->IsMenuOpen(menuName))
				{
					return true;
				}
			}
		}

		return false;
	}

	void GlobalCoopData::ModifyLevelUpXPThreshold(const bool& a_setForCoop)
	{
		// Should be called on co-op start/end and after leveling up.
		// Source: https://en.uesp.net/wiki/Skyrim:Leveling#Level_and_Skill_XP_Formulae
		// The XP levelup mult gamesetting is changed each level to indirectly get the desired level up XP threshold.
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto p1Skills = p1->skills;
		if (!p1 || !p1Skills) 
		{
			return;
		}

		// Scale levelup threshold with respect to the vanilla game's base and mult values for XP.
		float defBase = 75.0f;
		auto valueOpt = Util::GetGameSettingFloat("fXPLevelUpBase");
		if (valueOpt.has_value())
		{
			defBase = valueOpt.value();
		}

		float defMult = 25.0f;
		float currentMult = defMult;
		if (valueOpt = Util::GetGameSettingFloat("fXPLevelUpMult"); valueOpt.has_value())
		{
			currentMult = valueOpt.value();
		}

		float currentLevel = p1->GetLevel();
		float newMult = currentMult;
		if (a_setForCoop)
		{
			newMult = (Settings::fLevelUpXPThresholdMult * (defBase + currentLevel * defMult) - defBase) / (currentLevel);
		}

		if (newMult != currentMult)
		{
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

		if (newThreshold != currentThreshold)
		{
			p1Skills->data->levelThreshold = newThreshold;
		}

		// REMOVE when done debugging.
		if (newThreshold != currentThreshold || newMult != currentMult) 
		{
			logger::debug("[GLOB] ModifyLevelUpXPThreshold: Level {}, set for co-op: {}. P1's XP levelup mult is now {}, was {}. P1's level threshold is now {}, was {}, XP: {}.",
				p1->GetLevel(), a_setForCoop, newMult, currentMult, newThreshold, currentThreshold, p1Skills->data->xp);
		}
	}

	void GlobalCoopData::ModifyXPPerSkillLevelMult(const bool& a_setForCoop)
	{
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
			// REMOVE
			logger::debug("[GLOB] ModifyXPPerSkillLevelMult: update fXPPerSkillRank: {} -> {}: {}", currentXPMult, newXPMult, succ ? "SUCCESS" : "FAILURE");
		}
	}

	void GlobalCoopData::PerformInitialAVAutoScaling()
	{
		// Sometimes player actors do not auto-scale their AVs until their 3D is loaded in (?):
		// auto-scaling during serialization load fails at times. 
		// Called on co-op session start/end instead.
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			logger::error("[GLOB] ERR: PerformInitialAVAutoScaling: P1 is invalid: {}.", (bool)!p1);
			return;
		}

		// Auto scale without updating base AVs first.
		bool succ = TriggerAVAutoScaling(nullptr, false);
		if (succ) 
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive && !p->isPlayer1 && glob.serializablePlayerData.contains(p->coopActor->formID))
				{
					auto& data = glob.serializablePlayerData.at(p->coopActor->formID);
					if (data->firstSavedLevel != 0)
					{
						continue;
					}

					data->firstSavedLevel = p1->GetLevel();
					logger::debug("[GLOB] PerformInitialAVAutoScaling: First co-op level-up for {}. First saved level set to {}. Auto-scale AVs.",
						data->firstSavedLevel, p->coopActor->GetName());

					// Check for differences between auto-scaled skills and current skills lists.
					// If the current skill AV is greater than the auto-scaled one,
					// indicating some progression before the first co-op session,
					// save the difference to the skill increments list.
					auto currentSkills = data->skillBaseLevelsList;
					auto autoScaledSkills = Util::GetActorSkillAVs(p->coopActor.get());
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

						// REMOVE after debugging.
						auto currentSkill = static_cast<Skill>(j);
						if (SKILL_TO_AV_MAP.contains(currentSkill))
						{
							auto currentAV = SKILL_TO_AV_MAP.at(currentSkill);
							logger::debug("[GLOB] PerformInitialAVAutoScaling: {}'s {} skill levels are now: ({} + {}) (current: {}, rescaled: {}).",
								p->coopActor->GetName(),
								std::format("{}", currentAV),
								data->skillBaseLevelsList[j],
								data->skillLevelIncreasesList[j],
								currentSkills[j],
								autoScaledSkills[j]);
						}
					}
				}
			}
		}
		else
		{
			logger::error("[GLOB] ERR: PerformInitialAVAutoScaling: Auto-scaling failed, not updating serialized base AVs for any player.");
		}
		
	}

	void GlobalCoopData::RegisterEvents()
	{
		auto& glob = GetSingleton();
		if (!glob.onCoopHelperMenuRequest.Register(glob.player1RefAlias))
		{
			logger::debug("[GLOB] RegisterEvents: Could not register player ref alias ({}) for OnCoopHelperMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str());
		}
		else
		{
			logger::debug("[GLOB] RegisterEvents: Registered OnCoopHelperMenuRequest() event");
		}

		if (!glob.onDebugMenuRequest.Register(glob.player1RefAlias))
		{
			logger::debug("[GLOB] RegisterEvents: Could not register player ref alias ({}) for OnDebugMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str());
		}
		else
		{
			logger::debug("[GLOB] RegisterEvents: Registered OnDebugMenuRequest() event");
		}

		if (!glob.onSummoningMenuRequest.Register(glob.player1RefAlias))
		{
			logger::debug("[GLOB] RegisterEvents: Could not register player ref alias ({}) for OnSummoningMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str());
		}
		else
		{
			logger::debug("[GLOB] RegisterEvents: Registered OnSummoningMenuRequest() event");
		}
	}

	void GlobalCoopData::RescaleActivePlayerAVs()
	{
		auto& glob = GetSingleton();
		if (glob.allPlayersInit)
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive)
				{
					// Ensure active player's FID is used to index into serializable data map.
					if (!glob.serializablePlayerData.contains(p->coopActor->formID))
					{
						logger::error("[GLOB] ERR: RescaleActivePlayerAVs: Could not index serialized data with {}'s form ID (0x{:X}).",
							p->coopActor->GetName(), p->coopActor->formID);
						continue;
					}
					else
					{
						logger::debug("[GLOB] RescaleActivePlayerAVs: serializable data contains FID key: 0x{:X}", p->coopActor->formID);
					}

					if (!p->isPlayer1)
					{
						logger::debug("[GLOB] RescaleActivePlayerAVs: about to rescale HMS for {}.", p->coopActor->GetName());
						// Skill AVs first.
						RescaleSkillAVs(p->coopActor.get());
						// NOTE for Enderal:
						// Co-op companion HMS values are only scaled by the player's class as of now.
						if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
						{
							const auto& data = glob.serializablePlayerData.at(p->coopActor->formID);
							RescaleHMS(p->coopActor.get(), data->firstSavedLevel);
						}
					}
					else if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
					{
						logger::debug("[GLOB] RescaleActivePlayerAVs: about to rescale HMS for P1.");
						RescaleHMS(p->coopActor.get());
					}
				}
			}
		}
	}

	void GlobalCoopData::RescaleAVsOnBaseSkillAVChange(RE::Actor* a_playerActor)
	{
		logger::debug("[GLOB] RescaleAVsOnBaseSkillAVChange: {}, class: {}, 0x{:X}, race: {}, 0x{:X}, level: {}", 
			a_playerActor ? a_playerActor->GetName() : "NONE",
			a_playerActor && a_playerActor->GetActorBase() && a_playerActor->GetActorBase()->npcClass ? a_playerActor->GetActorBase()->npcClass->GetName() : "NONE",
			a_playerActor && a_playerActor->GetActorBase() && a_playerActor->GetActorBase()->npcClass ? a_playerActor->GetActorBase()->npcClass->formID : 0xDEADA55,
			a_playerActor && a_playerActor->GetRace() ? a_playerActor->GetRace()->GetName() : "NONE",
			a_playerActor && a_playerActor->GetRace() ? a_playerActor->GetRace()->formID : 0xDEADA55,
			a_playerActor ? a_playerActor->GetLevel() : -1);

		// NOTE: Can be called with no co-op session active,
		// for example, when in the summoning menu.
		auto p1 = RE::PlayerCharacter::GetSingleton();
		// Cannot scale if either the co-op companion actor or P1 are invalid.
		if (a_playerActor && p1)
		{
			auto& glob = GetSingleton();
			// Ensure active player's FID is used to index into serializable data map.
			if (!glob.serializablePlayerData.contains(a_playerActor->formID))
			{
				logger::error("[GLOB] ERR: RescaleAVsOnBaseSkillAVChange: Could not index serialized data with {}'s form ID (0x{:X}). No rescaling possible.",
					a_playerActor->GetName(), a_playerActor->formID);
				return;
			}
			else
			{
				logger::debug("[GLOB] RescaleAVsOnBaseSkillAVChange: serializable data contains FID key: 0x{:X}", a_playerActor->formID);
			}

			// REMOVE when done debugging.
			logger::debug("[GLOB] RescaleAVsOnBaseSkillAVChange: Before auto-scaling: {} is level {}. HMS values: {}, {}, {}, calc level with update: {}.",
				a_playerActor->GetName(),
				a_playerActor->GetLevel(),
				a_playerActor->GetActorValue(RE::ActorValue::kHealth),
				a_playerActor->GetActorValue(RE::ActorValue::kMagicka),
				a_playerActor->GetActorValue(RE::ActorValue::kStamina),
				a_playerActor->GetCalcLevel(true));

			// Auto scale first and save new base actor values.
			if (TriggerAVAutoScaling(a_playerActor, true)) 
			{
				// REMOVE when done debugging.
				logger::debug("[GLOB] RescaleAVsOnBaseSkillAVChange: After auto-scaling: {} is level {}. HMS values: {}, {}, {}, calc level with update: {}.",
					a_playerActor->GetName(),
					a_playerActor->GetLevel(),
					a_playerActor->GetActorValue(RE::ActorValue::kHealth),
					a_playerActor->GetActorValue(RE::ActorValue::kMagicka),
					a_playerActor->GetActorValue(RE::ActorValue::kStamina),
					a_playerActor->GetCalcLevel(true));

				// Scale skill AVs next.
				RescaleSkillAVs(a_playerActor);
				// Lastly, scale up HMS with saved increases.
				// NOTE for Enderal:
				// Health, magicka, and stamina are only modified by auto-scaling based on your chosen class.
				if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
				{
					const auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
					RescaleHMS(a_playerActor, data->firstSavedLevel);
				}
			}
		}
	}

	void GlobalCoopData::ResetMenuCIDs()
	{
		auto& glob = GetSingleton();
		// With no menus open, no player is in control of menus.
		// Otherwise, give P1 control.
		int32_t newCID = Util::MenusOnlyAlwaysOpenInMap() ? -1 : glob.player1CID;
		// Previous menu CID is never -1 after it is first set.
		int32_t newPrevCID = newCID != -1 ? newCID : glob.prevMenuCID;

		// REMOVE when done debugging.
		logger::debug("[GLOB] ResetMenuCIDs: reset menu CID from {} to {}, last menu CID from {} to {}.",
			glob.menuCID, 
			newCID,
			glob.prevMenuCID,
			newPrevCID);

		// REMOVE
		const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
		logger::debug("[GLOB] ResetMenuCIDs: Try to lock: 0x{:X}.", hash);
		{
			std::unique_lock<std::mutex> lock(glob.menuCIDMutex, std::try_to_lock);
			if (lock)
			{
				// REMOVE
				logger::debug("[GLOB] ResetMenuCIDs: Lock obtained: 0x{:X}.", hash);
				glob.prevMenuCID = newPrevCID;
				glob.menuCID = newCID;
			}
			else
			{
				// REMOVE
				logger::debug("[GLOB] ResetMenuCIDs: Failed to obtain lock. Not setting menu CIDs.");
			}
		}
	}

	void GlobalCoopData::SaveUnlockedPerksForAllPlayers()
	{
		auto& glob = GetSingleton();
		if (glob.allPlayersInit && glob.coopSessionActive) 
		{
			for (const auto& p : glob.coopPlayers) 
			{
				if (p->isActive) 
				{
					SaveUnlockedPerksForPlayer(p->coopActor.get());
				}
			}
		}
	}

	void GlobalCoopData::SaveUnlockedPerksForPlayer(RE::Actor* a_coopActor)
	{
		logger::debug("[GLOB] SaveUnlockedPerksForPlayer: {}",
			a_coopActor ? a_coopActor->GetName() : "NONE");
		if (a_coopActor) 
		{
			auto& glob = GetSingleton();
			if (glob.serializablePlayerData.contains(a_coopActor->formID)) 
			{
				auto& serializedData = glob.serializablePlayerData.at(a_coopActor->formID);
				// Save each player's perks to their serializable unlocked perks list.
				std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> savePlayerPerksVisitor = 
				[&serializedData, &glob](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
					if (a_node)
					{
						auto perk = a_node->perk;
						// Save p1's perks to serializable data.
						while (perk)
						{
							bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
							bool nativeFuncHasPerk = a_actor->HasPerk(perk);
							if (a_actor == RE::PlayerCharacter::GetSingleton()) 
							{
								bool singletonListHasPerk = Util::Player1PerkListHasPerk(perk);
								// When not in co-op, the native func check either does not recognize that a perk was added (when first loading a save after starting the game),
								// or captures changes made in a newer save (if loading an older save after choosing new perks). 
								// Reference the glob perk list when out of co-op for these reasons.
								if (nativeFuncHasPerk || singletonListHasPerk)
								{
									// If either check indicates that the player has the current perk, add it to the list.
									if (glob.coopSessionActive) 
									{
										logger::debug("[GLOB] SaveUnlockedPerksForPlayer: {} has perk {} (0x{:X}) (native func: {}, glob list: {})",
											a_actor->GetName(), perk->GetName(), perk->formID, nativeFuncHasPerk, singletonListHasPerk);
										if (nativeFuncHasPerk != singletonListHasPerk)
										{
											logger::debug("[GLOB] ERR: SaveUnlockedPerksForPlayer {} has perk check inconsistency. Adding {} (0x{:X}).",
												a_actor->GetName(), perk->GetName(), perk->formID);
											Util::Player1AddPerk(perk);
										}

										serializedData->InsertUnlockedPerk(perk);
									}
									else if (nativeFuncHasPerk)
									{
										logger::debug("[GLOB] SaveUnlockedPerksForPlayer: NO CO-OP: {} has perk {} (0x{:X}) (native func: {}, glob list: {})",
											a_actor->GetName(), perk->GetName(), perk->formID, nativeFuncHasPerk, singletonListHasPerk);
										// Only add the current perk if it is in the glob list.
										Util::Player1AddPerk(perk);
										serializedData->InsertUnlockedPerk(perk);
									}
								}
							}
							else if (nativeFuncHasPerk)
							{
								logger::debug("[GLOB] SaveUnlockedPerksForPlayer: {} has perk {} (0x{:X}), is shared: {}", a_actor->GetName(), perk->GetName(), perk->formID, shared);
								serializedData->InsertUnlockedPerk(perk);
							}

							perk = perk->nextPerk;
						}
					}
				};

				logger::debug("[GLOB] SaveUnlockedPerksForPlayer BEFORE: {} has {} unlocked perks.",
					a_coopActor->GetName(), serializedData->GetUnlockedPerksList().size());
				// Clear and re-add.
				serializedData->ClearUnlockedPerks();
				Util::TraverseAllPerks(a_coopActor, savePlayerPerksVisitor);
				logger::debug("[GLOB] SaveUnlockedPerksForPlayer AFTER: {} has {} unlocked perks.",
					a_coopActor->GetName(), serializedData->GetUnlockedPerksList().size());
			}
			else
			{
				logger::error("[GLOB] ERR: SaveUnlockedPerksForPlayer: {}: could not get serializable data for player with form ID 0x{:X}.",
					a_coopActor->GetName(), a_coopActor->formID);
			}
		}
		else
		{
			logger::error("[GLOB] ERR: SaveUnlockedPerksForPlayer: player is invalid.");
		}
	}

	void GlobalCoopData::SetCrosshairText()
	{
		// Credits to Ryan-rsm-McKenzie and his quick loot repo for
		// the code on modifying the crosshair text.
		// https://github.com/Ryan-rsm-McKenzie
		// Set crosshair text to the concatenation of all players' notification messages.

		if (auto& glob = GetSingleton(); glob.coopSessionActive && glob.coopPlayers[glob.player1CID]) 
		{
			// Can't concatenate to fixed string, so use a temp string.
			// P1 is always first.
			std::string tempCrosshairText = std::string(glob.coopPlayers[glob.player1CID]->tm->crosshairMessage->text) + "\n";
			// Concatenate the other active players' messages to P1's.
			for (uint8_t i = 0; i < glob.coopPlayers.size(); ++i) 
			{
				if (const auto& p = glob.coopPlayers[i]; p && p->isActive && !p->isPlayer1) 
				{
					tempCrosshairText += std::string(p->tm->crosshairMessage->text) + "\n";
				}
			}

			// Copy over to fixed string and send a copy to task.
			RE::BSFixedString crosshairTextToSet = tempCrosshairText;
			SKSE::GetTaskInterface()->AddUITask([&glob, crosshairTextToSet]() {
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					if (auto hudMenu = ui->GetMenu<RE::HUDMenu>(); hudMenu)
					{
						if (auto view = hudMenu->uiMovie; view)
						{
							RE::GFxValue hudBase;
							view->GetVariable(std::addressof(hudBase), "_root.HUDMovieBaseInstance");
							if (hudBase.IsNull() || hudBase.IsUndefined() || !hudBase.IsObject())
							{
								logger::debug("[GLOB] ERR: SetCrosshairText: HUD base instance is invalid: null: {}, undefined: {}, not an object: {}.",
									hudBase.IsNull(), hudBase.IsUndefined(), !hudBase.IsObject());
							}
							else
							{
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
								if (!rolloverText.IsNull() && !rolloverText.IsUndefined() && rolloverText.IsObject())
								{
									if (rolloverText.HasMember("_alpha"))
									{
										RE::GFxValue alpha;
										rolloverText.GetMember("_alpha", std::addressof(alpha));
										if (alpha.GetNumber() != 100.0)
										{
											alpha.SetNumber(100.0);
											rolloverText.SetMember("_alpha", alpha);
											view->SetVariable("HUDMovieBaseInstance.RolloverText", rolloverText, RE::GFxMovie::SetVarType::kPermanent);
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
											view->SetVariable("HUDMovieBaseInstance.RolloverText", rolloverText, RE::GFxMovie::SetVarType::kPermanent);
										}
									}
								}
							}
						}
						else
						{
							logger::debug("[GLOB] ERR: SetCrosshairText: Could not HUD UI Movie.");
						}
					}
					else
					{
						logger::debug("[GLOB] ERR: SetCrosshairText: Could not get HUD Menu");
					}
				}
				else
				{
					logger::debug("[GLOB] ERR: SetCrosshairText: Could not get UI");
				}
			});
		}
	}

	void GlobalCoopData::SetMenuCIDs(const int32_t a_controllerID)
	{
		auto& glob = GetSingleton();
		if (a_controllerID == -1) 
		{
			ResetMenuCIDs();
		}
		else
		{
			// REMOVE when done debugging.
			logger::debug("[GLOB] SetMenuCIDs: set current/last menu CIDs from {}, {} to {}.",
				glob.menuCID, glob.prevMenuCID, a_controllerID);
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[GLOB] SetMenuCIDs: Try to lock: 0x{:X}.", hash);
			{
				std::unique_lock<std::mutex> lock(glob.menuCIDMutex, std::try_to_lock);
				if (lock)
				{
					// REMOVE
					logger::debug("[GLOB] SetMenuCIDs: Lock obtained: 0x{:X}.", hash);
					glob.prevMenuCID = glob.menuCID = a_controllerID;
				}
				else
				{
					// REMOVE
					logger::debug("[GLOB] SetMenuCIDs: Failed to obtain lock. Not setting menu CIDs.");
				}
			}
		}
	}

	void GlobalCoopData::StopAllCombatOnCoopPlayers(bool&& a_onlyAmongParty, bool&& a_removeCrimeGold)
	{
		auto& glob = GetSingleton();
		if (glob.globalDataInit)
		{
			auto p1 = RE::PlayerCharacter::GetSingleton();
			auto procLists = RE::ProcessLists::GetSingleton(); 
			RE::TESForm* goldObj = nullptr;
			if (auto defObjMgr = RE::BGSDefaultObjectManager::GetSingleton(); defObjMgr)
			{
				goldObj = defObjMgr->objects[RE::DEFAULT_OBJECT::kGold];
			}

			if (!p1 || !procLists || !goldObj) 
			{
				logger::error("[GLOB] ERR: StopAllCombatOnCoopPlayers: P1 is invalid: {}, proc lists is invalid: {}, gold object is invalid: {}.",
					(bool)!p1, (bool)!procLists, (bool)!goldObj);
			}

			auto actorStopCombat =
				[p1, goldObj, procLists, &a_onlyAmongParty, &a_removeCrimeGold](RE::ActorHandle a_actorHandle) {
					if (auto actorPtr = Util::GetActorPtrFromHandle(a_actorHandle); actorPtr)
					{
						if (actorPtr->IsInCombat() || actorPtr->IsHostileToActor(p1))
						{
							// Give P1 an amount of gold equal to their bounty before paying their bounty so that they break even.
							// P1 and co. get off scot-free.
							if (a_removeCrimeGold)
							{
								if (auto base = actorPtr->GetActorBase(); base)
								{
									for (auto& factionInfo : base->factions)
									{
										if (auto faction = factionInfo.faction; faction)
										{
											float crimeGold = faction->GetCrimeGold();
											if (crimeGold > 0 && goldObj && goldObj->IsBoundObject())
											{
												p1->AddObjectToContainer(goldObj->As<RE::TESBoundObject>(), nullptr, crimeGold, p1);
												faction->PlayerPayCrimeGold(false, false);
											}
										}
									}

									auto factionChanges = actorPtr->extraList.GetByType<RE::ExtraFactionChanges>();
									if (factionChanges)
									{
										for (auto& change : factionChanges->factionChanges)
										{
											if (auto faction = change.faction)
											{
												float crimeGold = faction->GetCrimeGold();
												if (crimeGold > 0 && goldObj && goldObj->IsBoundObject())
												{
													p1->AddObjectToContainer(goldObj->As<RE::TESBoundObject>(), nullptr, crimeGold, p1);
													faction->PlayerPayCrimeGold(false, false);
												}
											}
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
								if (!GlobalCoopData::IsCoopPlayer(actorPtr)) 
								{
									actorPtr->NotifyAnimationGraph("attackStop");
								}

								if (actorPtr->combatController)
								{
									actorPtr->combatController->stoppedCombat = true;
								}

								actorPtr->StopCombat();
								actorPtr->currentProcess->lowProcessFlags.reset(RE::AIProcess::LowProcessFlags::kAlert);
							}

							procLists->ClearCachedFactionFightReactions();
						}
					}
				};

			// Stop combat for all actors at each process level, so that there are no straggling NPCs that are still hostile.
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
				if (!a_onlyAmongParty)
				{
					for (const auto& p : glob.coopPlayers)
					{
						if (p && p->isActive && p->coopActor)
						{
							procLists->StopCombatAndAlarmOnActor(p->coopActor.get(), !a_removeCrimeGold);
						}
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
				// Remove all crime gold from vanilla factions.
				// P1 and co. get off scot-free.
				for (auto& [constFaction, _] : p1->crimeGoldMap)
				{
					if (auto faction = RE::TESForm::LookupByID<RE::TESFaction>(constFaction->formID); faction && faction->GetCrimeGold() > 0.0f)
					{
						p1->AddObjectToContainer(goldObj->As<RE::TESBoundObject>(), nullptr, faction->GetCrimeGold(), p1);
						faction->PlayerPayCrimeGold(false, false);
					}
				}
			}
		}
	}

	void GlobalCoopData::StopMenuInputManager()
	{
		auto& glob = GetSingleton();
		if (glob.globalDataInit)
		{
			if (glob.mim->IsRunning())
			{
				glob.mim->ToggleCoopPlayerMenuMode(-1);
				glob.mim->ResetPlayerMenuControlOverlay();
			}

			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[GLOB] StopMenuInputManager: Trying to reset menu CIDs. Thread: 0x{:X}.", hash);
			ResetMenuCIDs();
		}
	}

	void GlobalCoopData::SyncSharedPerks()
	{
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1) 
		{
			logger::error("[GLOB] ERR: SyncSharedPerks: P1 is invalid.");
			return;
		}

		if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
		{
			// For Enderal, just sync shared perks with P1.
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive && !p->isPlayer1)
				{
					for (auto perk : p1->perks)
					{
						// Add any shared perks that P1 has but this player does not have.
						if (!p->coopActor->HasPerk(perk)) 
						{
							logger::debug("[GLOB] SyncSharedPerks: P1 {} has perk {}. Adding to {}.",
								p1->GetName(), perk->GetName(), p->coopActor->GetName());
							Util::ChangePerk(p->coopActor.get(), perk, true);
						}
					}
				}
			}
		}
		else
		{
			// Add all shared skill trees' perks to the co-op player to keep these perks in sync among all players.
			std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> addSharedSkillPerks = 
			[p1, &glob](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
				if (a_node)
				{
					auto perk = a_node->perk;
					while (perk)
					{
						bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
						if (shared && (p1->HasPerk(perk) || Util::Player1PerkListHasPerk(perk)))
						{
							bool hadSharedPerk = true;
							if (glob.serializablePlayerData.contains(a_actor->formID))
							{
								auto& data = glob.serializablePlayerData.at(a_actor->formID);
								if (!data->HasUnlockedPerk(perk))
								{
									data->InsertUnlockedPerk(perk);
									hadSharedPerk = false;
								}
							}

							// REMOVE when done debugging.
							bool succ = Util::ChangePerk(a_actor, perk, true);
							logger::debug("[GLOB] SyncSharedPerks TraversePerkTree. Adding shared perk {} (0x{:X}) to {}: {}. Had unlocked shared perk in list: {}",
								perk->GetName(), perk->formID,
								a_actor->GetName(), succ ? "SUCC" : "FAIL",
								hadSharedPerk);
						}

						perk = perk->nextPerk;
					}
				}
			};

			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive && !p->isPlayer1)
				{
					// Sync all shared skill tree perks.
					Util::TraverseAllPerks(p->coopActor.get(), addSharedSkillPerks);
				}
			}
		}
	}

	void GlobalCoopData::SyncSharedSkillAVs()
	{
		auto& glob = GetSingleton();
		// Sync all shared skill AVs for each player.
		// The highest shared skill level is used 
		// for each shared skill.
		for (const auto& p : glob.coopPlayers)
		{
			if (p->isActive)
			{
				p->pam->CopyOverSharedSkillAVs();
			}
		}
	}

	void GlobalCoopData::TeardownCoopSession(bool a_shouldDismiss)
	{
		auto& glob = GetSingleton();
		// No global data or players, so bail.
		if (!glob.globalDataInit || !glob.allPlayersInit || glob.player1CID < 0 || glob.player1CID >= ALYSLC_MAX_PLAYER_COUNT) 
		{
			return;
		}

		// Set the co-op session as ended.
		glob.coopSessionActive = false;
		if (a_shouldDismiss)
		{
			// Dismiss player 1 last, as the player 1 ref alias script performs final cleanup measures for the co-op session.
			for (const auto& p : glob.coopPlayers)
			{
				if (p && p->isActive && !p->isPlayer1)
				{
					logger::debug("[GLOB] TeardownCoopSession: Co-op session over. Dismissing companion {}. P1 CID is {}.", p->coopActor->GetName(), glob.player1CID);
					p->DismissPlayer();
				}
			}

			const auto& p1 = glob.coopPlayers[glob.player1CID];
			if (p1 && p1.get() && p1->isActive) 
			{
				logger::debug("[GLOB] TeardownCoopSession: Co-op session over. Dismissing player 1 {}.", p1->coopActor->GetName());
				glob.coopPlayers[glob.player1CID]->DismissPlayer();
			}
		}
		else
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (p && p->isActive)
				{
					logger::debug("[GLOB] TeardownCoopSession: Co-op session over. Signalling listener threads to pause for {}.", p->coopActor->GetName());
					p->RequestStateChange(ManagerState::kAwaitingRefresh);
				}
			}
		}

		// Ensure any copied data is reverted for P1.
		if (glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone) 
		{
			logger::debug("[GLOB] TeardownCoopSession: Co-op session ended with data copied (types: 0x{:X}) over to P1. Restoring P1 data.",
				*glob.copiedPlayerDataTypes);
			CopyOverCoopPlayerData(false, "CO-OP SESSION ENDED", glob.player1Actor->GetHandle(), nullptr);
		}

		logger::debug("[GLOB] TeardownCoopSession: Co-op session over. Pausing camera manager and awaiting the start of a new co-op session.");
		glob.cam->RequestStateChange(ManagerState::kPaused);
	}

	void GlobalCoopData::ToggleGodModeForAllPlayers(const bool& a_enable)
	{
		auto& glob = GetSingleton();
		for (const auto& p : glob.coopPlayers) 
		{
			ToggleGodModeForPlayer(p->controllerID, a_enable);
		}
	}

	void GlobalCoopData::ToggleGodModeForPlayer(const int32_t& a_controllerID, const bool& a_enable)
	{
		if (a_controllerID != -1)
		{
			auto& glob = GetSingleton();
			const auto& p = glob.coopPlayers[a_controllerID];
			if (p->isActive) 
			{
				logger::debug("[GLOB] ToggleGodModeForPlayer: Toggling god mode for {} to {}.", p->coopActor->GetName(), a_enable);
				if (p->isPlayer1)
				{
					p->isInGodMode = p->coopActor->IsInvulnerable() && !p->coopActor->IsGhost();
					if ((a_enable && !p->isInGodMode) || (!a_enable && p->isInGodMode))
					{
						logger::debug("[GLOB] ToggleGodModeForPlayer: Should {} god mode for P1.", a_enable ? "set" : "unset");
						const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
						const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
						if (script)
						{
							script->SetCommand("tgm");
							script->CompileAndRun(p->coopActor.get());
							delete script;
							p->isInGodMode = a_enable;
						}
					}

					// Enderal: remove arcane fever related effects, since reaching 100% arcane fever will not
					// kill P1 while in god mode, and will also completely prevent leveling up in the future
					// if the game is saved while at 100% arcane fever.
					if (ALYSLC::EnderalCompat::g_enderalSSEInstalled && p->isInGodMode)
					{
						for (auto effect : *p->coopActor->GetActiveEffectList())
						{
							if (effect)
							{
								if (auto baseObj = effect->GetBaseObject(); (baseObj) && 
									(baseObj->data.primaryAV == RE::ActorValue::kLastFlattered || 
									baseObj->data.secondaryAV == RE::ActorValue::kLastFlattered))
								{
									effect->Dispel(true);
								}
							}
						}

						// Set all 'LastFlattered' AV and AV modifiers to 0.
						p->coopActor->SetActorValue(RE::ActorValue::kLastFlattered, 0.0f);
						p->coopActor->SetBaseActorValue(RE::ActorValue::kLastFlattered, 0.0f);
						float restoreAmount = -p->coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kLastFlattered);
						p->coopActor->As<RE::ActorValueOwner>()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kLastFlattered, restoreAmount);
						restoreAmount = -p->coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kLastFlattered);
						p->coopActor->As<RE::ActorValueOwner>()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kLastFlattered, restoreAmount);
						restoreAmount = -p->coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kLastFlattered);
						p->coopActor->As<RE::ActorValueOwner>()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kLastFlattered, restoreAmount);
						restoreAmount = -p->coopActor->GetActorValue(RE::ActorValue::kLastFlattered);
						p->coopActor->As<RE::ActorValueOwner>()->ModActorValue(RE::ActorValue::kLastFlattered, restoreAmount);
					}
				}
				else
				{
					if (auto actorBase = p->coopActor->GetActorBase(); actorBase)
					{
						auto& baseFlags = actorBase->actorData.actorBaseFlags;
						p->isInGodMode = baseFlags.all(RE::ACTOR_BASE_DATA::Flag::kInvulnerable, RE::ACTOR_BASE_DATA::Flag::kDoesntBleed);
						// Set god mode flag to prevent AV expenditure.
						if (a_enable && !p->isInGodMode)
						{
							logger::debug("[GLOB] ToggleGodModeForPlayer: Set is ghost/invuln/nobleed to TRUE for {}", p->coopActor->GetName());
							baseFlags.set(RE::ACTOR_BASE_DATA::Flag::kInvulnerable, RE::ACTOR_BASE_DATA::Flag::kDoesntBleed);
							p->isInGodMode = true;
						}
						else if (!a_enable && p->isInGodMode)
						{
							logger::debug("[GLOB] ToggleGodModeForPlayer: Set is ghost/invuln/nobleed to FALSE for {}", p->coopActor->GetName());
							baseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kInvulnerable, RE::ACTOR_BASE_DATA::Flag::kDoesntBleed);
							p->isInGodMode = false;
						}
					}
				}
			}
		}
	}

	void GlobalCoopData::UnregisterEvents()
	{
		auto& glob = GetSingleton();
		if (!glob.onCoopHelperMenuRequest.Unregister(glob.player1RefAlias))
		{
			logger::debug("[GLOB] UnregisterEvents: Could not unregister player ref alias ({}) for OnCoopHelperMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str());
		}
		else
		{
			logger::debug("[GLOB] UnregisterEvents: Unregistered OnCoopHelperMenuRequest() event");
		}

		if (!glob.onDebugMenuRequest.Unregister(glob.player1RefAlias))
		{
			logger::debug("[GLOB] UnregisterEvents: Could not unregister player ref alias ({}) for OnDebugMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str());
		}
		else
		{
			logger::debug("[GLOB] UnregisterEvents: Unregistered OnDebugMenuRequest() event");
		}

		if (!glob.onSummoningMenuRequest.Unregister(glob.player1RefAlias))
		{
			logger::debug("[GLOB] UnregisterEvents: Could not unregister player ref alias ({}) for OnSummoningMenuRequest() event",
				glob.player1RefAlias->aliasName.c_str());
		}
		else
		{
			logger::debug("[GLOB] UnregisterEvents: Unregistered OnSummoningMenuRequest() event");
		}
	}

	void GlobalCoopData::UpdateAllSerializedCompanionPlayerFIDKeys()
	{
		// The game might've changed this mod's index between
		// the initial serialization load function call and this function call,
		// meaning that all the co-op companion player's serialized FID keys are invalid.
		// Update them here (must be called before starting co-op).
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			// Co-op companion player actors.
			auto companion1 = dataHandler->LookupForm<RE::Actor>(0x22FD, GlobalCoopData::PLUGIN_NAME)->As<RE::Actor>();
			auto companion2 = dataHandler->LookupForm<RE::Actor>(0x22FE, GlobalCoopData::PLUGIN_NAME)->As<RE::Actor>();
			auto companion3 = dataHandler->LookupForm<RE::Actor>(0x22FF, GlobalCoopData::PLUGIN_NAME)->As<RE::Actor>();

			bool succ1 = GlobalCoopData::UpdateSerializedCompanionPlayerFIDKey(companion1);
			bool succ2 = GlobalCoopData::UpdateSerializedCompanionPlayerFIDKey(companion2);
			bool succ3 = GlobalCoopData::UpdateSerializedCompanionPlayerFIDKey(companion3);
			if (!succ1 || !succ2 || !succ3)
			{
				logger::error("[Proxy] ERR: UpdateAllSerializedCompanionPlayerFIDKeys: Failed to update serialized FID key for {}: {}, {}: {}, {}: {}.",
					companion1 ? companion1->GetName() : "NONE", !succ1,
					companion2 ? companion2->GetName() : "NONE", !succ2,
					companion3 ? companion3->GetName() : "NONE", !succ3);
			}
		}
	}

	bool GlobalCoopData::UpdateSerializedCompanionPlayerFIDKey(RE::Actor* a_playerActor)
	{
		if (a_playerActor) 
		{
			auto& glob = GetSingleton();
			// Serializable data:
			// Ensure that the actor's updated FID is used as the key for accessing their serializable data.
			// Extract the node and update its key if this mod's position has changed in the load order.
			if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
			{
				std::vector<RE::FormID> keys;
				for (auto& [k, _] : glob.serializablePlayerData)
				{
					keys.emplace_back(k);
				}

				for (auto& formID : keys)
				{
					// FIDs do not match and the mod-index-independent portion of the FIDs match.
					bool newActorFID = formID != a_playerActor->formID && (formID & 0xFFFFFF) == (a_playerActor->formID & 0xFFFFFF);
					if (newActorFID)
					{
						logger::debug("[GLOB] UpdateSerializedCompanionPlayerFIDKey: {}'s FID went from 0x{:X} to 0x{:X}, inserting new FID key into serializable data now.",
							a_playerActor->GetName(), formID, a_playerActor->formID, newActorFID);
						auto node = glob.serializablePlayerData.extract(formID);
						node.key() = a_playerActor->formID;
						glob.serializablePlayerData.insert(std::move(node));
					}
				}
			}

			return glob.serializablePlayerData.contains(a_playerActor->formID);
		}

		return false;
	}

	void GlobalCoopData::YouDied(RE::Actor* a_deadPlayer)
	{
		// NOTE: P1 kill calls still fail at times.
		// One example being when all other players die while P1 is getting up after being revived.
		// Use the 'player.kill' console command or use the Debug Menu's 'Reset Equip State' option on P1
		// to properly end the co-op session, since P1 will remain paralyzed on the ground otherwise.
		auto& glob = GetSingleton();
		// No living players and either called on a particular dead co-op player or on the whole party.
		if ((glob.livingPlayers > 0) && (!a_deadPlayer || IsCoopPlayer(a_deadPlayer)))
		{
			glob.livingPlayers = 0;
			RE::DebugMessageBox("Your party was bested this time.\n\nOne thread of fate severed, another thread spun.");
			logger::debug("[GLOB] YouDied: All players downed or dead. Ending co-op session.");
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive)
				{
					// Make sure god mode is disabled for each player first; otherwise, they won't die below.
					if (p->isInGodMode) 
					{
						GlobalCoopData::ToggleGodModeForPlayer(p->controllerID, false);
						Util::StopEffectShader(p->coopActor.get(), glob.ghostFXShader);
					}

					// Revert any active transformation.
					if (p->isTransforming || p->isTransformed)
					{
						logger::debug("[GLOB] YouDied: {} is transformed into a {}. Reverting before removal.", p->coopActor->GetName(), p->coopActor->race ? p->coopActor->race->formEditorID : "NONE");
						p->RevertTransformation();
					}

					if (auto actorBase = p->coopActor->GetActorBase(); actorBase)
					{
						auto currentHealth = p->coopActor->GetActorValue(RE::ActorValue::kHealth);
						logger::debug("[GLOB] YouDied: Removing {}. Current health: {}", p->coopActor->GetName(), currentHealth);
						// Reset essential flags before killing actor.
						Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kEssential, false);
						// Ragdoll.
						Util::PushActorAway(p->coopActor.get(), p->coopActor->data.location, -1.0f, true);
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

						// Kill calls fail on P1 at times, especially when the player dies in water, and the game will not reload.
						// The kill console command appears to work more often when this happens, so as an extra layer of insurance,
						// run that command here.
						const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
						const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
						if (script)
						{
							script->SetCommand("kill");
							script->CompileAndRun(p->coopActor.get());
							delete script;
						}

						// Also run the other kill functions and set life state to dead.
						p->coopActor->KillImpl(p->coopActor.get(), FLT_MAX, false, false);
						p->coopActor->KillImmediate();
						p->coopActor->SetLifeState(RE::ACTOR_LIFE_STATE::kDead);
						currentHealth = p->coopActor->GetActorValue(RE::ActorValue::kHealth);
						logger::debug("[GLOB] YouDied: Removing {}. Post-'death' health: {}", p->coopActor->GetName(), currentHealth);
					}
					else
					{
						logger::error("[GLOB] ERR: YouDied: could not get actor base for {}",
							p->coopActor->GetName());
					}
				}
			}

			// Reset skill gain multiplier since there are no living players in the party now.
			ModifyXPPerSkillLevelMult(false);
			// Teardown the session afterward.
			TeardownCoopSession(true);

			// If all else fails, reload the most recent save after a short period of time.
			// This is making me go insane.
			if (auto saveLoadManager = RE::BGSSaveLoadManager::GetSingleton(); saveLoadManager) 
			{
				std::jthread reloadTask(
					[]() {
						const auto& glob = GlobalCoopData::GetSingleton();
						auto ui = RE::UI::GetSingleton();
						auto p1 = RE::PlayerCharacter::GetSingleton();
						auto saveLoadManager = RE::BGSSaveLoadManager::GetSingleton(); 
						// If players are still alive or any singletons are invalid, return early.
						if (glob.livingPlayers > 0 || !ui || !p1 || !saveLoadManager)
						{
							logger::debug("[GLOB EXT] ReloadTask: Players still alive: {}, UI invalid: {}, P1 invalid: {}, SaveLoadManager invalid: {}.",
								glob.livingPlayers > 0, (bool)!ui, (bool)!p1, (bool)!saveLoadManager);
							return;
						}

						logger::debug("[GLOB EXT] BEFORE ReloadTask: Waiting for the game to reload the last save. Co-op session active: {}, p1 dead: {}, loading menu open: {}.",
							glob.coopSessionActive,
							p1->IsDead(),
							ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME));

						const float maxSecsToWait = 10.0f;
						float secsWaited = 0.0f;
						SteadyClock::time_point waitTP = SteadyClock::now();
						// Wait at most 10 seconds without a loading screen opening before loading the most recent save.
						while (secsWaited < maxSecsToWait && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))
						{
							secsWaited = Util::GetElapsedSeconds(waitTP);
						}

						if (secsWaited >= maxSecsToWait) 
						{
							logger::debug("[GLOB EXT] ReloadTask: Loading most recent save game after {} seconds.", secsWaited);
							saveLoadManager->LoadMostRecentSaveGame();
						}

						logger::debug("[GLOB EXT] AFTER ReloadTask: Waiting for the game to reload the last save. Co-op session active: {}, p1 dead: {}, loading menu open: {}.",
							glob.coopSessionActive,
							p1->IsDead(),
							ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME));
					});
				reloadTask.detach();
			}
			else
			{
				// Delayed async check to make sure P1 dies to trigger the LoadingMenu,
				// since the game still fails to reload at times.
				// P1 is set as killed above, but then is sometimes alive when checked later via console command (?).
				std::jthread killTask(
				[]() {
					const auto& glob = GlobalCoopData::GetSingleton();
					auto ui = RE::UI::GetSingleton();
					auto p1 = RE::PlayerCharacter::GetSingleton();
					if (!ui || !p1 || glob.livingPlayers > 0)
					{
						logger::debug("[GLOB EXT] KillTask: UI invalid: {}, P1 invalid: {}, players still alive: {}.",
							!ui, !p1, glob.livingPlayers > 0);
						return;
					}

					const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
					const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
					logger::debug("[GLOB EXT] BEFORE KillTask: Waiting for P1 to die. Co-op session active: {}, p1 dead: {}, loading menu open: {}. Full reset: {}, reset game: {}, reload content: {}.",
						glob.coopSessionActive,
						p1->IsDead(),
						ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME),
						RE::Main::GetSingleton()->fullReset, RE::Main::GetSingleton()->resetGame, RE::Main::GetSingleton()->reloadContent);
					
					while (!glob.coopSessionActive && (!p1->IsDead() || !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)))
					{
						logger::debug("[GLOB EXT] DURING KillTask: Waiting for P1 to die. Co-op session active: {}, p1 dead: {}, loading menu open: {}. Full reset: {}, reset game: {}, reload content: {}.",
							glob.coopSessionActive,
							p1->IsDead(),
							ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME),
							RE::Main::GetSingleton()->fullReset, RE::Main::GetSingleton()->resetGame, RE::Main::GetSingleton()->reloadContent);
						
						// NotifyAnimationGraph() hook for P1 will attempt to kill P1 once no other players are alive.
						p1->NotifyAnimationGraph("GetUpBegin");
						std::this_thread::sleep_for(std::chrono::seconds(static_cast<long long>(*g_deltaTimeRealTime)));
					}

					logger::debug("[GLOB EXT] AFTER KillTask: Waiting for P1 to die. Co-op session active: {}, p1 dead: {}, loading menu open: {}. Full reset: {}, reset game: {}, reload content: {}.",
						glob.coopSessionActive,
						p1->IsDead(),
						ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME),
						RE::Main::GetSingleton()->fullReset, RE::Main::GetSingleton()->resetGame, RE::Main::GetSingleton()->reloadContent);
					if (script) 
					{
						delete script;
					}
				});
				killTask.detach();
			}
		}
	}

	void GlobalCoopData::CopyPlayerData(const std::unique_ptr<CopyPlayerDataRequestInfo>& a_info)
	{
		// Copy over player data from co-op player to player 1.
		// What's copied is dependent on both the requested menu and if the menu is opening or closing.
		auto requestingPlayer = Util::GetActorPtrFromHandle(a_info->requestingPlayerHandle);
		if (!requestingPlayer)
		{
			logger::error("[GLOB] ERR: CopyPlayerData: Requesting player is invalid. Cannot {} data.", a_info->shouldImport ? "import" : "export");
			return;
		}

		// REMOVE
		logger::debug("[GLOB] CopyPlayerData: Request to copy player data for {} on {} of {}.",
			requestingPlayer->GetName(),
			a_info->shouldImport ? "opening" : "closing",
			a_info->menuName);

		auto& glob = GetSingleton();
		const auto menuNameHash = Hash(a_info->menuName);
		const auto& p = glob.coopPlayers[GetCoopPlayerIndex(requestingPlayer.get())];
		if (auto ui = RE::UI::GetSingleton(); ui)
		{
			// Prevent saving when co-op player data is copied onto player 1.
			// Modify HUD allow saving flag, since it is almost always open during gameplay,
			// and if one menu on the stack has its 'kAllowSaving' flag unset, saving is disabled.
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
					logger::debug("[GLOB] CopyPlayerData: Enderal Hero Menu: Should copy over AVs and name.");
					if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
					{
						logger::debug("[GLOB] CopyPlayerData: Import Name.");
						CopyOverActorBaseData(requestingPlayer.get(), a_info->shouldImport, true, false, false);
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kName);
					}

					if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
					{
						logger::debug("[GLOB] CopyPlayerData: Import AVs.");
						CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kSkillsAndHMS);
					}
				}
				else
				{
					logger::debug("[GLOB] CopyPlayerData: Enderal Hero Menu: Should restore AVs and name.");
					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
					{
						logger::debug("[GLOB] CopyPlayerData: Export Name.");
						CopyOverActorBaseData(requestingPlayer.get(), a_info->shouldImport, true, false, false);
						glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kName);
					}

					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
					{
						logger::debug("[GLOB] CopyPlayerData: Export AVs.");
						CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
					}
				}
			}
			else if (menuNameHash == Hash(RE::BarterMenu::MENU_NAME))
			{
				if (a_info->shouldImport)
				{
					logger::debug("[GLOB] CopyPlayerData: Barter Menu: Should copy over inventory on import.");
					if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
					{
						logger::debug("[GLOB] CopyPlayerData: Import Inventory.");
						CopyOverInventories(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kInventory);
					}
				}
				else
				{
					logger::debug("[GLOB] CopyPlayerData: Barter Menu: Should copy back inventory on export.");
					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
					{
						logger::debug("[GLOB] CopyPlayerData: Export Inventory.");
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
					logger::debug("[GLOB] CopyPlayerData: Container Menu: Should copy over AVs, name, carryweight, and perk list.");
					if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kCarryWeight))
					{
						logger::debug("[GLOB] CopyPlayerData: Import Carryweight.");
						CopyOverActorBaseData(requestingPlayer.get(), a_info->shouldImport, false, false, true);
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kCarryWeight);
					}

					if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkList))
					{
						logger::debug("[GLOB] CopyPlayerData: Import Perk list.");
						CopyOverPerkLists(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kPerkList);
					}

					if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
					{
						logger::debug("[GLOB] CopyPlayerData: Import AVs.");
						CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kSkillsAndHMS);
					}
				}
				else
				{
					logger::debug("[GLOB] CopyPlayerData: Container Menu: Should restore AVs, name, carryweight, and perk list.");
					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kCarryWeight))
					{
						logger::debug("[GLOB] CopyPlayerData: Export Carryweight.");
						CopyOverActorBaseData(requestingPlayer.get(), a_info->shouldImport, false, false, true);
						glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kCarryWeight);
					}

					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkList))
					{
						logger::debug("[GLOB] CopyPlayerData: Export Perk List.");
						CopyOverPerkLists(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kPerkList);
					}

					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
					{
						logger::debug("[GLOB] CopyPlayerData: Export AVs.");
						CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
					}
				}
			}
			else if (menuNameHash == Hash(RE::CraftingMenu::MENU_NAME))
			{
				// TODO: Don't copy the entire inventory every time.
				// Only copy categories of items dependent on the crafting menu's linked furniture type.
				// For now, the entire inventory is copied over to P1.
				if (a_info->shouldImport)
				{
					logger::debug("[GLOB] CopyPlayerData: Crafting Menu: Should copy over inventory on import.");
					if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
					{
						logger::debug("[GLOB] CopyPlayerData: Import Inventory.");
						CopyOverInventories(requestingPlayer.get(), a_info->shouldImport);
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kInventory);
					}
				}
				else
				{
					logger::debug("[GLOB] CopyPlayerData: Crafting Menu: Should copy back inventory on export.");
					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
					{
						logger::debug("[GLOB] CopyPlayerData: Export Inventory.");
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
					logger::debug("[GLOB] CopyPlayerData: Favorites Menu: Should import {}'s favorites to P1.", requestingPlayer.get()->GetName());
					if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kFavorites))
					{
						logger::debug("[GLOB] CopyPlayerData: Import Favorites to P1.");
						p->em->ImportCoopFavorites();
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kFavorites);
					}
				}
				else
				{
					// Revert changes to player 1's favorites if the favorites menu is closing.
					logger::debug("[GLOB] CopyPlayerData: Favorites Menu: Should remove {}'s favorites from P1 and re-favorite P1's cached favorites.", requestingPlayer.get()->GetName());
					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kFavorites))
					{
						logger::debug("[GLOB] CopyPlayerData: Restore P1 Favorites.");
						p->em->RestoreP1Favorites();
						glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kFavorites);
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
					logger::debug("[GLOB] CopyPlayerData: Adjust perk data for {} before entering the Stats Menu.", requestingPlayer->GetName());
					AdjustPerkDataForCompanionPlayer(requestingPlayer.get(), a_info->shouldImport);
				}
			}
			else if (menuNameHash == Hash(RE::TrainingMenu::MENU_NAME))
			{
				// Dialogue NPC is trainer/vendor.
				bool isTrainer = false;
				if (a_info->assocForm)
				{
					if (auto asActor = a_info->assocForm->As<RE::Actor>(); asActor && asActor->GetActorBase())
					{
						if (auto npcClass = asActor->GetActorBase()->npcClass; npcClass && npcClass->data.maximumTrainingLevel != 0)
						{
							logger::debug("[GLOB] CopyPlayerData: Dialogue NPC {} is a trainer: {} to level {}.",
								asActor->GetName(), *npcClass->data.teaches, npcClass->data.maximumTrainingLevel);
							isTrainer = true;
						}
					}
				}

				if (isTrainer)
				{
					// Copy over AVs.
					if (a_info->shouldImport)
					{
						logger::debug("[GLOB] CopyPlayerData: Trainer: Should copy over AVs on import.");
						if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
						{
							logger::debug("[GLOB] CopyPlayerData: Import AVs.");
							CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
							glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kSkillsAndHMS);
						}
					}
					else
					{
						logger::debug("[GLOB] CopyPlayerData: Trainer: Should copy back AVs on export.");
						if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
						{
							logger::debug("[GLOB] CopyPlayerData: Export AVs.");
							CopyOverAVs(requestingPlayer.get(), a_info->shouldImport);
							glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
						}
					}
				}
			}

			// Clear all copied data flags on export if no supported menus are open.
			if (auto ui = RE::UI::GetSingleton(); ui) 
			{
				bool supportedMenusClosed = std::find_if(COPY_PLAYER_DATA_MENU_NAMES.begin(), COPY_PLAYER_DATA_MENU_NAMES.end(),
												[ui, &a_info](const std::string_view& a_menuName) {
													// All closed except the current menu (which is opening/closing right now).
													return a_menuName != a_info->menuName && ui->IsMenuOpen(a_menuName);
												}) == COPY_PLAYER_DATA_MENU_NAMES.end();
				// Failsafe if multiple menus close before a single copy-data export task is run here.
				// Ensure all P1's data is restored based off the previously-imported data types.
				if (!a_info->shouldImport && (supportedMenusClosed || !glob.coopSessionActive))
				{
					logger::debug("[GLOB] CopyPlayerData: All supported menus closed or co-op session ended. Clearing copyable data types.");
					if (*glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone)
					{
						logger::critical("[GLOB] CopyPlayerData: WARNING: still uncleared data types on export: 0x{:X}. Clearing now", *glob.copiedPlayerDataTypes);
						if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kCarryWeight))
						{
							logger::debug("[GLOB] CopyPlayerData: Restore P1 Carryweight.");
							CopyOverActorBaseData(requestingPlayer.get(), false, false, false, true);
							glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kCarryWeight);
						}

						if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kFavorites))
						{
							logger::debug("[GLOB] CopyPlayerData: Restore P1 Favorites.");
							p->em->RestoreP1Favorites();
							glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kFavorites);
						}

						if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
						{
							logger::debug("[GLOB] CopyPlayerData: Restore P1 Inventory.");
							CopyOverInventories(requestingPlayer.get(), false);
							glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kInventory);
						}

						if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
						{
							logger::debug("[GLOB] CopyPlayerData: Restore P1 Name.");
							CopyOverActorBaseData(requestingPlayer.get(), false, true, false, false);
							glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kName);
						}

						if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkList))
						{
							logger::debug("[GLOB] CopyPlayerData: Restore P1 Perk List.");
							CopyOverPerkLists(requestingPlayer.get(), false);
							glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kPerkList);
						}

						if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kPerkTree))
						{
							logger::debug("[GLOB] CopyPlayerData: Restore P1 Perk Tree.");
							CopyOverPerkTrees(requestingPlayer.get(), false);
							glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kPerkTree);
						}

						if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kRaceName))
						{
							logger::debug("[GLOB] CopyPlayerData: Restore P1 Race Name.");
							CopyOverActorBaseData(requestingPlayer.get(), false, false, true, false);
							glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kRaceName);
						}

						if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
						{
							logger::debug("[GLOB] CopyPlayerData: Restore P1 AVs.");
							CopyOverAVs(requestingPlayer.get(), false);
							glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
						}
					}

					glob.copiedPlayerDataTypes = CopyablePlayerDataTypes::kNone;
				}
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
	}

	void GlobalCoopData::CopyOverActorBaseData(RE::Actor* a_coopActor, const bool& a_shouldImport, bool&& a_name, bool&& a_raceName, bool&& a_carryWeight)
	{
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor) 
		{
			logger::error("[GLOB] ERR: CopyOverActorBaseData: P1 is invalid: {}, requesting player is invalid: {}.",
				(bool)!p1, (bool)!a_coopActor);
			return;
		}

		if (a_shouldImport)
		{
			if (a_name)
			{
				// Save P1 and co-op player names before setting P1's full name to the co-op player's.
				std::string_view p1Name = p1->GetDisplayFullName();
				std::string_view coopCompanionName = a_coopActor->GetDisplayFullName();
				glob.p1ExchangeableData->name = p1Name;
				glob.coopCompanionExchangeableData->name = coopCompanionName;
				p1->GetObjectReference()->As<RE::TESFullName>()->fullName = glob.coopCompanionExchangeableData->name;
				logger::debug("[GLOB] CopyOverActorBaseData: Storing p1, coop actor names: {}, {}",
					glob.p1ExchangeableData->name, glob.coopCompanionExchangeableData->name);
			}

			if (a_raceName)
			{
				// Save P1 and co-op player race names before setting P1's race name to the co-op player's.
				std::string_view p1RaceName = p1->GetRace()->fullName;
				std::string_view coopCompanionRaceName = a_coopActor->GetRace()->fullName;
				glob.p1ExchangeableData->raceName = p1RaceName;
				glob.coopCompanionExchangeableData->raceName = coopCompanionRaceName;
				p1->GetRace()->fullName = glob.coopCompanionExchangeableData->raceName;
				logger::debug("[GLOB] CopyOverActorBaseData: Storing p1, coop actor race names: {}, {}",
					glob.p1ExchangeableData->raceName, glob.coopCompanionExchangeableData->raceName);
			}

			if (a_carryWeight) 
			{
				// Save carryweight AV and AV mods before swapping the two.
				glob.coopCompanionExchangeableData->carryWeightAVData = { 
					a_coopActor->GetActorValue(RE::ActorValue::kCarryWeight),
					a_coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight),
					a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight),
					a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight),
					a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight)
				};
				glob.p1ExchangeableData->carryWeightAVData = {
					p1->GetActorValue(RE::ActorValue::kCarryWeight),
					p1->GetBaseActorValue(RE::ActorValue::kCarryWeight),
					p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight),
					p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight),
					p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight)
				};

				p1->SetActorValue(RE::ActorValue::kCarryWeight, glob.coopCompanionExchangeableData->carryWeightAVData[0]);
				p1->SetBaseActorValue(RE::ActorValue::kCarryWeight, glob.coopCompanionExchangeableData->carryWeightAVData[1]);
				p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, glob.coopCompanionExchangeableData->carryWeightAVData[2] - glob.p1ExchangeableData->carryWeightAVData[2]);
				p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight, glob.coopCompanionExchangeableData->carryWeightAVData[3] - glob.p1ExchangeableData->carryWeightAVData[3]);
				p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, glob.coopCompanionExchangeableData->carryWeightAVData[4] - glob.p1ExchangeableData->carryWeightAVData[4]);
				logger::debug("[GLOB] CopyOverActorBaseData: Adjust: saved p1, coop player carryweights: ({}, {}, {}, {}, {}), ({}, {}, {}, {}, {}). P1's carryweight now set to {}.",
					glob.p1ExchangeableData->carryWeightAVData[0],
					glob.p1ExchangeableData->carryWeightAVData[1],
					glob.p1ExchangeableData->carryWeightAVData[2],
					glob.p1ExchangeableData->carryWeightAVData[3],
					glob.p1ExchangeableData->carryWeightAVData[4], 
					glob.coopCompanionExchangeableData->carryWeightAVData[0],
					glob.coopCompanionExchangeableData->carryWeightAVData[1],
					glob.coopCompanionExchangeableData->carryWeightAVData[2],
					glob.coopCompanionExchangeableData->carryWeightAVData[3],
					glob.coopCompanionExchangeableData->carryWeightAVData[4],
					p1->GetActorValue(RE::ActorValue::kCarryWeight));
			}
		}
		else
		{
			// Restore full name and/or race name.
			if (a_name)
			{
				logger::debug("[GLOB] CopyOverActorBaseData: Restoring '{}' (from cached imported data) as player 1's name.", glob.p1ExchangeableData->name);
				p1->GetObjectReference()->As<RE::TESFullName>()->fullName = glob.p1ExchangeableData->name;
			}

			if (a_raceName)
			{
				logger::debug("[GLOB] CopyOverActorBaseData: Restoring '{}' as player 1's race name.", glob.p1ExchangeableData->raceName);
				p1->GetRace()->fullName = glob.p1ExchangeableData->raceName;
			}

			if (a_carryWeight)
			{
				// Swap saved carryweight AV and AV mods.
				p1->SetActorValue(RE::ActorValue::kCarryWeight, glob.p1ExchangeableData->carryWeightAVData[0]);
				p1->SetBaseActorValue(RE::ActorValue::kCarryWeight, glob.p1ExchangeableData->carryWeightAVData[1]);
				p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, glob.p1ExchangeableData->carryWeightAVData[2] - p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight));
				p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight, glob.p1ExchangeableData->carryWeightAVData[3] - p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight));
				p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, glob.p1ExchangeableData->carryWeightAVData[4] - p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight));
				logger::debug("[GLOB] CopyOverActorBaseData: Restore: saved p1, coop player carryweights: ({}, {}, {}, {}, {}), ({}, {}, {}, {}, {}). P1's carryweight now set to {}.",
					glob.p1ExchangeableData->carryWeightAVData[0],
					glob.p1ExchangeableData->carryWeightAVData[1],
					glob.p1ExchangeableData->carryWeightAVData[2],
					glob.p1ExchangeableData->carryWeightAVData[3],
					glob.p1ExchangeableData->carryWeightAVData[4],
					glob.coopCompanionExchangeableData->carryWeightAVData[0],
					glob.coopCompanionExchangeableData->carryWeightAVData[1],
					glob.coopCompanionExchangeableData->carryWeightAVData[2],
					glob.coopCompanionExchangeableData->carryWeightAVData[3],
					glob.coopCompanionExchangeableData->carryWeightAVData[4],
					p1->GetActorValue(RE::ActorValue::kCarryWeight));
			}
		}
	}

	void GlobalCoopData::CopyOverAVs(RE::Actor* a_coopActor, const bool& a_shouldImport, const bool& a_shouldCopyChanges)
	{
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor)
		{
			logger::error("[GLOB] ERR: CopyOverAVs: P1 is invalid: {}, requesting player is invalid: {}.",
				(bool)!p1, (bool)!a_coopActor);
			return;
		}

		if (a_shouldImport)
		{
			// Save player 1 AV and AV mods on entry.
			glob.p1ExchangeableData->hmsBaseAVs = {
				p1->GetBaseActorValue(RE::ActorValue::kHealth),
				p1->GetBaseActorValue(RE::ActorValue::kMagicka),
				p1->GetBaseActorValue(RE::ActorValue::kStamina)
			};

			glob.p1ExchangeableData->hmsAVs = {
				p1->GetActorValue(RE::ActorValue::kHealth),
				p1->GetActorValue(RE::ActorValue::kMagicka),
				p1->GetActorValue(RE::ActorValue::kStamina)
			};

			std::array<float, 3> tempHealthMods{
				p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth),
				p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth),
				p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth)
			};
			std::array<float, 3> tempMagickaMods = {
				p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka),
				p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka),
				p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka)
			};
			std::array<float, 3> tempStaminaMods = {
				p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina),
				p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina),
				p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina)
			};

			// Temporary (buff/debuff) and permanent (ex. modav) changes to the HMS actor values.
			glob.p1ExchangeableData->hmsAVMods = {
				tempHealthMods, tempMagickaMods, tempStaminaMods
			};

			glob.coopCompanionExchangeableData->hmsBaseAVs = {
				a_coopActor->GetBaseActorValue(RE::ActorValue::kHealth),
				a_coopActor->GetBaseActorValue(RE::ActorValue::kMagicka),
				a_coopActor->GetBaseActorValue(RE::ActorValue::kStamina)
			};

			glob.coopCompanionExchangeableData->hmsAVs = {
				a_coopActor->GetActorValue(RE::ActorValue::kHealth),
				a_coopActor->GetActorValue(RE::ActorValue::kMagicka),
				a_coopActor->GetActorValue(RE::ActorValue::kStamina)
			};

			tempHealthMods = {
				a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth),
				a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth),
				a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth)
			};
			tempMagickaMods = {
				a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka),
				a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka),
				a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka)
			};
			tempStaminaMods = {
				a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina),
				a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina),
				a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina)
			};

			glob.coopCompanionExchangeableData->hmsAVMods = {
				tempHealthMods, tempMagickaMods, tempStaminaMods
			};

			logger::debug("[GLOB] CopyOverAVs: Setting player 1's Health base/normal AV to {}, {}.",
				glob.coopCompanionExchangeableData->hmsBaseAVs[0], glob.coopCompanionExchangeableData->hmsAVs[0]);
			p1->SetActorValue(RE::ActorValue::kHealth, glob.coopCompanionExchangeableData->hmsAVs[0]);
			p1->SetBaseActorValue(RE::ActorValue::kHealth, glob.coopCompanionExchangeableData->hmsBaseAVs[0]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, glob.coopCompanionExchangeableData->hmsAVMods[0][0] - glob.p1ExchangeableData->hmsAVMods[0][0]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth, glob.coopCompanionExchangeableData->hmsAVMods[0][1] - glob.p1ExchangeableData->hmsAVMods[0][1]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth, glob.coopCompanionExchangeableData->hmsAVMods[0][2] - glob.p1ExchangeableData->hmsAVMods[0][2]);
			logger::debug("[GLOB] CopyOverAVs: Setting player 1's Magicka base/normal AV to {}, {}.",
				glob.coopCompanionExchangeableData->hmsBaseAVs[1], glob.coopCompanionExchangeableData->hmsAVs[1]);
			p1->SetActorValue(RE::ActorValue::kMagicka, glob.coopCompanionExchangeableData->hmsAVs[1]);
			p1->SetBaseActorValue(RE::ActorValue::kMagicka, glob.coopCompanionExchangeableData->hmsBaseAVs[1]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, glob.coopCompanionExchangeableData->hmsAVMods[1][0] - glob.p1ExchangeableData->hmsAVMods[1][0]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka, glob.coopCompanionExchangeableData->hmsAVMods[1][1] - glob.p1ExchangeableData->hmsAVMods[1][1]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka, glob.coopCompanionExchangeableData->hmsAVMods[1][2] - glob.p1ExchangeableData->hmsAVMods[1][2]);
			logger::debug("[GLOB] CopyOverAVs: Setting player 1's Stamina base/normal AV to {}, {}.",
				glob.coopCompanionExchangeableData->hmsBaseAVs[2], glob.coopCompanionExchangeableData->hmsAVs[2]);
			p1->SetActorValue(RE::ActorValue::kStamina, glob.coopCompanionExchangeableData->hmsAVs[2]);
			p1->SetBaseActorValue(RE::ActorValue::kStamina, glob.coopCompanionExchangeableData->hmsBaseAVs[2]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, glob.coopCompanionExchangeableData->hmsAVMods[2][0] - glob.p1ExchangeableData->hmsAVMods[2][0]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina, glob.coopCompanionExchangeableData->hmsAVMods[2][1] - glob.p1ExchangeableData->hmsAVMods[2][1]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina, glob.coopCompanionExchangeableData->hmsAVMods[2][2] - glob.p1ExchangeableData->hmsAVMods[2][2]);

			// Skills next.
			auto currentAV = RE::ActorValue::kNone;
			for (auto i = 0; i < Skill::kTotal; ++i)
			{
				// Ignore shared skill AVs, since these are already synced.
				currentAV = SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
				if (!SHARED_SKILL_AVS_SET.contains(currentAV))
				{
					glob.coopCompanionExchangeableData->skillAVs[i] = a_coopActor->GetBaseActorValue(currentAV);
					glob.coopCompanionExchangeableData->skillAVMods[0][i] = a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, currentAV);
					glob.coopCompanionExchangeableData->skillAVMods[1][i] = a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, currentAV);
					glob.coopCompanionExchangeableData->skillAVMods[2][i] = a_coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, currentAV);
					glob.p1ExchangeableData->skillAVs[i] = p1->GetBaseActorValue(currentAV);
					glob.p1ExchangeableData->skillAVMods[0][i] = p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, currentAV);
					glob.p1ExchangeableData->skillAVMods[1][i] = p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, currentAV);
					glob.p1ExchangeableData->skillAVMods[2][i] = p1->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, currentAV);

					logger::debug("[GLOB] CopyOverAVs: Setting player 1's {} base AV to {}, was {}. Setting temp modifiers to ({}, {}, {}), were ({}, {}, {}), diffs: ({}, {}, {}).",
						std::format("{}", currentAV), 
						glob.coopCompanionExchangeableData->skillAVs[i], 
						glob.p1ExchangeableData->skillAVs[i], 
						glob.coopCompanionExchangeableData->skillAVMods[0][i],
						glob.coopCompanionExchangeableData->skillAVMods[1][i],
						glob.coopCompanionExchangeableData->skillAVMods[2][i],
						glob.p1ExchangeableData->skillAVMods[0][i],
						glob.p1ExchangeableData->skillAVMods[1][i],
						glob.p1ExchangeableData->skillAVMods[2][i],
						glob.coopCompanionExchangeableData->skillAVMods[0][i] - glob.p1ExchangeableData->skillAVMods[0][i],
						glob.coopCompanionExchangeableData->skillAVMods[1][i] - glob.p1ExchangeableData->skillAVMods[1][i],
						glob.coopCompanionExchangeableData->skillAVMods[2][i] - glob.p1ExchangeableData->skillAVMods[2][i]);
					p1->SetBaseActorValue(currentAV, glob.coopCompanionExchangeableData->skillAVs[i]);
					p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, currentAV, glob.coopCompanionExchangeableData->skillAVMods[0][i] - glob.p1ExchangeableData->skillAVMods[0][i]);
					p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, currentAV, glob.coopCompanionExchangeableData->skillAVMods[1][i] - glob.p1ExchangeableData->skillAVMods[1][i]);
					p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, currentAV, glob.coopCompanionExchangeableData->skillAVMods[2][i] - glob.p1ExchangeableData->skillAVMods[2][i]);
				}
			}
		}
		else
		{
			std::vector<float> newExportedBaseAVs = {
				p1->GetBaseActorValue(RE::ActorValue::kHealth),
				p1->GetBaseActorValue(RE::ActorValue::kMagicka),
				p1->GetBaseActorValue(RE::ActorValue::kStamina)
			};

			std::vector<float> newExportedAVs = {
				p1->GetActorValue(RE::ActorValue::kHealth),
				p1->GetActorValue(RE::ActorValue::kMagicka),
				p1->GetActorValue(RE::ActorValue::kStamina)
			};

			// Restore saved player 1 AVs, AV mods.
			logger::debug("[GLOB] CopyOverAVs: Resetting player 1's {} base/normal AV to {}, {}, {}'s base/normal AV to {}, {}.",
				RE::ActorValue::kHealth, glob.p1ExchangeableData->hmsBaseAVs[0], glob.p1ExchangeableData->hmsAVs[0],
				a_coopActor->GetName(), newExportedBaseAVs[0], newExportedAVs[0]);
			p1->SetActorValue(RE::ActorValue::kHealth, glob.p1ExchangeableData->hmsAVs[0]);
			p1->SetBaseActorValue(RE::ActorValue::kHealth, glob.p1ExchangeableData->hmsBaseAVs[0]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, glob.p1ExchangeableData->hmsAVMods[0][0] - glob.coopCompanionExchangeableData->hmsAVMods[0][0]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth, glob.p1ExchangeableData->hmsAVMods[0][1] - glob.coopCompanionExchangeableData->hmsAVMods[0][1]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth, glob.p1ExchangeableData->hmsAVMods[0][2] - glob.coopCompanionExchangeableData->hmsAVMods[0][2]);
			logger::debug("[GLOB] CopyOverAVs: Resetting player 1's {} base/normal AV to {}, {}, {}'s base/normal AV to {}, {}.",
				RE::ActorValue::kMagicka, glob.p1ExchangeableData->hmsBaseAVs[1], glob.p1ExchangeableData->hmsAVs[1],
				a_coopActor->GetName(), newExportedBaseAVs[1], newExportedAVs[1]);
			p1->SetActorValue(RE::ActorValue::kMagicka, glob.p1ExchangeableData->hmsAVs[1]);
			p1->SetBaseActorValue(RE::ActorValue::kMagicka, glob.p1ExchangeableData->hmsBaseAVs[1]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, glob.p1ExchangeableData->hmsAVMods[1][0] - glob.coopCompanionExchangeableData->hmsAVMods[1][0]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka, glob.p1ExchangeableData->hmsAVMods[1][1] - glob.coopCompanionExchangeableData->hmsAVMods[1][1]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka, glob.p1ExchangeableData->hmsAVMods[1][2] - glob.coopCompanionExchangeableData->hmsAVMods[1][2]);
			logger::debug("[GLOB] CopyOverAVs: Resetting player 1's {} base/normal AV to {}, {}, {}'s base/normal AV to {}, {}.",
				RE::ActorValue::kStamina, glob.p1ExchangeableData->hmsBaseAVs[2], glob.p1ExchangeableData->hmsAVs[2],
				a_coopActor->GetName(), newExportedBaseAVs[2], newExportedAVs[2]);
			p1->SetActorValue(RE::ActorValue::kStamina, glob.p1ExchangeableData->hmsAVs[2]);
			p1->SetBaseActorValue(RE::ActorValue::kStamina, glob.p1ExchangeableData->hmsBaseAVs[2]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, glob.p1ExchangeableData->hmsAVMods[2][0] - glob.coopCompanionExchangeableData->hmsAVMods[2][0]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina, glob.p1ExchangeableData->hmsAVMods[2][1] - glob.coopCompanionExchangeableData->hmsAVMods[2][1]);
			p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina, glob.p1ExchangeableData->hmsAVMods[2][2] - glob.coopCompanionExchangeableData->hmsAVMods[2][2]);
			
			// Skills next.
			auto currentAV = RE::ActorValue::kNone;
			for (auto i = 0; i < Skill::kTotal; ++i)
			{
				currentAV = SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
				if (!SHARED_SKILL_AVS_SET.contains(currentAV))
				{
					if (a_shouldCopyChanges)
					{
						float newAV = p1->GetBaseActorValue(currentAV);
						if (newAV != glob.coopCompanionExchangeableData->skillAVs[i])
						{
							a_coopActor->SetBaseActorValue(currentAV, newAV);
							if (glob.serializablePlayerData.contains(a_coopActor->formID))
							{
								auto& data = glob.serializablePlayerData[a_coopActor->formID];
								logger::debug("[GLOB] CopyOverAVs: {}'s {} skill inc went from {} to {}.",
									a_coopActor->GetName(), std::format("{}", currentAV),
									data->skillLevelIncreasesList[i],
									data->skillLevelIncreasesList[i] + newAV - glob.coopCompanionExchangeableData->skillAVs[i]);
								data->skillLevelIncreasesList[i] += newAV - glob.coopCompanionExchangeableData->skillAVs[i];
							}
						}
					}

					logger::debug("[GLOB] CopyOverAVs: Resetting player 1's {} AV to {}, was {}. Setting temp modifiers back to ({}, {}, {}), were copied from {} as ({}, {}, {}), diffs: ({}, {}, {}).",
						std::format("{}", currentAV),
						glob.p1ExchangeableData->skillAVs[i],
						glob.coopCompanionExchangeableData->skillAVs[i],
						glob.p1ExchangeableData->skillAVMods[0][i],
						glob.p1ExchangeableData->skillAVMods[1][i],
						glob.p1ExchangeableData->skillAVMods[2][i],
						a_coopActor->GetName(),
						glob.coopCompanionExchangeableData->skillAVMods[0][i],
						glob.coopCompanionExchangeableData->skillAVMods[1][i],
						glob.coopCompanionExchangeableData->skillAVMods[2][i],
						glob.p1ExchangeableData->skillAVMods[0][i] - glob.coopCompanionExchangeableData->skillAVMods[0][i],
						glob.p1ExchangeableData->skillAVMods[1][i] - glob.coopCompanionExchangeableData->skillAVMods[1][i],
						glob.p1ExchangeableData->skillAVMods[2][i] - glob.coopCompanionExchangeableData->skillAVMods[2][i]);
					p1->SetBaseActorValue(currentAV, glob.p1ExchangeableData->skillAVs[i]);
					p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, currentAV, glob.p1ExchangeableData->skillAVMods[0][i] - glob.coopCompanionExchangeableData->skillAVMods[0][i]);
					p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, currentAV, glob.p1ExchangeableData->skillAVMods[1][i] - glob.coopCompanionExchangeableData->skillAVMods[1][i]);
					p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, currentAV, glob.p1ExchangeableData->skillAVMods[2][i] - glob.coopCompanionExchangeableData->skillAVMods[2][i]);
				}
			}
		}
	}

	void GlobalCoopData::CopyOverInventories(RE::Actor* a_coopActor, const bool& a_shouldImport) 
	{
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor)
		{
			logger::error("[GLOB] ERR: CopyOverInventories: P1 is invalid: {}, requesting player is invalid: {}.",
				(bool)!p1, (bool)!a_coopActor);
			return;
		}

		int8_t pIndex = GetCoopPlayerIndex(a_coopActor->GetHandle());
		const auto& p = glob.coopPlayers[pIndex];
		auto transferP1InventoryToRefr = 
		[p1](RE::TESObjectREFR* a_toRefr) {
			auto inventory = p1->GetInventory();
			for (auto& [boundObj, entry] : inventory)
			{
				// Do not transfer equipped or party-wide items.
				// Don't want to allow other players to sell P1's equipped items while bartering.
				// Also do not remove P1's gold, since P1's gold total is the total of all players' collected gold.
				if ((boundObj && entry.first > 0 && !Util::IsPartyWideItem(boundObj)) && (!entry.second || !entry.second->IsWorn()))
				{
					logger::debug("[GLOB] CopyOverInventories: Moving x{} {} from P1 to {}.", entry.first, boundObj->GetName(), a_toRefr->GetName());
					p1->RemoveItem(boundObj, entry.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					// IMPORTANT:
					// Adding the object does not flag the player's inventory as changed and does not trigger the
					// game's equip calculations, which normally clear out the player's currently equipped gear.
					a_toRefr->AddObjectToContainer(boundObj, nullptr, entry.first, a_toRefr);
				}
			}

			if (auto invChanges = p1->GetInventoryChanges(); invChanges && invChanges->entryList) 
			{
				auto invCounts = p1->GetInventoryCounts();
				for (auto invChangesEntry : *invChanges->entryList) 
				{
					if (invChangesEntry && invChangesEntry->object) 
					{
						auto obj = invChangesEntry->object;
						// Not equipped by P1 and not a party-wide item.
						if (!Util::IsPartyWideItem(obj) && !invChangesEntry->IsWorn()) 
						{
							auto count = invCounts.contains(obj) ? invCounts.at(obj) : 0;
							if (count > 0) 
							{
								logger::debug("[GLOB] CopyOverInventories: Moving x{} {} from P1 to {}.", count, obj->GetName(), a_toRefr->GetName());
								p1->RemoveItem(obj, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
								// IMPORTANT:
								// Adding the object does not flag the player's inventory as changed and does not trigger the
								// game's equip calculations, which normally clear out the player's currently equipped gear.
								a_toRefr->AddObjectToContainer(obj, nullptr, count, a_toRefr);
							}
						}
					}
				}
			}
		};

		auto transferRefrInventoryToP1 =
		[p1](RE::TESObjectREFR* a_fromRefr) {
			auto inventory = a_fromRefr->GetInventory();
			for (auto& [boundObj, entry] : inventory)
			{
				// Do not transfer the co-op companion player's equipped items to P1.
				if ((boundObj && entry.first > 0) && (!entry.second || !entry.second->IsWorn()))
				{
					a_fromRefr->RemoveItem(boundObj, entry.first, RE::ITEM_REMOVE_REASON::kStoreInTeammate, nullptr, p1);
				}
			}

			if (auto invChanges = a_fromRefr->GetInventoryChanges(); invChanges && invChanges->entryList)
			{
				auto invCounts = a_fromRefr->GetInventoryCounts();
				for (auto invChangesEntry : *invChanges->entryList)
				{
					if (invChangesEntry && invChangesEntry->object)
					{
						auto obj = invChangesEntry->object;
						// Do not transfer the co-op companion player's equipped items to P1.
						if (!invChangesEntry->IsWorn())
						{
							auto count = invCounts.contains(obj) ? invCounts.at(obj) : 0;
							if (count > 0)
							{
								a_fromRefr->RemoveItem(obj, count, RE::ITEM_REMOVE_REASON::kStoreInTeammate, nullptr, p1);
							}
						}
					}
				}
			}
		};

		const auto& coopP1 = glob.coopPlayers[glob.player1CID];
		if (a_shouldImport)
		{
			auto p1StorageChestRefr = glob.coopInventoryChests[0];
			if (!p1StorageChestRefr || !p1StorageChestRefr.get()) 
			{
				logger::error("[GLOB] ERR: CopyOverInventories: Could not get P1's extra storage chest on import.");
				return;
			}

			// Use chest inventory as temporary storage for P1's inventory items. Clear it out first.
			if (auto chestInvChanges = p1StorageChestRefr->GetInventoryChanges(); chestInvChanges)
			{
				chestInvChanges->RemoveAllItems(p1StorageChestRefr.get(), nullptr, false, false, false);
			}

			// From P1 to storage chest.
			logger::debug("[GLOB] CopyOverInventories: IMPORT: Move all P1 items to storage chest.");
			transferP1InventoryToRefr(p1StorageChestRefr.get());
			// From co-op player to P1.
			logger::debug("[GLOB] CopyOverInventories: IMPORT: Move all co-op companion items to P1.");
			transferRefrInventoryToP1(a_coopActor);

			p1->OnArmorActorValueChanged();
			auto invChanges = p1->GetInventoryChanges();
			if (invChanges)
			{
				invChanges->armorWeight = invChanges->totalWeight;
				invChanges->totalWeight = -1.0f;
				p1->equippedWeight = -1.0f;
			}

			// Update list after swapping inventories.
			if (auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::BarterMenu::MENU_NAME))
			{
				ui->GetMenu<RE::BarterMenu>()->itemList->Update();
			}
		}
		else
		{
			auto p1StorageChestRefr = glob.coopInventoryChests[0];
			if (!p1StorageChestRefr || !p1StorageChestRefr.get())
			{
				logger::error("[GLOB] ERR: CopyOverInventories: Could not get P1's extra storage chest on export.");
				return;
			}

			// Transfer P1's current items back to the co-op player.
			logger::debug("[GLOB] CopyOverInventories: EXPORT: Move all P1 items back to co-op companion.");
			transferP1InventoryToRefr(a_coopActor);
			// Transfer P1's original items back to P1 via the storage chest.
			logger::debug("[GLOB] CopyOverInventories: EXPORT: Move all P1 items from storage chest back to P1.");
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
		// NOTE: Unlocked perks lists do NOT get modified and are simply imported and restored.
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor)
		{
			logger::error("[GLOB] ERR: CopyOverPerkLists: P1 is invalid: {}, requesting player is invalid: {}.",
				(bool)!p1, (bool)!a_coopActor);
			return;
		}

		// Remove all non-shared perks from player 1.
		std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> removeNonSharedP1PerksVisitor =
		[p1](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
			if (a_node)
			{
				auto perk = a_node->perk;
				// Must remove perks from highest rank to lowest.
				std::stack<RE::BGSPerk*> perkStack;
				while (perk)
				{
					perkStack.push(perk);
					perk = perk->nextPerk;
				}

				while (!perkStack.empty())
				{
					// Don't remove shared skill perks.
					bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
					if (auto perkToRemove = perkStack.top(); perkToRemove && !shared)
					{
						Util::Player1RemovePerk(perkToRemove);
					}

					perkStack.pop();
				}
			}
		};

		// Add all perks from the given list to player 1.
		auto addSavedPerksToP1 = 
		[p1, &a_shouldImport](const std::vector<RE::BGSPerk*>& a_perksList) {
			for (auto i = 0; i < a_perksList.size(); ++i)
			{
				auto perkToAdd = a_perksList[i];
				logger::debug("[GLOB] CopyOverPerkLists: {}: Adding perk {} (0x{:X}) to player 1, {} (0x{:X}). Has perk already: {}, {}",
					a_shouldImport ? "Import" : "Export",
					perkToAdd->GetName(), perkToAdd->formID, p1->GetName(), p1->formID, p1->HasPerk(perkToAdd), Util::Player1PerkListHasPerk(perkToAdd));
				bool succ = Util::Player1AddPerk(perkToAdd);
				logger::debug("[GLOB] CopyOverPerkLists. Added {} to p1's perks list: {}. New glob list perk count: {}",
					perkToAdd->GetName(), succ ? "SUCC" : "FAIL", p1->perks.size());
			}
		};

		auto& p1SerializedData = glob.serializablePlayerData.at(p1->formID);
		auto& coopPlayerSerializedData = glob.serializablePlayerData.at(a_coopActor->formID);
		if (a_shouldImport)
		{
			Util::TraverseAllPerks(nullptr, removeNonSharedP1PerksVisitor);
			// Add co-op player's saved perks to player 1.
			addSavedPerksToP1(coopPlayerSerializedData->GetUnlockedPerksList());
		}
		else
		{
			Util::TraverseAllPerks(nullptr, removeNonSharedP1PerksVisitor);
			// Restore all of player 1's perks.
			addSavedPerksToP1(p1SerializedData->GetUnlockedPerksList());
		}
	}

	void GlobalCoopData::CopyOverPerkTrees(RE::Actor* a_coopActor, const bool& a_shouldImport)
	{
		// NOTE: Unlocked perks lists DO get modified on import and restore.
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1 || !a_coopActor)
		{
			logger::error("[GLOB] ERR: CopyOverPerkTrees: P1 is invalid: {}, requesting player is invalid: {}.",
				(bool)!p1, (bool)!a_coopActor);
			return;
		}

		auto& p1SerializedData = glob.serializablePlayerData.at(p1->formID);
		auto& coopPlayerSerializedData = glob.serializablePlayerData.at(a_coopActor->formID);

		auto adjustSkillXP =
		[p1, a_coopActor, &a_shouldImport, &glob, &p1SerializedData, &coopPlayerSerializedData]() {
			if (a_shouldImport)
			{
				// Copy over skill XP, ignore shared skills.
				auto currentAV = RE::ActorValue::kNone;
				for (auto i = 0; i < Skill::kTotal; ++i)
				{
					currentAV = glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
					if (!glob.SHARED_SKILL_AVS_SET.contains(currentAV))
					{
						// REMOVE
						const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
						logger::debug("[GLOB] CopyOverPerkTrees Import: Lock: 0x{:X}.", hash);
						{
							std::unique_lock<std::mutex> lock(glob.p1SkillXPMutex);
							logger::debug("[GLOB] CopyOverPerkTrees Import: Lock obtained: 0x{:X}.", hash);
							p1SerializedData->skillXPList[i] = p1->skills->data->skills[i].xp;
							p1->skills->data->skills[i].xp = coopPlayerSerializedData->skillXPList[i];

							logger::debug("[GLOB] CopyOverPerkTrees Import: saved skill {}'s XP ({}) for P1. {}'s XP ({}) was imported.",
								glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i)),
								p1SerializedData->skillXPList[i],
								a_coopActor->GetName(),
								coopPlayerSerializedData->skillXPList[i]);
						}
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
					if (!glob.SHARED_SKILL_AVS_SET.contains(currentAV))
					{
						// REMOVE
						const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
						logger::debug("[GLOB] CopyOverPerkTrees Export: Lock: 0x{:X}.", hash);
						{
							std::unique_lock<std::mutex> lock(glob.p1SkillXPMutex);
							logger::debug("[GLOB] CopyOverPerkTrees Export: Lock obtained: 0x{:X}.", hash);
							p1->skills->data->skills[i].xp = p1SerializedData->skillXPList[i];
							logger::debug("[GLOB] CopyOverPerkTrees Export: P1's skill {}'s XP ({}) was restored.",
								glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i)),
								p1SerializedData->skillXPList[i]);
						}
					}
				}
			}
		};

		// Save both players' perks to serializable data list.
		std::function<void(RE::BGSSkillPerkTreeNode * a_node, RE::Actor * a_actor)> savePlayerPerksVisitor = 
		[p1, &glob, &p1SerializedData, &coopPlayerSerializedData](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
			if (a_node)
			{
				auto perk = a_node->perk;
				uint32_t perkIndex = 0;
				while (perk)
				{
					bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
					bool nativeFuncHasPerk = a_actor->HasPerk(perk);
					if (a_actor == RE::PlayerCharacter::GetSingleton())
					{
						bool singletonListHasPerk = Util::Player1PerkListHasPerk(perk);
						// Sync glob perk list with native perk check result.
						if (nativeFuncHasPerk || singletonListHasPerk)
						{
							logger::debug("[GLOB] CopyOverPerkTrees: Import: {} has perk {} (0x{:X}) (native func: {}, glob list: {})",
								p1->GetName(), perk->GetName(), perk->formID, nativeFuncHasPerk, singletonListHasPerk);

							if (nativeFuncHasPerk != singletonListHasPerk)
							{
								logger::debug("[GLOB] ERR: CopyOverPerkTrees {} has perk check inconsistency. Adding {} (0x{:X}).",
									a_actor->GetName(), perk->GetName(), perk->formID);
								Util::Player1AddPerk(perk);
							}

							bool succ = p1SerializedData->InsertUnlockedPerk(perk);
							// Increment on-import shared perks count if player 1 has the perk.
							// No duplicates!
							if (shared && succ)
							{
								++glob.importUnlockedSharedPerksCount;
							}
						}
					}
					else if (nativeFuncHasPerk)
					{
						logger::debug("[GLOB] CopyOverPerkTrees: Import: {} has perk #{} {} (0x{:X}), is shared: {}",
							a_actor->GetName(), perkIndex, perk->GetName(), perk->formID, shared);
						coopPlayerSerializedData->InsertUnlockedPerk(perk);
					}

					perk = perk->nextPerk;
					++perkIndex;
				}
			}
		};

		// Update co-op player's unlocked perks list on exiting the level up menu and add all selected perks to the co-op player.
		std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> saveAndAddSelectedPerksVisitor = 
		[p1, &glob, &coopPlayerSerializedData](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
			if (a_node)
			{
				auto perk = a_node->perk;
				uint32_t perkIndex = 0;
				while (perk)
				{
					// Selected perks do not seem to get added to the player 1 glob list while the Stats Menu is open.
					// Have to use native func check here too.
					bool nativeFuncP1HasPerk = p1->HasPerk(perk);
					bool singletonListHasPerk = Util::Player1PerkListHasPerk(perk);
					if (nativeFuncP1HasPerk || singletonListHasPerk)
					{
						bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
						bool succ = Util::ChangePerk(a_actor, perk, true);
						logger::debug("[GLOB] CopyOverPerkTrees: Export: {} has perk #{} {} (0x{:X}), is shared: {}. Adding to co-op player: {}. Native func has perk: {}, glob list has perk: {}",
							a_actor->GetName(), perkIndex, perk->GetName(), perk->formID, shared, succ ? "SUCC" : "FAIL",
							nativeFuncP1HasPerk, singletonListHasPerk);
						succ = coopPlayerSerializedData->InsertUnlockedPerk(perk);

						// Increment on-export shared perks count if the perk is unlocked on menu exit.
						// No duplicates!
						if (shared && succ)
						{
							++glob.exportUnlockedSharedPerksCount;
						}
					}

					perk = perk->nextPerk;
					++perkIndex;
				}
			}
		};

		// Remove all non-shared perks from player 1.
		std::function<void(RE::BGSSkillPerkTreeNode * a_node, RE::Actor * a_actor)> removeNonSharedP1PerksVisitor =
		[p1](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
			if (a_node)
			{
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
					// Don't remove shared skill perks from P1, since they should carry over after the co-op companion player exits the Stats Menu.
					bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
					if (auto perkToRemove = perkStack.top(); perkToRemove && !shared)
					{
						Util::Player1RemovePerk(perkToRemove);
					}

					perkStack.pop();
				}
			}
		};


		// Add all perks from the given list to player 1.
		auto addSavedPerksToP1 = 
		[p1, &a_shouldImport](const std::vector<RE::BGSPerk*>& a_perksList) {
			for (auto i = 0; i < a_perksList.size(); ++i)
			{
				auto perkToAdd = a_perksList[i];
				logger::debug("[GLOB] CopyOverPerkTrees: {}: Adding perk {} (0x{:X}) to player 1, {} (0x{:X}). Has perk already: {}, {}",
					a_shouldImport ? "Import" : "Export",
					perkToAdd->GetName(), perkToAdd->formID, p1->GetName(), p1->formID, p1->HasPerk(perkToAdd), Util::Player1PerkListHasPerk(perkToAdd));
				bool succ = Util::Player1AddPerk(perkToAdd);
				logger::debug("[GLOB] CopyOverPerkTrees: Added {} to p1's perks list: {}. New glob list perk count: {}",
					perkToAdd->GetName(), succ ? "SUCC" : "FAIL", p1->perks.size());
			}
		};

		// REMOVE
		std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> checkPerkTree = 
		[p1, &a_shouldImport](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
			if (a_node)
			{
				auto perk = a_node->perk;
				uint32_t perkIndex = 0;
				while (perk)
				{
					// Selected perks do not get added to the player 1 glob list while the level up menu is open?
					// Have to use native func check here as a result.
					if (p1->HasPerk(perk) || Util::Player1PerkListHasPerk(perk)) 
					{
						logger::debug("[GLOB] CopyOverPerkTrees: {}: CHECK: AFTER ADJUSTMENTS {} has perk #{} {} (0x{:X})",
							a_shouldImport ? "Import" : "Export", a_actor->GetName(), perkIndex, perk->GetName(), perk->formID);
					}

					perk = perk->nextPerk;
					++perkIndex;
				}
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
			// Updated in "save perks" visitor below.
			glob.importUnlockedSharedPerksCount = 0;
			Util::TraverseAllPerks(p1, savePlayerPerksVisitor);
			Util::TraverseAllPerks(a_coopActor, savePlayerPerksVisitor);
			logger::debug("[GLOB] CopyOverPerkTrees: Import: {} has {} unlocked perks, {} has {} unlocked perks.",
				p1->GetName(), p1SerializedData->GetUnlockedPerksList().size(),
				a_coopActor->GetName(), coopPlayerSerializedData->GetUnlockedPerksList().size());
			Util::TraverseAllPerks(nullptr, removeNonSharedP1PerksVisitor);
			// Add co-op player's saved perks to player 1.
			addSavedPerksToP1(coopPlayerSerializedData->GetUnlockedPerksList());

			// REMOVE when done debugging.
			Util::TraverseAllPerks(a_coopActor, checkPerkTree);
		}
		else
		{
			// REMOVE when done debugging.
			Util::TraverseAllPerks(a_coopActor, checkPerkTree);

			// Adjust skill XP first.
			adjustSkillXP();
			// Clear out old unlocked perks list before updating.
			coopPlayerSerializedData->ClearUnlockedPerks();
			// Reset on-export unlocked shared perks count.
			// Updated in "save and add perks" visitor below.
			glob.exportUnlockedSharedPerksCount = 0;
			Util::TraverseAllPerks(a_coopActor, saveAndAddSelectedPerksVisitor);
			Util::TraverseAllPerks(nullptr, removeNonSharedP1PerksVisitor);
			// Add back all player 1's original perks cached when entering.
			addSavedPerksToP1(p1SerializedData->GetUnlockedPerksList());
			logger::debug("[GLOB] CopyOverPerkTrees: Export: FINAL COUNT: {} has {} perks", a_coopActor->GetName(), coopPlayerSerializedData->GetUnlockedPerksList().size());
			logger::debug("[GLOB] CopyOverPerkTrees: Export: FINAL COUNT: {} has {} perks.", p1->GetName(), p1SerializedData->GetUnlockedPerksList().size());

			// Update unlocked shared perks count for this player.
			coopPlayerSerializedData->sharedPerksUnlocked += max(0, glob.exportUnlockedSharedPerksCount - glob.importUnlockedSharedPerksCount);
			logger::debug("[GLOB] CopyOverPerkTrees: Export: {} has personally unlocked {} shared perks.",
				a_coopActor->GetName(), coopPlayerSerializedData->sharedPerksUnlocked);
		}
	}

	void GlobalCoopData::CopyOverCoopPlayerData(const bool a_shouldImport, const RE::BSFixedString a_menuName, RE::ActorHandle a_requestingPlayerHandle, RE::TESForm* a_assocForm)
	{
		logger::debug("[GLOB] CopyOverPlayerDataTask: import: {}, menu name: {}, requesting player: {}, associated form: {}",
			a_shouldImport, a_menuName,
			Util::HandleIsValid(a_requestingPlayerHandle) ? a_requestingPlayerHandle.get()->GetName() : "NONE",
			a_assocForm ? a_assocForm->GetName() : "NONE");
		auto info = std::make_unique<CopyPlayerDataRequestInfo>(a_shouldImport, a_menuName, a_requestingPlayerHandle, a_assocForm);
		// Copy data here.
		CopyPlayerData(info);
	}

	void GlobalCoopData::PromptForPlayer1CIDTask()
	{
		// Debug option: assign linked controller ID for player 1 via a prompt to press a certain button.
		// Workaround until finding direct way of accessing player 1's controller's XInput index. 
		auto& glob = GetSingleton();
		bool shouldSetP1CID = false;
		bool openingMessagePrompt = false;
		std::array<bool, ALYSLC_MAX_PLAYER_COUNT> pauseBindPressed = { false, false, false, false };
		auto ui = RE::UI::GetSingleton();

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

		if (activeControllers > 1) 
		{
			while (!shouldSetP1CID && ui)
			{
				if (!openingMessagePrompt && !ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME))
				{
					openingMessagePrompt = true;
					Util::AddSyncedTask([]() { RE::DebugMessageBox("[ALYSLC] Player 1: Please press the 'Pause' or 'Journal Menu' button to set your controller ID for co-op."); });			
				}
				else if (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME))
				{
					auto userEvents = RE::UserEvents::GetSingleton();
					auto controlMap = RE::ControlMap::GetSingleton();
					uint16_t pauseBind = XINPUT_GAMEPAD_START;
					if (userEvents && controlMap) 
					{
						pauseBind = controlMap->GetMappedKey(userEvents->journal, RE::INPUT_DEVICE::kGamepad);
					}

					openingMessagePrompt = false;
					cid = 0;
					while (cid < ALYSLC_MAX_PLAYER_COUNT)
					{
						if (auto errorNum = XInputGetState(cid, &inputState); errorNum == ERROR_SUCCESS)
						{
							// Should set once pause bind is released.
							if ((inputState.Gamepad.wButtons & pauseBind) == pauseBind)
							{
								// Is pressed but not released yet.
								pauseBindPressed[cid] = true;
							}
							else if ((inputState.Gamepad.wButtons & pauseBind) == 0 && pauseBindPressed[cid])
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
							Util::AddSyncedTask([msgQ]() { msgQ->AddMessage(RE::MessageBoxMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kForceHide, nullptr); });
						}

						float waitSecs = 0.0f;
						while (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) && waitSecs < 2.0f)
						{
							std::this_thread::sleep_for(0.1s);
							waitSecs += 0.1f;
						}

						std::this_thread::sleep_for(0.1s);
						// Show result message box.
						Util::AddSyncedTask([&cid]() { RE::DebugMessageBox(fmt::format("[ALYSLC] Player 1 has been assigned controller ID {}.", cid).c_str()); });
						glob.player1CID = cid;
					}
				}
			}
		}
		else
		{
			Util::AddSyncedTask([]() { RE::DebugMessageBox("[ALYSLC] Please connect another controller before starting co-op."); });
		}
	}

	void GlobalCoopData::ResetPlayer1AndCameraTask()
	{
		auto& glob = GetSingleton();
		const auto& coopP1 = glob.coopPlayers[glob.player1CID];
		if (coopP1->isActive)
		{
			// Stop P1 managers, cam manager, and menu input manager.
			coopP1->RequestStateChange(ManagerState::kAwaitingRefresh);
			SteadyClock::time_point waitStartTP = SteadyClock::now();
			float secsWaited = 0.0f;
			// 1 second failsafe.
			while (coopP1->currentState != ManagerState::kAwaitingRefresh && secsWaited < 1.0f)
			{
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
				std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f)));
			}

			glob.cam->SetWaitForToggle(true);
			glob.cam->ToggleCoopCamera(false);
			waitStartTP = SteadyClock::now();
			secsWaited = 0.0f;
			while (glob.cam->currentState != ManagerState::kPaused && secsWaited < 1.0f)
			{
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
				std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f)));
			}


			glob.mim->ToggleCoopPlayerMenuMode(-1);
			waitStartTP = SteadyClock::now();
			secsWaited = 0.0f;
			while (glob.cam->currentState != ManagerState::kPaused && secsWaited < 1.0f)
			{
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
				std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f)));
			}
		}
	}

	void GlobalCoopData::RestartCoopCameraTask()
	{
		auto& glob = GetSingleton();
		glob.cam->ToggleCoopCamera(false);
		SteadyClock::time_point waitStartTP = SteadyClock::now();
		float secsWaited = 0.0f;
		// 1 second failsafe.
		while (glob.cam->currentState != ManagerState::kPaused && secsWaited < 1.0f)
		{
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
			std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f)));
		}

		glob.cam->ToggleCoopCamera(true);
	}

	void GlobalCoopData::AdjustPlayerTorsoRotation(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// KNOWN ISSUES: 
		// As of now, I haven't figure out how to directly set the world rotation matrices for the player's spinal nodes.
		// Has led to indirectly adjusting rotation via the local rotation matrices instead.
		// Upper spinal nodes begin to twist when pitch/roll nears 90 degrees. 
		// Probably related to headtracking and the above local rotation workaround.
		auto& glob = GetSingleton();
		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings)
		{
			return;
		}

		auto loadedData = a_p->coopActor->loadedData; 
		if (!loadedData)
		{
			return;
		}

		auto data3D = loadedData->data3D; 
		if (!data3D || !data3D->parent)
		{
			return;
		}

		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);
		auto weaponNode = a_p->coopActor->loadedData->data3D->GetObjectByName(strings->weapon);
		auto rootNode = a_p->coopActor->loadedData->data3D->GetObjectByName(strings->npcRoot);
		bool isAimingWithBow = a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
								a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDraw ||
								a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn ||
								a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing ||
								a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleased ||
								a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowFollowThrough;
		bool isMounted = a_p->coopActor->IsOnMount();

		// Interpolation factor for pitch.
		// Roll factor is pitchFactor - 1.
		// Yaw handled separately.
		float pitchFactor = 1.0f;
		// One-over ratio of total aim pitch by which to rotate each node.
		const float div = 3.0f;
		const uint8_t numAdjustableNodes = a_p->isTransformed ? TORSO_ADJUSTMENT_NPC_NODES.size() - 1 : TORSO_ADJUSTMENT_NPC_NODES.size();
		for (uint8_t i = 0; i < numAdjustableNodes; ++i) 
		{
			const auto& nodeName = TORSO_ADJUSTMENT_NPC_NODES[i];
			if (auto node3D = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName)); node3D && node3D.get())
			{
				const auto& worldRot = node3D->world.rotate;
				auto worldXAxis = RE::NiPoint3(worldRot * right);
				auto worldYAxis = RE::NiPoint3(worldRot * forward);
				auto worldZAxis = RE::NiPoint3(worldRot * up);

				// REMOVE when done debugging.
				/*glm::vec3 start{ node3D->world.translate.x, node3D->world.translate.y, node3D->world.translate.z };
				glm::vec3 endX{ start + glm::vec3(worldXAxis.x, worldXAxis.y, worldXAxis.z) * 15.0f };
				glm::vec3 endY{ start + glm::vec3(worldYAxis.x, worldYAxis.y, worldYAxis.z) * 15.0f };
				glm::vec3 endZ{ start + glm::vec3(worldZAxis.x, worldZAxis.y, worldZAxis.z) * 15.0f };
				DebugAPI::QueueArrow3D(start, endX, 0xFF0000FF, 5.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endY, 0x00FF00FF, 5.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endZ, 0x0000FFFF, 5.0f, 2.0f);*/

				const auto nodeNameHash = Hash(node3D->name);
				bool hasBeenSet = a_p->mm->nrm->lastSetTorsoLocalRotations.contains(nodeNameHash);
				auto newLocalRot = !hasBeenSet ? node3D->local.rotate : a_p->mm->nrm->lastSetTorsoLocalRotations.at(nodeNameHash);
				float oldPitch = a_p->mm->nrm->lastSetTorsoRotationInputs.contains(nodeNameHash) ? 
									a_p->mm->nrm->lastSetTorsoRotationInputs[nodeNameHash][0] : 
									a_p->mm->aimPitch / div;
				float oldYaw = a_p->mm->nrm->lastSetTorsoRotationInputs.contains(nodeNameHash) ? 
								a_p->mm->nrm->lastSetTorsoRotationInputs[nodeNameHash][1] : 
								0.0f;
				float oldRoll = a_p->mm->nrm->lastSetTorsoRotationInputs.contains(nodeNameHash) ? 
								a_p->mm->nrm->lastSetTorsoRotationInputs[nodeNameHash][2] : 
								0.0f;

				// Pitching nodes up/down when attacking with a ranged weapon also uses the roll angle to varying degrees (no pun intended).
				// If we only modify the node's pitch angle, the player will pitch forward or backward at up to 90 degrees, 
				// but not necessarily in the direction in which they are attacking, 
				// since facing angle and attacking angle do not match, depending on the attack animation.
				float pitch = 0.0f;
				float roll = 0.0f;				
				// Only adjusted when mounted.
				auto localForwardXY = RE::NiPoint3(node3D->local.rotate * forward);
				localForwardXY.z = 0.0f;
				localForwardXY.Unitize();
				float yaw = Util::NormalizeAngToPi(Util::GetYawBetweenPositions(RE::NiPoint3(), localForwardXY));
				float playerFacingAng = a_p->coopActor->data.angle.z;

				// Pitch factor is obtained from spine base node (first one).
				// Produces the less twisting when modifying subsequent child nodes.
				if (Hash(TORSO_ADJUSTMENT_NPC_NODES[0]) == Hash(nodeName))
				{
					float yawDiff = 0.0f;
					float nodeHeading = Util::DirectionToGameAngYaw(worldYAxis);
					if (bool weaponDrawn = a_p->coopActor->IsWeaponDrawn(); weaponDrawn && weaponNode && isAimingWithBow) 
					{
						auto playerFacingDir = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(playerFacingAng));
						// Difference between facing and attacking angles.
						yawDiff = Util::NormalizeAngToPi(Util::DirectionToGameAngYaw(playerFacingDir) - nodeHeading);
						// Aim straight up with the arrow node when player heading and arrow node directions are oriented at > 90 degrees.
						// Prevents stuttering when adjusting pitch near this endpoint.
						// Stuttering reason: player heading direction flips 180 degrees when the aim pitch goes above PI / 2 in magnitude.
						// Since we rotate the player so that their heading direction faces their target,
						// they will turn 180 degrees to face the target.
						if (playerFacingDir.Dot(weaponNode->world.rotate * forward) < 0.0f) 
						{
							// Identity matrix.
							weaponNode->world.rotate = RE::NiMatrix3();
						}
					}
					else if (a_p->coopActor->IsAttacking())
					{
						if (a_p->mm->shouldFaceTarget || Util::HandleIsValid(a_p->tm->crosshairRefrHandle)) 
						{
							yawDiff = Util::NormalizeAngToPi(Util::GetYawBetweenPositions(a_p->mm->playerAttackSourcePos, a_p->tm->crosshairWorldPos) - nodeHeading);
						}
						else
						{
							yawDiff = Util::NormalizeAngToPi(playerFacingAng - nodeHeading);
						}
					}
					else
					{
						yawDiff = 0.0f;
					}

					// The smaller the yaw difference between attacking angle and player facing angle,
					// the more we rotate along the pitch axis, as opposed to the roll axis.
					pitchFactor = 1.0f - (abs(std::clamp(yawDiff, -PI / 2.0f, PI / 2.0f)) / (PI / 2.0f));
				}

				pitch = Util::InterpolateSmootherStep(oldPitch, pitchFactor * a_p->mm->aimPitch / div, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
				roll = Util::InterpolateSmootherStep(oldRoll, (pitchFactor - 1.0f) * a_p->mm->aimPitch / div, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
				// Sooo, this took way too long.
				// Definitely not polished and has issues when moving away from a target while aiming or when pitching
				// too high or low.
				// Also hourglass torso mode when rotated > 90 degrees in either direction.
				// Haven't figured out the math yet because there is no proper heading angle to base calculations off of while mounted.
				// The player's heading angle is the same as the horse's while mounted and ranged weapon aim yaw is at a variable angular
				// offset from the player's facing direction that I can't seem to pinpoint, so I've estimated this angular offset for bows and crossbows.
				// Good enough for now.
				if (isMounted) 
				{
					float newYaw = oldYaw;
					float angDiffScalingInterpFactor = 1.0f;
					if (RE::ActorPtr mount; a_p->coopActor->GetMount(mount) && mount && mount.get()) 
					{
						if (a_p->coopActor->IsWeaponDrawn() && weaponNode && rootNode && isAimingWithBow && nodeName == strings->npcSpine)
						{
							// Face the target.
							if (auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle); crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get())) 
							{
								auto playerHeadingDir = rootNode->world.rotate * forward;
								// Not the same as the game's returned actor heading angle.
								playerFacingAng = Util::DirectionToGameAngYaw(playerHeadingDir);
								// If there were a way to get the fully drawn arrow's heading angle before it's fully drawn, I would.
								float playerToTargetYaw = Util::DirectionToGameAngYaw(a_p->tm->crosshairWorldPos - weaponNode->world.translate);
								float headingToTargetDiff = Util::NormalizeAngToPi(playerToTargetYaw - playerFacingAng);

								// Scale down interpolation factor when the weapon node is close to the target or when the yaw change is large.
								angDiffScalingInterpFactor = (1.1f - abs(newYaw - oldYaw) / (2.0f * PI));
								float xyDistToTarget = Util::GetXYDistance(weaponNode->world.translate, a_p->tm->crosshairWorldPos);
								// Can't compute yaw offset from player facing angle (unknown) to aim node yaw angle.
								float distScalingDiffFactor = Util::InterpolateEaseInEaseOut(0.0f, 1.0f, min(xyDistToTarget, Settings::fTargetAttackSourceDistToSlowRotation) / (Settings::fTargetAttackSourceDistToSlowRotation + 0.01f), 5.0f);
								// Scale down yaw diff if close to target to prevent wild swings in yaw.
								// Ugly as heck, I know.
								newYaw = Util::NormalizeAngToPi(oldYaw + distScalingDiffFactor * (Util::NormalizeAngToPi(headingToTargetDiff + (a_p->em->HasBowEquipped() ? PI / 3.0f : PI / 4.0f) - oldYaw)));

								// Frequent sign changes lead to spasmodic oscillation.
								// To void this, don't interpolate on sign change and instead directly set to the new yaw.
								// Side effect: torso twists into an hourglass near 180 degrees.
								// Oh well.
								float sign = newYaw < 0.0f ? -1.0f : 1.0f;
								if ((oldYaw * sign) < 0.0f)
								{
									oldYaw = newYaw;
								}
							}
							else
							{
								// Face forward by adding estimated offset.
								newYaw = a_p->em->HasBowEquipped() ? PI / 3.0f : PI / 4.0f;
							}
						}
						else
						{
							newYaw = 0.0f;
						}
					}

					yaw = Util::InterpolateSmootherStep(oldYaw, newYaw, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor * angDiffScalingInterpFactor));
				}
				else
				{
					yaw = Util::InterpolateSmootherStep(oldYaw, yaw, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
				}

				RE::NiPoint3 locXAxis{ node3D->local.rotate * right };
				RE::NiPoint3 locYAxis{ node3D->local.rotate * forward };
				RE::NiPoint3 locZAxis{ node3D->local.rotate * up };
				Util::SetRotationMatrix3(newLocalRot, locXAxis, locYAxis, locZAxis, pitch, yaw, roll);
				// Cache our newly calculated local rotation matrix and rotation angles to set in the UpdateDownwardPass() hook.
				if (!hasBeenSet) 
				{
					a_p->mm->nrm->lastSetTorsoLocalRotations.insert({ nodeNameHash, newLocalRot });
				}
				else
				{
					a_p->mm->nrm->lastSetTorsoLocalRotations[nodeNameHash] = newLocalRot;
				}

				if (!hasBeenSet)
				{
					a_p->mm->nrm->lastSetTorsoRotationInputs.insert({ nodeNameHash, { pitch, yaw, roll } });
				}
				else
				{
					a_p->mm->nrm->lastSetTorsoRotationInputs[nodeNameHash][0] = pitch;
					a_p->mm->nrm->lastSetTorsoRotationInputs[nodeNameHash][1] = yaw;
					a_p->mm->nrm->lastSetTorsoRotationInputs[nodeNameHash][2] = roll;
				}
			}
		}
	}

	void GlobalCoopData::CheckAndPerformArmCollisions(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Setup raycasting start and end positions
		// along the lengths of the player's arms (approximation).
		// Then perform said raycasts and check for collisions.
		// Finally, if certain conditions hold, apply impulses/knockdowns/damage to hit objects.

		auto& glob = GetSingleton();
		if (!glob.coopSessionActive)
		{
			return;
		}

		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings)
		{
			return;
		}

		// Not moving the right stick, so no need to check for collisions.
		const auto& rsLinSpeed = glob.cdh->GetAnalogStickState(a_p->controllerID, false).stickLinearSpeed;
		if (rsLinSpeed == 0.0f)
		{
			return;
		}

		auto loadedData = a_p->coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		const auto& uiRGBA = Settings::vuOverlayRGBAValues[a_p->playerID];
		std::vector<RE::BSFixedString> nodeNamesToCheck{};
		bool checkLeftArm = a_p->pam->IsPerformingOneOf(InputAction::kRotateLeftShoulder, InputAction::kRotateLeftForearm, InputAction::kRotateLeftHand);
		bool checkRightArm = a_p->pam->IsPerformingOneOf(InputAction::kRotateRightShoulder, InputAction::kRotateRightForearm, InputAction::kRotateRightHand);
		if (checkLeftArm)
		{
			nodeNamesToCheck.emplace_back("NPC L Hand [LHnd]");
			nodeNamesToCheck.emplace_back(strings->npcLForearm);
			nodeNamesToCheck.emplace_back(strings->npcLUpperArm);
		}

		if (checkRightArm)
		{
			nodeNamesToCheck.emplace_back("NPC R Hand [RHnd]");
			nodeNamesToCheck.emplace_back("NPC R Forearm [RLar]");
			nodeNamesToCheck.emplace_back(strings->npcRUpperArm);
		}

		const uint32_t raycastsPerNode = 10;
		for (const auto& name : nodeNamesToCheck)
		{
			const auto nodeNameHash = Hash(name);
			if (auto node3D = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(name)); node3D && node3D.get())
			{
				// Update position and velocity first.
				const auto& playerPos = a_p->coopActor->data.location;
				const auto& nodeWorldPos = node3D->world.translate;
				RE::NiPoint3 localPos = nodeWorldPos - playerPos;
				if (!a_p->mm->nrm->lastSetArmLocalPositions.contains(nodeNameHash))
				{
					a_p->mm->nrm->lastSetArmLocalPositions.insert({ nodeNameHash, localPos });
					a_p->mm->nrm->lastRecordedArmLocalVelocities.insert({ nodeNameHash, RE::NiPoint3() });
				}
				else
				{
					// Each frame.
					RE::NiPoint3 velocity = (localPos - a_p->mm->nrm->lastSetArmLocalPositions[nodeNameHash]) / (*g_deltaTimeRealTime);
					a_p->mm->nrm->lastSetArmLocalPositions[nodeNameHash] = localPos;
					a_p->mm->nrm->lastRecordedArmLocalVelocities[nodeNameHash] = velocity;
				}

				ArmNodeType armNodeType = ArmNodeType::kShoulder;
				if (nodeNameHash == "NPC L Hand [LHnd]"_h || nodeNameHash == "NPC R Hand [RHnd]"_h)
				{
					armNodeType = ArmNodeType::kHand;
				}
				else if (nodeNameHash == Hash(strings->npcLForearm) || nodeNameHash == "NPC R Forearm [RLar]"_h)
				{
					armNodeType = ArmNodeType::kForearm;
				}

				if (auto armHkpRigidBody = Util::GethkpRigidBody(node3D.get()); armHkpRigidBody)
				{
					if (auto hkpShape = armHkpRigidBody->GetShape(); hkpShape->type == RE::hkpShapeType::kCapsule)
					{
						auto hkpCapsuleShape = static_cast<const RE::hkpCapsuleShape*>(hkpShape);
						RE::NiPoint3 vertexA;
						RE::NiPoint3 vertexB;
						RE::NiPoint3 zAxisDir;
						const float& capsuleRadius = hkpCapsuleShape->radius;
						// Special raycast offset and rotation handling for hand nodes,
						// since rigidbody capsule axes don't line up with the hands' node orientations.
						if (armNodeType == ArmNodeType::kHand)
						{
							zAxisDir = RE::NiPoint3(
								node3D->world.rotate.entry[0][2],
								node3D->world.rotate.entry[1][2],
								node3D->world.rotate.entry[2][2]);
							zAxisDir.Unitize();

							RE::NiPoint3 handCenterPos = node3D->world.translate + zAxisDir * capsuleRadius * 1.5f;
							if (zAxisDir.z > 0.0f)
							{
								vertexA = handCenterPos + zAxisDir * capsuleRadius;
								vertexB = handCenterPos - zAxisDir * capsuleRadius;
							}
							else
							{
								vertexA = handCenterPos - zAxisDir * capsuleRadius;
								vertexB = handCenterPos + zAxisDir * capsuleRadius;
							}
						}
						else
						{
							const auto& hkTransform = armHkpRigidBody->motion.motionState.transform;
							RE::NiPoint3 vertAOffset = ToNiPoint3(hkpCapsuleShape->vertexA) * HAVOK_TO_GAME;
							RE::NiPoint3 vertBOffset = ToNiPoint3(hkpCapsuleShape->vertexB) * HAVOK_TO_GAME;
							RE::NiTransform niTransform;
							niTransform.scale = 1.0f;
							niTransform.translate = ToNiPoint3(hkTransform.translation) * HAVOK_TO_GAME;
							niTransform.rotate.entry[0][0] = hkTransform.rotation.col0.quad.m128_f32[0];
							niTransform.rotate.entry[1][0] = hkTransform.rotation.col0.quad.m128_f32[1];
							niTransform.rotate.entry[2][0] = hkTransform.rotation.col0.quad.m128_f32[2];

							niTransform.rotate.entry[0][1] = hkTransform.rotation.col1.quad.m128_f32[0];
							niTransform.rotate.entry[1][1] = hkTransform.rotation.col1.quad.m128_f32[1];
							niTransform.rotate.entry[2][1] = hkTransform.rotation.col1.quad.m128_f32[2];

							niTransform.rotate.entry[0][2] = hkTransform.rotation.col2.quad.m128_f32[0];
							niTransform.rotate.entry[1][2] = hkTransform.rotation.col2.quad.m128_f32[1];
							niTransform.rotate.entry[2][2] = hkTransform.rotation.col2.quad.m128_f32[2];
							vertexA = niTransform * vertAOffset;
							vertexB = niTransform * vertBOffset;
							RE::NiPoint3 temp = (vertexA.z > vertexB.z) ? vertexA - vertexB : vertexB - vertexA;
							temp.Unitize();
							zAxisDir = { temp.x, temp.y, temp.z };
						}

						// Extend a little past the capsule edges for some overlap between nodes.
						const glm::vec4 zAxisDirVec = ToVec4(zAxisDir);
						float capsuleLength = (vertexB - vertexA).Length() + 2.5f * hkpCapsuleShape->radius * HAVOK_TO_GAME;
						glm::vec4 originPos = (vertexA.z > vertexB.z) ?
												  ToVec4(vertexB - zAxisDir * hkpCapsuleShape->radius) :
													ToVec4(vertexA - zAxisDir * hkpCapsuleShape->radius);
						RE::hkVector4 velVec{};
						glm::vec4 velDir{};
						glm::vec4 startPos{};
						glm::vec4 endPos{};
						uint32_t castNum = 1;
						bool hit = false;
						while (!hit && castNum <= raycastsPerNode)
						{
							startPos = originPos + (zAxisDirVec * (capsuleLength / raycastsPerNode)) * static_cast<float>(castNum);
							velVec = armHkpRigidBody->motion.GetPointVelocity(TohkVector4(startPos) * GAME_TO_HAVOK) * HAVOK_TO_GAME;
							velDir = ToVec4(velVec, true);
							endPos = startPos + (velDir * hkpCapsuleShape->radius * 2.0f * HAVOK_TO_GAME);
							hit = PerformArmCollisionRaycastCheck(a_p, startPos, endPos, a_p->mm->nrm->lastRecordedArmLocalVelocities[nodeNameHash], ToNiPoint3(velVec), armNodeType);

							// REMOVE when done debugging.
							//DebugAPI::QueueArrow3D(startPos, endPos, Settings::vuOverlayRGBAValues[a_p->playerID], 5.0f, 2.0f, 0.0f);

							++castNum;
						}
					}
				}
			}
		}
	}

	void GlobalCoopData::RotateArmsAboutShoulders(const std::shared_ptr<CoopPlayer>& a_p)
	{
		auto& glob = GetSingleton();
		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings)
		{
			return;
		}

		bool rotatingLeftShoulder = a_p->pam->IsPerforming(InputAction::kRotateLeftShoulder);
		bool rotatingRightShoulder = a_p->pam->IsPerforming(InputAction::kRotateRightShoulder);

		auto loadedData = a_p->coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		const auto uiRGBA = Settings::vuOverlayRGBAValues[a_p->playerID];
		auto leftShoulderNode = data3D->GetObjectByName(strings->npcLUpperArm);
		auto rightShoulderNode = data3D->GetObjectByName(strings->npcRUpperArm);
		if (leftShoulderNode && rightShoulderNode)
		{
			auto tiltShoulder =
				[&a_p, &glob, &uiRGBA](RE::NiAVObject* a_data3D, RE::NiAVObject* a_shoulderNode, bool a_rightShoulder) {
				{
					if (a_shoulderNode->collisionObject)
					{
						auto collisionObject = static_cast<RE::bhkCollisionObject*>(a_shoulderNode->collisionObject.get());
						auto rigidBody = collisionObject->GetRigidBody();
						if (rigidBody && rigidBody->referencedObject)
						{
							// Pick RS quadrant/axis based on its displacement.
							const auto& rsData = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
							auto rsLoc = AnalogStickLocation::kCenter;
							float xDisp = std::clamp(rsData.xComp * rsData.normMag, -1.0f, 1.0f);
							float yDisp = std::clamp(rsData.yComp * rsData.normMag, -1.0f, 1.0f);
							if (rsData.normMag > 0.0f) 
							{
								if (xDisp > 0.0f && yDisp > 0.0f) 
								{
									rsLoc = AnalogStickLocation::kTopRight;
								}
								else if (xDisp > 0.0f && yDisp < 0.0f)
								{
									rsLoc = AnalogStickLocation::kBottomRight;
								}
								else if (xDisp < 0.0f && yDisp < 0.0f)
								{
									rsLoc = AnalogStickLocation::kBottomLeft;
								}
								else if (xDisp < 0.0f && yDisp > 0.0f)
								{
									rsLoc = AnalogStickLocation::kTopLeft;
								}
								else if (xDisp > 0.0f && yDisp == 0.0f)
								{
									rsLoc = AnalogStickLocation::kPosXAxis;
								}
								else if (xDisp == 0.0f && yDisp < 0.0f)
								{
									rsLoc = AnalogStickLocation::kNegYAxis;
								}
								else if (xDisp < 0.0f && yDisp == 0.0f)
								{
									rsLoc = AnalogStickLocation::kNegXAxis;
								}
								else if (xDisp == 0.0f && yDisp > 0.0f)
								{
									rsLoc = AnalogStickLocation::kPosYAxis;
								}
							}

							const auto& leftShoulderMatAngleInputs = a_p->mm->nrm->leftShoulderMatAngleInputs;
							const auto& rightShoulderMatAngleInputs = a_p->mm->nrm->rightShoulderMatAngleInputs;
							// Yaw (determined by RS X displacement and heading angle), pitch (determined by RS Y displacement), roll (constant).
							std::array<float, 3> matAngleInputs{ 0.0f, 0.0f, 0.0f };
							if (rsLoc == AnalogStickLocation::kCenter) 
							{
								// Forward when centered.
								if (a_rightShoulder)
								{
									matAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kForward);
								}
								else
								{
									matAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kForward);
								}
							}
							else
							{
								// "Outer point" represents arm orientation points to use when RS is displaced from center, 
								// outer points ratio = 1, forward ratio = 0 at max displacement.
								// When centered, the forward ratio is 1 and the outer points ratio is 0,
								// meaning the forward arm orientation point is used.
								const auto& forwardMatAngleInputs = (a_rightShoulder) ? rightShoulderMatAngleInputs.at(ArmOrientation::kForward) : leftShoulderMatAngleInputs.at(ArmOrientation::kForward);
								float forwardRatio = 1.0f - std::clamp(rsData.normMag, 0.0f, 1.0f);
								float outerPointsRatio = 1.0f - forwardRatio;
								float forwardAngleX = forwardMatAngleInputs[0] * forwardRatio;
								float forwardAngleY = forwardMatAngleInputs[1] * forwardRatio;
								float forwardAngleZ = forwardMatAngleInputs[2] * forwardRatio;

								if (rsLoc == AnalogStickLocation::kPosYAxis) 
								{
									// Right and left shoulder blend:
									// Forward and Upward.
									// Outer point: upward.
									const auto& upwardMatAngleInputs = (a_rightShoulder) ? rightShoulderMatAngleInputs.at(ArmOrientation::kUpward) : leftShoulderMatAngleInputs.at(ArmOrientation::kUpward);
									float outerPointsAngleX = upwardMatAngleInputs[0] * outerPointsRatio;
									float outerPointsAngleY = upwardMatAngleInputs[1] * outerPointsRatio;
									float outerPointsAngleZ = upwardMatAngleInputs[2] * outerPointsRatio;

									matAngleInputs = {
										forwardAngleX + outerPointsAngleX,
										forwardAngleY + outerPointsAngleY,
										forwardAngleZ + outerPointsAngleZ
									};
								}
								else if (rsLoc == AnalogStickLocation::kTopRight)
								{
									// Right shoulder blend:
									// Forward, Outward, and Upward.
									if (a_rightShoulder)
									{
										// Outer points: outward and upward.
										const auto& outwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kOutward);
										const auto& upwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kUpward);
										float outwardWeight = max(RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float upwardWeight = max(RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float totalOuterPointsWeight = outwardWeight + upwardWeight;
										float outwardAngleX = outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight);
										float outwardAngleY = outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight);
										float outwardAngleZ = outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight);
										float upwardAngleX = upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight);
										float upwardAngleY = upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight);
										float upwardAngleZ = upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight);

										float outerPointsAngleX = (outwardAngleX + upwardAngleX) * outerPointsRatio;
										float outerPointsAngleY = (outwardAngleY + upwardAngleY) * outerPointsRatio;
										float outerPointsAngleZ = (outwardAngleZ + upwardAngleZ) * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
									// Left shoulder blend:
									// Forward, Inward, and Upward.
									else
									{
										// Outer points: inward and upward.
										const auto& inwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kInward);
										const auto& upwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kUpward);
										float inwardWeight = max(RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float upwardWeight = max(RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float totalOuterPointsWeight = inwardWeight + upwardWeight;
										float inwardAngleX = inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight);
										float inwardAngleY = inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight);
										float inwardAngleZ = inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight);
										float upwardAngleX = upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight);
										float upwardAngleY = upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight);
										float upwardAngleZ = upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight);

										float outerPointsAngleX = (inwardAngleX + upwardAngleX) * outerPointsRatio;
										float outerPointsAngleY = (inwardAngleY + upwardAngleY) * outerPointsRatio;
										float outerPointsAngleZ = (inwardAngleZ + upwardAngleZ) * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
								}
								else if (rsLoc == AnalogStickLocation::kPosXAxis)
								{
									// Right shoulder blend:
									// Forward and Outward
									if (a_rightShoulder)
									{
										// Outer point: outward.
										const auto& outwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kOutward);
										float outerPointsAngleX = outwardMatAngleInputs[0] * outerPointsRatio;
										float outerPointsAngleY = outwardMatAngleInputs[1] * outerPointsRatio;
										float outerPointsAngleZ = outwardMatAngleInputs[2] * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
									// Left shoulder blend:
									// Forward and Inward.
									else
									{
										// Outer points: inward.
										const auto& inwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kInward);
										float outerPointsAngleX = inwardMatAngleInputs[0] * outerPointsRatio;
										float outerPointsAngleY = inwardMatAngleInputs[1] * outerPointsRatio;
										float outerPointsAngleZ = inwardMatAngleInputs[2] * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
								}
								else if (rsLoc == AnalogStickLocation::kBottomRight)
								{
									// Right shoulder blend:
									// Forward, Outward, and Downward.
									if (a_rightShoulder)
									{
										// Outer points: outward and downward.
										const auto& outwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kOutward);
										const auto& downwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kDownward);
										float outwardWeight = max(RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float downwardWeight = max(RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float totalOuterPointsWeight = outwardWeight + downwardWeight;
										float outwardAngleX = outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight);
										float outwardAngleY = outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight);
										float outwardAngleZ = outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight);
										float downwardAngleX = downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight);
										float downwardAngleY = downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight);
										float downwardAngleZ = downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight);

										float outerPointsAngleX = (outwardAngleX + downwardAngleX) * outerPointsRatio;
										float outerPointsAngleY = (outwardAngleY + downwardAngleY) * outerPointsRatio;
										float outerPointsAngleZ = (outwardAngleZ + downwardAngleZ) * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
									// Left shoulder blend:
									// Forward, Inward, and Downward.
									else
									{
										// Outer points: inward and downward.
										const auto& inwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kInward);
										const auto& downwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kDownward);
										float inwardWeight = max(RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float downwardWeight = max(RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float totalOuterPointsWeight = inwardWeight + downwardWeight;
										float inwardAngleX = inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight);
										float inwardAngleY = inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight);
										float inwardAngleZ = inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight);
										float downwardAngleX = downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight);
										float downwardAngleY = downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight);
										float downwardAngleZ = downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight);

										float outerPointsAngleX = (inwardAngleX + downwardAngleX) * outerPointsRatio;
										float outerPointsAngleY = (inwardAngleY + downwardAngleY) * outerPointsRatio;
										float outerPointsAngleZ = (inwardAngleZ + downwardAngleZ) * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
								}
								else if (rsLoc == AnalogStickLocation::kNegYAxis)
								{
									// Right and left shoulder blend:
									// Forward and Downward.
											
									// Outer point: downward.
									const auto& downwardMatAngleInputs = (a_rightShoulder) ? rightShoulderMatAngleInputs.at(ArmOrientation::kDownward) : leftShoulderMatAngleInputs.at(ArmOrientation::kDownward);
									float outerPointsAngleX = downwardMatAngleInputs[0] * outerPointsRatio;
									float outerPointsAngleY = downwardMatAngleInputs[1] * outerPointsRatio;
									float outerPointsAngleZ = downwardMatAngleInputs[2] * outerPointsRatio;

									matAngleInputs = {
										forwardAngleX + outerPointsAngleX,
										forwardAngleY + outerPointsAngleY,
										forwardAngleZ + outerPointsAngleZ
									};
								}
								else if (rsLoc == AnalogStickLocation::kBottomLeft)
								{
									// Right shoulder blend:
									// Forward, Inward, and Downward.
									if (a_rightShoulder)
									{
										// Outer points: inward and downward.
										const auto& inwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kInward);
										const auto& downwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kDownward);
										float inwardWeight = max(RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float downwardWeight = max(RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float totalOuterPointsWeight = inwardWeight + downwardWeight;
										float inwardAngleX = inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight);
										float inwardAngleY = inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight);
										float inwardAngleZ = inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight);
										float downwardAngleX = downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight);
										float downwardAngleY = downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight);
										float downwardAngleZ = downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight);

										float outerPointsAngleX = (inwardAngleX + downwardAngleX) * outerPointsRatio;
										float outerPointsAngleY = (inwardAngleY + downwardAngleY) * outerPointsRatio;
										float outerPointsAngleZ = (inwardAngleZ + downwardAngleZ) * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
									// Left shoulder blend:
									// Forward, Outward, and Downward.
									else
									{
										// Outer points: outward and downward.
										const auto& outwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kOutward);
										const auto& downwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kDownward);
										float outwardWeight = max(RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float downwardWeight = max(RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float totalOuterPointsWeight = outwardWeight + downwardWeight;
										float outwardAngleX = outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight);
										float outwardAngleY = outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight);
										float outwardAngleZ = outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight);
										float downwardAngleX = downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight);
										float downwardAngleY = downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight);
										float downwardAngleZ = downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight);

										float outerPointsAngleX = (outwardAngleX + downwardAngleX) * outerPointsRatio;
										float outerPointsAngleY = (outwardAngleY + downwardAngleY) * outerPointsRatio;
										float outerPointsAngleZ = (outwardAngleZ + downwardAngleZ) * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
								}
								else if (rsLoc == AnalogStickLocation::kNegXAxis)
								{
									// Right shoulder blend:
									// Forward and Inward
									if (a_rightShoulder)
									{
										// Outer point: inward.
										const auto& inwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kInward);
										float outerPointsAngleX = inwardMatAngleInputs[0] * outerPointsRatio;
										float outerPointsAngleY = inwardMatAngleInputs[1] * outerPointsRatio;
										float outerPointsAngleZ = inwardMatAngleInputs[2] * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
									// Left shoulder blend:
									// Forward and Outward.
									else
									{
										// Outer point: outward.
										const auto& outwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kOutward);
										float outerPointsAngleX = outwardMatAngleInputs[0] * outerPointsRatio;
										float outerPointsAngleY = outwardMatAngleInputs[1] * outerPointsRatio;
										float outerPointsAngleZ = outwardMatAngleInputs[2] * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
								}
								else if (rsLoc == AnalogStickLocation::kTopLeft)
								{
									// Right shoulder blend:
									// Forward, Inward, and Upward.
									if (a_rightShoulder)
									{
										// Outer points: inward and upward.
										const auto& inwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kInward);
										const auto& upwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kUpward);
										float inwardWeight = max(RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float upwardWeight = max(RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float totalOuterPointsWeight = inwardWeight + upwardWeight;
										float inwardAngleX = inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight);
										float inwardAngleY = inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight);
										float inwardAngleZ = inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight);
										float upwardAngleX = upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight);
										float upwardAngleY = upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight);
										float upwardAngleZ = upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight);

										float outerPointsAngleX = (inwardAngleX + upwardAngleX) * outerPointsRatio;
										float outerPointsAngleY = (inwardAngleY + upwardAngleY) * outerPointsRatio;
										float outerPointsAngleZ = (inwardAngleZ + upwardAngleZ) * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
									// Left shoulder blend:
									// Forward, Outward, and Upward.
									else
									{
										// Outer points: outward and upward.
										const auto& outwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kOutward);
										const auto& upwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kUpward);
										float outwardWeight = max(RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float upwardWeight = max(RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
										float totalOuterPointsWeight = outwardWeight + upwardWeight;
										float outwardAngleX = outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight);
										float outwardAngleY = outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight);
										float outwardAngleZ = outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight);
										float upwardAngleX = upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight);
										float upwardAngleY = upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight);
										float upwardAngleZ = upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight);

										float outerPointsAngleX = (outwardAngleX + upwardAngleX) * outerPointsRatio;
										float outerPointsAngleY = (outwardAngleY + upwardAngleY) * outerPointsRatio;
										float outerPointsAngleZ = (outwardAngleZ + upwardAngleZ) * outerPointsRatio;

										matAngleInputs = {
											forwardAngleX + outerPointsAngleX,
											forwardAngleY + outerPointsAngleY,
											forwardAngleZ + outerPointsAngleZ
										};
									}
								}
							}

							const auto nodeNameHash = Hash(a_shoulderNode->name);
							bool hasBeenSet = a_p->mm->nrm->lastSetArmLocalRotations.contains(nodeNameHash);
							RE::NiMatrix3 newLocal = !hasBeenSet ? a_shoulderNode->local.rotate : a_p->mm->nrm->lastSetArmLocalRotations.at(nodeNameHash);
							auto& worldTransform = a_shoulderNode->world;
							const float oldYaw = a_p->mm->nrm->lastSetArmRotationInputs.contains(nodeNameHash) ? a_p->mm->nrm->lastSetArmRotationInputs[nodeNameHash][0] : matAngleInputs[0];
							const float oldPitch = a_p->mm->nrm->lastSetArmRotationInputs.contains(nodeNameHash) ? a_p->mm->nrm->lastSetArmRotationInputs[nodeNameHash][1] : matAngleInputs[1];
							float yaw = Util::InterpolateSmootherStep(oldYaw, matAngleInputs[0], min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
							float pitch = Util::InterpolateSmootherStep(oldPitch, matAngleInputs[1], min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
							Util::SetRotationMatrix2(worldTransform.rotate, pitch, yaw + a_p->coopActor->GetHeading(false), 0.0f);

							// Set local rotation from new world transform and inverse of parent transform.
							if (a_shoulderNode->parent)
							{
								RE::NiTransform inverseParent = a_shoulderNode->parent->world.Invert();
								newLocal = (inverseParent * worldTransform).rotate;
							}

							if (!hasBeenSet)
							{
								a_p->mm->nrm->lastSetArmLocalRotations.insert({ nodeNameHash, newLocal });
							}
							else
							{
								a_p->mm->nrm->lastSetArmLocalRotations[nodeNameHash] = newLocal;
							}

							if (!hasBeenSet)
							{
								a_p->mm->nrm->lastSetArmRotationInputs.insert({ nodeNameHash, { yaw, pitch, 0.0f } });
							}
							else
							{
								a_p->mm->nrm->lastSetArmRotationInputs[nodeNameHash][0] = yaw;
								a_p->mm->nrm->lastSetArmRotationInputs[nodeNameHash][1] = pitch;
								a_p->mm->nrm->lastSetArmRotationInputs[nodeNameHash][2] = 0.0f;
							}
						}
					}
				}
			};

			if (rotatingLeftShoulder && rotatingRightShoulder) 
			{
				tiltShoulder(data3D.get(), leftShoulderNode, false);
				tiltShoulder(data3D.get(), rightShoulderNode, true);
			}
			else if (rotatingLeftShoulder)
			{
				tiltShoulder(data3D.get(), leftShoulderNode, false);
			}
			else if (rotatingRightShoulder)
			{
				tiltShoulder(data3D.get(), rightShoulderNode, true);
			}
		}
	}

	void GlobalCoopData::TwistAndBendArms(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Ensure no modifications are made if the player has their weapon drawn.
		if (a_p->coopActor->IsWeaponDrawn()) 
		{
			return;
		}

		auto& glob = GetSingleton();
		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings)
		{
			return;
		}

		auto loadedData = a_p->coopActor->loadedData; 
		if (!loadedData)
		{
			return;
		}

		auto data3D = loadedData->data3D; 
		if (!data3D || !data3D->parent)
		{
			return;
		}

		auto leftForearmNode = data3D->GetObjectByName(strings->npcLForearm);
		auto rightForearmNode = data3D->GetObjectByName("NPC R Forearm [RLar]");
		auto leftHandNode = data3D->GetObjectByName("NPC L Hand [LHnd]");
		auto rightHandNode = data3D->GetObjectByName("NPC R Hand [RHnd]");
		if (!leftForearmNode || !leftHandNode || !rightForearmNode || !rightHandNode)
		{
			return;
		}

		auto twistAndBendArm = 
		[&a_p, &glob](RE::NiAVObject* a_data3D, RE::NiAVObject* a_forearmNode, RE::NiAVObject* a_handNode, bool a_onlyRotatingShoulder, bool a_rightSide) 
		{		
			if (!a_data3D || !a_forearmNode || !a_handNode) 
			{
				return;
			}
						
			const auto& rsData = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
			const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
			const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
			const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);

			const auto forearmNameHash = Hash(a_forearmNode->name);
			const auto handNameHash = Hash(a_handNode->name);
			bool forearmRotHasBeenSet = a_p->mm->nrm->lastSetArmLocalRotations.contains(forearmNameHash);
			bool handRotHasBeenSet = a_p->mm->nrm->lastSetArmLocalRotations.contains(handNameHash);
			RE::NiMatrix3 newForearmLocalRot = !forearmRotHasBeenSet ? a_forearmNode->local.rotate : a_p->mm->nrm->lastSetArmLocalRotations.at(forearmNameHash);
			RE::NiMatrix3 newHandLocalRot = !handRotHasBeenSet ? a_handNode->local.rotate : a_p->mm->nrm->lastSetArmLocalRotations.at(handNameHash);
			const float forearmOldYaw = a_p->mm->nrm->lastSetArmRotationInputs.contains(forearmNameHash) ? a_p->mm->nrm->lastSetArmRotationInputs[forearmNameHash][0] : 0.0f;
			const float forearmOldPitch = a_p->mm->nrm->lastSetArmRotationInputs.contains(forearmNameHash) ? a_p->mm->nrm->lastSetArmRotationInputs[forearmNameHash][1] : 0.0f;
			const float forearmOldRoll = a_p->mm->nrm->lastSetArmRotationInputs.contains(forearmNameHash) ? a_p->mm->nrm->lastSetArmRotationInputs[forearmNameHash][2] : 0.0f;
			const float handOldYaw = a_p->mm->nrm->lastSetArmRotationInputs.contains(handNameHash) ? a_p->mm->nrm->lastSetArmRotationInputs[handNameHash][0] : a_rightSide ? -(0.5f * PI) : (0.5f * PI);
			const float handOldPitch = a_p->mm->nrm->lastSetArmRotationInputs.contains(handNameHash) ? a_p->mm->nrm->lastSetArmRotationInputs[handNameHash][1] : 0.0f;
			const float handOldRoll = a_p->mm->nrm->lastSetArmRotationInputs.contains(handNameHash) ? a_p->mm->nrm->lastSetArmRotationInputs[handNameHash][2] : 0.0f;
			float forearmYaw = forearmOldYaw;
			float forearmPitch = forearmOldPitch;
			float forearmRoll = forearmOldRoll;
			float handYaw = handOldYaw;
			float handPitch = handOldPitch;
			float handRoll = handOldRoll;
			// Bend arms if not only rotating shoulders.
			if (!a_onlyRotatingShoulder) 
			{
				float xDisp = std::clamp(rsData.xComp * rsData.normMag, -1.0f, 1.0f);
				float yDisp = std::clamp(rsData.yComp * rsData.normMag, -1.0f, 1.0f);

				float targetForearmPitch = forearmOldPitch;
				float targetForearmRoll = forearmOldRoll;
				float targetForearmYaw = forearmOldYaw;
				float targetHandPitch = handOldPitch;
				float targetHandRoll = handOldRoll;
				float targetHandYaw = handOldYaw;
				if (rsData.normMag != 0.0f)
				{
					if (a_p->pam->IsPerformingOneOf(InputAction::kRotateLeftHand, InputAction::kRotateRightHand))
					{
						// When clicking in the RS, change forearm yaw and hand yaw together along the +X axis,
						// and hand roll along the -X axis. 
						// Change hand pitch along the Y axis.
						// Flip angle sign for the other hand.
						if (xDisp > 0.0f) 
						{
							targetForearmYaw = xDisp * (0.25f * PI) - (PI / 60.0f);
							targetHandYaw = (xDisp - 1.0f) * (0.5f * PI);
							if (!a_rightSide)
							{
								targetForearmYaw = -targetForearmYaw;
								targetHandYaw = -targetHandYaw;
							}
						}
						else
						{
							// Hand roll.
							if (xDisp >= -0.333333f)
							{
								targetHandRoll = (-xDisp * 4.0f) * (0.25f * PI);
							}
							else if (xDisp >= -0.666666f)
							{
								targetHandRoll = (2.666664f + xDisp * 4.0f) * (0.25f * PI);
							}
							else
							{
								targetHandRoll = ((0.666666f + xDisp) * 3.0f) * (0.25f * PI);
							}

							if (!a_rightSide)
							{
								targetHandRoll = -targetHandRoll;
							}
						}

						// Hand pitch.
						targetHandPitch = yDisp * (0.5f * PI);
					}
					else
					{
						// When NOT clicking in the RS, change forearm pitch along the +Y axis and
						// forearm roll along the -Y axis.
						// Change forearm yaw along the X axis.
						// Flip angle sign for the other side.
						if (yDisp > 0.0f) 
						{
							// Forearm pitch.
							targetForearmPitch = yDisp * (0.5f * PI);
						}
						else
						{
							// Forearm roll.
							targetForearmRoll = -yDisp * (0.25f * PI) + (PI / 60.0f);
							if (!a_rightSide)
							{
								targetForearmRoll = -targetForearmRoll;
							}
						}
											
						if (xDisp > 0.0f) 
						{
							// Forearm yaw.
							targetForearmYaw = xDisp * (0.25f * PI) - (PI / 60.0f);
						}
						else
						{
							// Forearm yaw.
							targetForearmYaw = xDisp * (0.25f * PI) + (PI / 60.0f);
						}

						if (!a_rightSide)
						{
							targetForearmYaw = -targetForearmYaw;
						}
					}
				}

				// Set next forearm/hand rotation angles if different from previous ones.
				if (forearmOldPitch != targetForearmPitch) 
				{
					forearmPitch = Util::InterpolateSmootherStep(forearmOldPitch, targetForearmPitch, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
				}

				if (forearmOldRoll != targetForearmRoll)
				{
					forearmRoll = Util::InterpolateSmootherStep(forearmOldRoll, targetForearmRoll, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
				}

				if (forearmOldYaw != targetForearmYaw)
				{
					forearmYaw = Util::InterpolateSmootherStep(forearmOldYaw, targetForearmYaw, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
				}

				if (handOldPitch != targetHandPitch)
				{
					handPitch = Util::InterpolateSmootherStep(handOldPitch, targetHandPitch, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
				}

				if (handOldRoll != targetHandRoll)
				{
					handRoll = Util::InterpolateSmootherStep(handOldRoll, targetHandRoll, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
				}

				if (handOldYaw != targetHandYaw)
				{
					handYaw = Util::InterpolateSmootherStep(handOldYaw, targetHandYaw, min(1.0f, *g_deltaTimeRealTime * a_p->mm->nrm->interpFactor));
				}
			}

			RE::NiPoint3 locXAxis{ right };
			RE::NiPoint3 locYAxis{ forward };
			RE::NiPoint3 locZAxis{ up };

			// Forearm node rotations.
			Util::SetRotationMatrix3(newForearmLocalRot, locXAxis, locYAxis, locZAxis, forearmPitch, forearmYaw, forearmRoll);
			if (!forearmRotHasBeenSet)
			{
				a_p->mm->nrm->lastSetArmLocalRotations.insert({ forearmNameHash, newForearmLocalRot });
			}
			else
			{
				a_p->mm->nrm->lastSetArmLocalRotations[forearmNameHash] = newForearmLocalRot;
			}

			if (!forearmRotHasBeenSet)
			{
				a_p->mm->nrm->lastSetArmRotationInputs.insert({ forearmNameHash, { forearmYaw, forearmPitch, forearmRoll } });
			}
			else
			{
				a_p->mm->nrm->lastSetArmRotationInputs[forearmNameHash][0] = forearmYaw;
				a_p->mm->nrm->lastSetArmRotationInputs[forearmNameHash][1] = forearmPitch;
				a_p->mm->nrm->lastSetArmRotationInputs[forearmNameHash][2] = forearmRoll;
			}

			// Hand node rotations.
			Util::SetRotationMatrix3(newHandLocalRot, locXAxis, locYAxis, locZAxis, handPitch, handYaw, handRoll);
			if (!handRotHasBeenSet)
			{
				a_p->mm->nrm->lastSetArmLocalRotations.insert({ handNameHash, newHandLocalRot });
			}
			else
			{
				a_p->mm->nrm->lastSetArmLocalRotations[handNameHash] = newHandLocalRot;
			}

			if (!handRotHasBeenSet)
			{
				a_p->mm->nrm->lastSetArmRotationInputs.insert({ handNameHash, { handYaw, handPitch, handRoll } });
			}
			else
			{
				a_p->mm->nrm->lastSetArmRotationInputs[handNameHash][0] = handYaw;
				a_p->mm->nrm->lastSetArmRotationInputs[handNameHash][1] = handPitch;
				a_p->mm->nrm->lastSetArmRotationInputs[handNameHash][2] = handRoll;
			}
		};


		bool rotatingLeftArm = a_p->pam->IsPerformingOneOf(InputAction::kRotateLeftShoulder, InputAction::kRotateLeftForearm, InputAction::kRotateLeftHand);
		bool rotatingRightArm = a_p->pam->IsPerformingOneOf(InputAction::kRotateRightShoulder, InputAction::kRotateRightForearm, InputAction::kRotateRightHand);
		bool onlyRotatingLeftShoulder = a_p->pam->IsPerforming(InputAction::kRotateLeftShoulder) && a_p->pam->IsNotPerformingAnyOf(InputAction::kRotateLeftForearm, InputAction::kRotateLeftHand);
		bool onlyRotatingRightShoulder = a_p->pam->IsPerforming(InputAction::kRotateRightShoulder) && a_p->pam->IsNotPerformingAnyOf(InputAction::kRotateRightForearm, InputAction::kRotateRightHand);
		if (rotatingLeftArm && rotatingRightArm) 
		{
			twistAndBendArm(data3D.get(), leftForearmNode, leftHandNode, onlyRotatingLeftShoulder, false);
			twistAndBendArm(data3D.get(), rightForearmNode, rightHandNode, onlyRotatingRightShoulder, true);
		}
		else if (rotatingLeftArm)
		{
			twistAndBendArm(data3D.get(), leftForearmNode, leftHandNode, onlyRotatingLeftShoulder, false);
		}
		else if (rotatingRightArm)
		{
			twistAndBendArm(data3D.get(), rightForearmNode, rightHandNode, onlyRotatingRightShoulder, true);
		}
	}

	bool GlobalCoopData::PerformArmCollisionRaycastCheck(const std::shared_ptr<CoopPlayer>& a_p, const glm::vec4& a_startPos, const glm::vec4& a_endPos, const RE::NiPoint3& a_armNodeVelocity, const RE::NiPoint3& a_armPointVelocity, const ArmNodeType& a_armNodeType)
	{
		// Perform raycast and iterate through results, 
		// applying impulses to hit actors and forces to inanimate objects
		// at the raycasts' hit positions.
		// If an actor is hit hard enough 
		// (hit speed above certain thresholds, depending on the connecting node),
		// knock the actor down and apply damage.

		auto& glob = GetSingleton();
		const auto& rsLinSpeed = glob.cdh->GetAnalogStickState(a_p->controllerID, false).stickLinearSpeed;
		auto raycastResults = Raycast::GetAllHavokCastHitResults(a_startPos, a_endPos);
		for (const auto& result : raycastResults)
		{
			if (!result.hit || !result.hitObject || !result.hitObject.get())
			{
				continue;
			}

			if (auto hitRefrPtr = RE::TESObjectREFRPtr(Util::GetRefrFrom3D(result.hitObject.get()));
				hitRefrPtr && hitRefrPtr.get() && Util::HandleIsValid(hitRefrPtr->GetHandle()) && hitRefrPtr != a_p->coopActor)
			{

				auto hitActor = hitRefrPtr->As<RE::Actor>();
				// Direct skeleton hits deform the actor ragdoll much more when and impulse is applied.
				bool skeletonHit = hitActor && std::string(result.hitObject->name).contains("skeleton");
				if (auto hitHkpRigidBody = Util::GethkpRigidBody(result.hitObject.get()); hitHkpRigidBody)
				{
					Util::NativeFunctions::hkpEntity_Activate(hitHkpRigidBody.get());
					if (hitActor)
					{
						// Stop hit actor from attacking when the slap connects.
						if (hitActor->IsAttacking())
						{
							hitActor->NotifyAnimationGraph("attackStop");
							hitActor->NotifyAnimationGraph("recoilStart");
						}

						if (auto precisionAPI4 = ALYSLC::PrecisionCompat::g_precisionAPI4; precisionAPI4)
						{
							auto hitPosVec = TohkVector4(result.hitPos) * GAME_TO_HAVOK;
							auto hitPosPointVel = ToNiPoint3
							(
								hitHkpRigidBody->motion.GetPointVelocity(TohkVector4(result.hitPos) * GAME_TO_HAVOK) * HAVOK_TO_GAME
							);
							float hitSpeed = a_armPointVelocity.Length();
							// Knock down if over a certain RS linear speed.
							bool knockOut = false;
							switch (a_armNodeType)
							{
							case ArmNodeType::kForearm:
							{
								knockOut = hitSpeed > 1100.0f;
								break;
							}
							case ArmNodeType::kHand:
							{
								knockOut = hitSpeed > 1200.0f;
								break;
							}
							case ArmNodeType::kShoulder:
							{
								knockOut = hitSpeed > 800.0f;
								break;
							}
							default:
								break;
							}

							// Apply weaker impulse mult when about to ragdoll or when hitting an actor's skeleton.nif,
							// since the impulse effects are more pronounced by default in these two cases.
							float impulseMult = 1.0f;
							if (knockOut) 
							{
								impulseMult = 0.5f;
							}
							else if (skeletonHit)
							{
								impulseMult = 0.6f;
							}
							else
							{
								impulseMult = 1.0f;
							}

							precisionAPI4->ApplyHitImpulse2(hitActor->GetHandle(), a_p->coopActor->GetHandle(), hitHkpRigidBody.get(), a_armPointVelocity, hitPosVec, impulseMult);
							// Damage scales with thrown object damage.
							if (knockOut)
							{
								a_p->tm->rmm->AddGrabbedRefr(a_p, hitRefrPtr->GetHandle());
								a_p->tm->SetIsGrabbing(false);
							}

							// REMOVE when done debugging.
							/*DebugAPI::QueuePoint3D(result.hitPos, Settings::vuOverlayRGBAValues[a_p->playerID], a_armPointVelocity.Length() / 200.0f, 1.0f);
							logger::debug("[GLOB] PerformArmCollisionRaycastCheck: {} hit {} (0x{:X}, {}, {}) with {} node, RS lin speed {}, impulse mult {}, and node/point velocity {}, {}. Hit pos point vel: {}, rel vels: {}, {}. {}",
								a_p->coopActor->GetName(),
								hitRefrPtr->GetName(),
								hitRefrPtr->formID,
								hitRefrPtr->GetBaseObject() ? *hitRefrPtr->GetBaseObject()->formType : RE::FormType::None,
								result.hitObject->name,
								a_armNodeType == ArmNodeType::kForearm ? "forearm" : a_armNodeType == ArmNodeType::kHand ? "hand" : "shoulder",
								rsLinSpeed,
								impulseMult,
								a_armNodeVelocity.Length(),
								a_armPointVelocity.Length(),
								hitPosPointVel.Length(),
								(a_armNodeVelocity - hitPosPointVel).Length(),
								(a_armPointVelocity - hitPosPointVel).Length(),
								knockOut ? "KO!" : "SLAPPED!");*/
						}
					}
					else
					{
						const auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
						float mass = hitHkpRigidBody->motion.GetMass();;
						RE::hkVector4 hitDirVec = RE::hkVector4(a_armPointVelocity.x, a_armPointVelocity.y, a_armPointVelocity.z, 0.0f);
						hitDirVec = hitDirVec / hitDirVec.Length3();
						float hitSpeed = a_armPointVelocity.Length();
						// Attempt to normalize somewhat based on mass.
						// Additional force if quickly flicking the RS.
						// Use RS speed to adjust impulse.
						float rsImpulseMult = Util::InterpolateEaseIn(1.0f, 1.5f, std::clamp(log(rsLinSpeed) / 3.0f, 0.0f, 1.0f), 3.0f);
						auto hitVelocityVec = hitDirVec * hitSpeed * std::clamp((hitHkpRigidBody->motion.GetMass() + 1.0f) / 50.0f, 0.0f, 2.0f) * rsImpulseMult * GAME_TO_HAVOK;
						hitHkpRigidBody->motion.ApplyForce(*g_deltaTimeRealTime * 1000.0f, hitVelocityVec);
					}

					return true;
				}
			}
		}

		return false;
	}

	void GlobalCoopData::RescaleHMS(RE::Actor* a_playerActor, const float& a_baseLevel)
	{
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto& glob = GetSingleton();
		if (!p1 || !a_playerActor || !glob.serializablePlayerData.contains(a_playerActor->formID)) 
		{
			logger::error("[GLOB] ERR: RescaleHMS: P1 is invalid: {}, co-op actor is invalid: {}, serializable data does not contain player FID: {}.", 
				(bool)!p1, (bool)!a_playerActor, !glob.serializablePlayerData.contains(a_playerActor->formID));
		}

		logger::debug("[GLOB] RescaleHMS: {}: base level: {}.", a_playerActor->GetName(), a_baseLevel);
		const auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
		if (a_playerActor == p1) 
		{
			// If player 1 has leveled up at least once and has a recorded increased their health, magicka, or stamina,
			// the saved serialized data is not the default saved data and the HMS AVs can be set to these saved values.
			if (a_playerActor->GetLevel() > 1 && (data->hmsPointIncreasesList[0] > 0.0f || data->hmsPointIncreasesList[1] > 0.0f || data->hmsPointIncreasesList[2] > 0.0f))
			{
				if (a_playerActor->GetBaseActorValue(RE::ActorValue::kHealth) != data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0])
				{
					a_playerActor->SetBaseActorValue(RE::ActorValue::kHealth, data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0]);
				}

				if (a_playerActor->GetBaseActorValue(RE::ActorValue::kMagicka) != data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1])
				{
					a_playerActor->SetBaseActorValue(RE::ActorValue::kMagicka, data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1]);
				}

				if (a_playerActor->GetBaseActorValue(RE::ActorValue::kStamina) != data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2])
				{
					a_playerActor->SetBaseActorValue(RE::ActorValue::kStamina, data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2]);
				}

				// REMOVE
				logger::debug("[GLOB] RescaleHMS: Resetting P1's base HMS AVs to ({}, {}, {}) -> ({} + {}, {} + {}, {} + {})",
					data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0],
					data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1],
					data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2],
					data->hmsBasePointsList[0], data->hmsPointIncreasesList[0],
					data->hmsBasePointsList[1], data->hmsPointIncreasesList[1],
					data->hmsBasePointsList[2], data->hmsPointIncreasesList[2]);
			}
			else
			{
				logger::debug("[GLOB] RescaleHMS: P1 has not leveled up yet: {}, or has not increased their HMS AVs: {}. No rescaling necessary.",
					a_playerActor->GetLevel() == 1,
					data->hmsPointIncreasesList[0] == 0.0f && data->hmsPointIncreasesList[1] == 0.0f && data->hmsPointIncreasesList[2] == 0.0f);
			}
		}
		else
		{
			// Has recorded level up.
			if (a_baseLevel != 0) 
			{
				a_playerActor->SetBaseActorValue(RE::ActorValue::kHealth, data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0]);
				logger::debug("[GLOB] RescaleHMS: {}'s health AV at base level {} is {}. Health inc: {}, setting health to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[0],
					data->hmsPointIncreasesList[0],
					data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0]);

				a_playerActor->SetBaseActorValue(RE::ActorValue::kMagicka, data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1]);
				logger::debug("[GLOB] RescaleHMS: {}'s magicka AV at base level {} is {}. Magicka inc: {}, setting magicka to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[1],
					data->hmsPointIncreasesList[1],
					data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1]);

				a_playerActor->SetBaseActorValue(RE::ActorValue::kStamina, data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2]);
				logger::debug("[GLOB] RescaleHMS: {}'s stamina AV at base level {} is {}. Stamina inc: {}, setting stamina to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[2],
					data->hmsPointIncreasesList[2],
					data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2]);
			}
			else if (a_playerActor->GetRace() && a_playerActor->GetActorBase())
			{
				// Before first level up, use sum of the race's starting HMS AVs and the actor base's HMS offsets.
				data->hmsBasePointsList[0] = a_playerActor->race->data.startingHealth + a_playerActor->GetActorBase()->actorData.healthOffset;
				data->hmsBasePointsList[1] = a_playerActor->race->data.startingMagicka + a_playerActor->GetActorBase()->actorData.magickaOffset;
				data->hmsBasePointsList[2] = a_playerActor->race->data.startingStamina + a_playerActor->GetActorBase()->actorData.staminaOffset;
				logger::debug("[GLOB] RescaleHMS: {} has not leveled up in co-op yet. Scaling HMS AVs down to their base values: {}, {}, {}.",
					a_playerActor->GetName(),
					data->hmsBasePointsList[0],
					data->hmsBasePointsList[1],
					data->hmsBasePointsList[2]);

				a_playerActor->SetBaseActorValue(RE::ActorValue::kHealth, data->hmsBasePointsList[0]);
				logger::debug("[GLOB] RescaleHMS: {}'s health AV at base level {} is {}. Health inc: {}, setting health to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[0],
					data->hmsPointIncreasesList[0],
					data->hmsBasePointsList[0] + data->hmsPointIncreasesList[0]);

				a_playerActor->SetBaseActorValue(RE::ActorValue::kMagicka, data->hmsBasePointsList[1]);
				logger::debug("[GLOB] RescaleHMS: {}'s magicka AV at base level {} is {}. Magicka inc: {}, setting magicka to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[1],
					data->hmsPointIncreasesList[1],
					data->hmsBasePointsList[1] + data->hmsPointIncreasesList[1]);

				a_playerActor->SetBaseActorValue(RE::ActorValue::kStamina, data->hmsBasePointsList[2]);
				logger::debug("[GLOB] RescaleHMS: {}'s stamina AV at base level {} is {}. Stamina inc: {}, setting stamina to {}",
					a_playerActor->GetName(),
					a_baseLevel,
					data->hmsBasePointsList[2],
					data->hmsPointIncreasesList[2],
					data->hmsBasePointsList[2] + data->hmsPointIncreasesList[2]);
			}
		}
	}

	void GlobalCoopData::RescaleSkillAVs(RE::Actor* a_playerActor)
	{
		// Preconditions:
		// 1. Player actor is *gasp* actually a player actor,
		// 2. Player 1 is valid,
		// 3. Serializable data contains data for player actor.

		logger::debug("[GLOB] RescaleSkillAVs for {}.", a_playerActor->GetName());
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto& glob = GetSingleton();
		if (!p1 || !a_playerActor || !glob.serializablePlayerData.contains(a_playerActor->formID))
		{
			logger::error("[GLOB] ERR: RescaleSkillAVs: P1 is invalid: {}, co-op actor is invalid: {}, serializable data does not contain player FID: {}.",
				(bool)!p1, (bool)!a_playerActor, !glob.serializablePlayerData.contains(a_playerActor->formID));
			return;
		}

		const auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
		Skill currentSkill = Skill::kTotal;
		RE::ActorValue currentAV = RE::ActorValue::kNone;
		for (auto i = 0; i < Skill::kTotal; ++i)
		{
			currentSkill = static_cast<Skill>(i);
			if (SKILL_TO_AV_MAP.contains(currentSkill))
			{
				currentAV = glob.SKILL_TO_AV_MAP.at(currentSkill);
				logger::debug("[GLOB] RescaleSkillAVs: base: {}, current: {}, modifiers (d, p, t): {}, {}, {}.",
					a_playerActor->GetBaseActorValue(currentAV),
					a_playerActor->GetActorValue(currentAV),
					a_playerActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, currentAV),
					a_playerActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, currentAV),
					a_playerActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, currentAV));
				if (SHARED_SKILL_AVS_SET.contains(currentAV))
				{
					// If shared, get the highest level for this AV among all co-op players, active or inactive,
					// and set this player's AV to that level.
					if (auto value = GlobalCoopData::GetHighestSharedAVLevel(currentAV); value != -1.0f)
					{
						// Since the base value is set directly and synced for each player,
						// we do not keep track of individual increases to shared AVs.
						data->skillBaseLevelsList[i] = value;
						data->skillLevelIncreasesList[i] = 0.0f;
						a_playerActor->SetBaseActorValue(currentAV, value);
						logger::debug("[GLOB] RescaleSkillAVs: Set {}'s SHARED skill AV {} to {}.",
							a_playerActor->GetName(), std::format("{}", currentAV), value);
					}
				}
				else
				{
					// Add recorded skill increases on top of serialized base skill level to get new level for this skill.
					a_playerActor->SetBaseActorValue(currentAV, data->skillBaseLevelsList[i] + data->skillLevelIncreasesList[i]);
					logger::debug("[GLOB] RescaleSkillAVs: {}'s INDEP skill AV {} at base level {} is {} + {}. Set to {} ({}).",
						a_playerActor->GetName(),
						std::format("{}", currentAV),
						data->firstSavedLevel,
						data->skillBaseLevelsList[i],
						data->skillLevelIncreasesList[i],
						data->skillBaseLevelsList[i] + data->skillLevelIncreasesList[i],
						Settings::bStackCoopPlayerSkillAVAutoScaling ? "AUTO-SCALING STACKS" : "AUTO-SCALING DOES NOT STACK");
				}
			}
		}
	}

	void GlobalCoopData::ResetPerkDataOnBaseSkillAVChange(RE::Actor* a_playerActor)
	{
		// Remove all perks from this player, remove shared perks from
		// all players, and reset all shared perk-related serialized data
		// when the player's saved base skill AVs change.

		logger::debug("[GLOB] ResetPerkDataOnBaseSkillAVChange: {}", a_playerActor->GetName());
		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (!p1 || !a_playerActor || !glob.serializablePlayerData.contains(a_playerActor->formID))
		{
			logger::error("[GLOB] ERR: ResetPerkDataOnBaseSkillAVChange: P1 is invalid: {}, co-op actor is invalid: {}, serializable data does not contain player FID: {}.",
				(bool)!p1, (bool)!a_playerActor, !glob.serializablePlayerData.contains(a_playerActor->formID));
		}

		auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
		// If P1 has leveled up at least once, at least one player will have selected perks.
		if (p1->GetLevel() > 1)
		{
			// Clear all of the given player's perks.
			data->ClearUnlockedPerks();
			data->availablePerkPoints = p1->GetLevel() - 1;
			data->extraPerkPoints = 0;
			data->prevTotalUnlockedPerks = 0;
			data->usedPerkPoints = 0;

			// Remove all shared perks for the passed in player.
			std::set<RE::BGSPerk*> sharedPerksRemoved;
			std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> removeAllSharedPerks = 
			[p1, &sharedPerksRemoved](RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) {
				if (a_node)
				{
					bool shared = SHARED_SKILL_NAMES_SET.contains(a_node->associatedSkill->enumName);
					if (shared) 
					{
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
								if (a_actor == p1) 
								{
									bool succ = Util::Player1RemovePerk(perkToRemove);
									logger::debug("[GLOB] ResetPerkDataOnBaseSkillAVChange. Removing shared perk {} (0x{:X}) from p1's perks list: {}.",
										perkToRemove->GetName(), perkToRemove->formID, succ ? "SUCC" : "FAIL");
									sharedPerksRemoved.insert(perkToRemove);
								}
								else
								{
									bool succ = Util::ChangePerk(a_actor, perkToRemove, false);
									logger::debug("[GLOB] ResetPerkDataOnBaseSkillAVChange. Removing shared perk {} (0x{:X}) from {}'s perks list: {}.",
										perkToRemove->GetName(), perkToRemove->formID, a_actor->GetName(), succ ? "SUCC" : "FAIL");
									sharedPerksRemoved.insert(perkToRemove);
								}
							}

							perkStack.pop();
						}
					}
				}
			};

			RE::Actor* playerActor = nullptr;
			if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
			{
				for (auto& [fid, data] : glob.serializablePlayerData)
				{
					if (fid == 0x14)
					{
						playerActor = p1;
					}
					else
					{
						playerActor = dataHandler->LookupForm<RE::Actor>(fid & 0x00FFFFFF, PLUGIN_NAME);
					}

					if (playerActor)
					{
						logger::debug("[GLOB] ResetPerkDataOnBaseSkillAVChange: Removing {}'s shared perks.", playerActor->GetName());
						Util::TraverseAllPerks(playerActor, removeAllSharedPerks);
						const auto& unlockedPerks = data->GetUnlockedPerksList();
						if (unlockedPerks.size() > 0) 
						{
							std::vector<RE::BGSPerk*> newUnlockedPerks{};
							logger::debug("[GLOB] ResetPerkDataOnBaseSkillAVChange: {} had {} unlocked perks.", playerActor->GetName(), unlockedPerks.size());
							// Construct new unlocked perks list with shared perks removed.
							for (auto perk : unlockedPerks)
							{
								if (!sharedPerksRemoved.contains(perk))
								{
									newUnlockedPerks.emplace_back(perk);
								}
							}

							data->SetUnlockedPerks(newUnlockedPerks);
							data->sharedPerksUnlocked = 0;
							logger::debug("[GLOB] ResetPerkDataOnBaseSkillAVChange: {} now has {} unlocked perks after shared perk removal.", playerActor->GetName(), data->GetUnlockedPerksList().size());
						}
					}
				}
			}

			logger::debug("[GLOB] ResetPerkDataOnBaseSkillAVChange: Adjust all players' perk counts after shared perk removal.");
			AdjustAllPlayerPerkCounts();
		}
	}

	bool GlobalCoopData::TriggerAVAutoScaling(RE::Actor* a_playerActor, bool&& a_updateBaseAVs) 
	{
		// Preconditions:
		// 1. Player actor who changed classes is *gasp* actually a player actor,
		// or nullptr if all players must have their AVs auto-scaled (no class change),
		// 2. Player 1 is valid,
		// 3. Serializable data contains data for player actor, if one is given.
		// Returns true if successful or if no auto-scaling was necessary.

		auto& glob = GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			logger::error("[GLOB] ERR: TriggerAVAutoScaling: P1 is invalid: {}.", (bool)!p1);
		}

		logger::debug("[GLOB] TriggerAVAutoScaling: {}{}. Update base AVs: {}.", 
			a_playerActor ? "Player Changed Class/Race: " : "All Active Players",
			a_playerActor ? a_playerActor->GetName() : "",
			a_updateBaseAVs);

		// If not updating all player's base AV levels or stacking auto-scaling on top 
		// of all players' skill level increments, auto-scale all AVs 
		// and then set the new base AVs if necessary.
		if (!a_updateBaseAVs || Settings::bStackCoopPlayerSkillAVAutoScaling)
		{
			// Just modify P1's level by 1, up or down and then reset to the original level to trigger auto-scaling.
			const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
			const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
			if (script)
			{
				auto p1Level = p1->GetLevel();
				uint16_t targetLevel = p1Level < UINT16_MAX ? p1Level + 1 : p1Level - 1;
				uint16_t savedLevel = p1Level;
				float savedXP = p1->skills->data->xp;
				logger::debug("[GLOB] TriggerAVAutoScaling: Before inc/dec: current XP, threshold: {}, {}, current level: {}, target level: {}.",
					p1->skills->data->xp, p1->skills->data->levelThreshold, p1Level, targetLevel);

				p1->skills->data->xp = 0.0f;
				// Set to target level.
				script->SetCommand("SetLevel " + std::to_string(targetLevel));
				script->CompileAndRun(p1);
				// Set to original level.
				script->SetCommand("SetLevel " + std::to_string(savedLevel));
				script->CompileAndRun(p1);
				// Restore XP.
				p1->skills->data->xp = savedXP;

				delete script;
				logger::debug("[GLOB] TriggerAVAutoScaling: After inc/dec: current XP, threshold: {}, {}, current level: {}.",
					p1->skills->data->xp, p1->skills->data->levelThreshold, p1Level);

				// Update base AVs when playing Enderal, since companion players' base AVs start at 5, instead of 15.
				if (a_updateBaseAVs || ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
				{
					logger::debug("[GLOB] TriggerAVAutoScaling: Update base AVs for all players. Enderal: {}, STACKED scaling: {}.",
						ALYSLC::EnderalCompat::g_enderalSSEInstalled, Settings::bStackCoopPlayerSkillAVAutoScaling);
					// Set base Skill AV levels to newly-scaled ones.
					for (const auto& p : glob.coopPlayers)
					{
						if (p->isActive && p->coopActor && glob.serializablePlayerData.contains(p->coopActor->formID))
						{
							auto& data = glob.serializablePlayerData.at(p->coopActor->formID);
							data->skillBaseLevelsList = Util::GetActorSkillAVs(p->coopActor.get());
						}
					}
				}

				return true;
			}

			return false;
		}
		else
		{
			// Otherwise, set the given player/all players' base AVs to the auto-scaled levels at their first saved level.
			// The player(s)' skill level increments will then be applied on top of these new base AV levels,
			// instead of the auto-scaled levels at their current level.
			auto autoScaleAndSetBaseAVs = [p1, &glob](RE::Actor* a_playerActor) {
				const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
				const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
				if (!script || !a_playerActor)
				{
					return false;
				}

				auto& data = glob.serializablePlayerData.at(a_playerActor->formID);
				// Dip down to the player's first saved level and update base skill AVs, not HMS AVs which will remain unchanged
				// and reflect the values that the player invested in them through the level up menu.
				// May add full respec option eventually.
				// Have to do this when not stacking skill increases with auto-scaling 
				// and if the player has changed their class or race.

				uint16_t savedP1Level = p1->GetLevel();
				// If P1 is still at level 1, move up one level to trigger auto-scaling instead.
				// Should not be 0, but due to my bad code, here's a failsafe.
				if (data->firstSavedLevel == 0)
				{
					data->firstSavedLevel = savedP1Level > 1 ? savedP1Level - 1 : savedP1Level;
					logger::debug("[GLOB] TriggerAVAutoScaling: First saved level for {} is 0. Set to {} now.", 
						a_playerActor->GetName(), data->firstSavedLevel);
				}

				// NOTE: First saved level is guaranteed to be >= 1 here.
				if (data->firstSavedLevel < savedP1Level)
				{
					// P1's level is >= 2.

					logger::debug("[GLOB] TriggerAVAutoScaling: Dip to update base skill AVs for {}. Assign to dipped-level base AVs list. Current level: {}, level to dip to: {}.",
						a_playerActor->GetName(), savedP1Level, data->firstSavedLevel);

					float savedXP = p1->skills->data->xp;
					p1->skills->data->xp = 0.0f;
					// Scale down.
					script->SetCommand("SetLevel " + std::to_string(data->firstSavedLevel));
					script->CompileAndRun(p1);

					// KNOWN MAJOR ISSUE: The world sometimes does not scale down to the dip level in time
					// before the new base skill AV levels are set below,
					// meaning the current level's skill AVs will be set as the base skill AV levels instead.
					// This means that the skill level increments will not stack properly.
					// Temporary workaround which has its own issues: do not update the base skill levels if this occurs.
					if (auto newLevel = p1->GetLevel(); newLevel != data->firstSavedLevel)
					{
						logger::error("[GLOB] ERR: TriggerAVAutoScaling: Dip level ({}) not reached before setting new base skill actor values for {}. Current level is {}. Not setting base skill actor values this time.",
							data->firstSavedLevel, a_playerActor->GetName(), newLevel);
					}
					else
					{
						// Update base skill AVs.
						logger::debug("[GLOB] TriggerAVAutoScaling: Dip level ({}) reached. Setting new base skill actor values for {}.",
							data->firstSavedLevel, a_playerActor->GetName());
						data->skillBaseLevelsList = Util::GetActorSkillAVs(a_playerActor);
					}

					logger::debug("[GLOB] TriggerAVAutoScaling: Update base skill AVs for {}. After dip: current XP, threshold: {}, {}, current player levels: {}, {}, target level: {}.",
						a_playerActor->GetName(), p1->skills->data->xp, p1->skills->data->levelThreshold, p1->GetLevel(), a_playerActor->GetLevel(), data->firstSavedLevel);
					// Restore original P1 level.
					script->SetCommand("SetLevel " + std::to_string(savedP1Level));
					script->CompileAndRun(p1);
					// Restore XP.
					p1->skills->data->xp = savedXP;

					logger::debug("[GLOB] TriggerAVAutoScaling: Update base skill AVs for {}. After dip: current XP, threshold: {}, {}, current player levels: {}, {}.",
						a_playerActor->GetName(), p1->skills->data->xp, p1->skills->data->levelThreshold, p1->GetLevel(), a_playerActor->GetLevel());
				}
				else if (data->firstSavedLevel == savedP1Level)
				{
					// Player is at the same level as P1.

					// Scale up/down only one level to trigger auto-scaling.
					uint16_t targetLevel = savedP1Level < UINT16_MAX ? savedP1Level + 1 : savedP1Level - 1;
					logger::debug("[GLOB] TriggerAVAutoScaling: {}: Assign to current skill AVs after returning to saved P1 level: {}, first saved level: {}, target level: {}.",
						a_playerActor->GetName(), savedP1Level, data->firstSavedLevel, targetLevel);

					float savedXP = p1->skills->data->xp;
					p1->skills->data->xp = 0.0f;
					script->SetCommand("SetLevel " + std::to_string(targetLevel));
					script->CompileAndRun(p1);
					logger::debug("[GLOB] TriggerAVAutoScaling: Update base skill AVs for {}. Before inc/dec: current XP, threshold: {}, {}, current level: {}, target level: {}.",
						a_playerActor->GetName(), p1->skills->data->xp, p1->skills->data->levelThreshold, p1->GetLevel(), targetLevel);
					// Scale back up.
					script->SetCommand("SetLevel " + std::to_string(savedP1Level));
					script->CompileAndRun(p1);
					// Restore XP.
					p1->skills->data->xp = savedXP;

					logger::debug("[GLOB] TriggerAVAutoScaling: Update base skill AVs for {}. After inc/dec: current XP, threshold: {}, {}, current level: {}.",
						a_playerActor->GetName(), p1->skills->data->xp, p1->skills->data->levelThreshold, p1->GetLevel());

					// KNOWN MAJOR ISSUE: The world sometimes does not scale up to the original level in time
					// before the new base skill AV levels are set below,
					// meaning the dip level's skill AVs will be set as the base skill AV levels instead.
					// Temporary workaround which has its own issues: do not update the base skill levels if this occurs.
					if (auto newLevel = p1->GetLevel(); newLevel != savedP1Level)
					{
						logger::error("[GLOB] ERR: TriggerAVAutoScaling: Original P1 level ({}) not reached before setting new base skill actor values for {}. Current level is {}. Not setting base skill actor values this time.",
							savedP1Level, a_playerActor->GetName(), newLevel);
					}
					else
					{
						logger::debug("[GLOB] TriggerAVAutoScaling: Original level ({}) reached. Setting base skill actor values for {}.",
							savedP1Level, a_playerActor->GetName());
						// Update base skill AVs at the current level.
						// Should only differ from pre-dip levels if the given player changed their class.
						data->skillBaseLevelsList = Util::GetActorSkillAVs(a_playerActor);
					}
				}
				else
				{
					logger::error("[GLOB] ERR: TriggerAVAutoScaling: P1's level ({}) is below the target dip level ({}) for {}. Do not change base skill AVs.",
						savedP1Level, data->firstSavedLevel, a_playerActor->GetName());
				}

				delete script;
				return true;
			};

			if (a_playerActor)
			{
				bool succ = autoScaleAndSetBaseAVs(a_playerActor);
				if (!succ)
				{
					logger::error("[GLOB] ERR: TriggerAVAutoScaling: Could not modify P1's level with console command. No rescaling possible.");
					return false;
				}

				// Reset all perks if the player's base AVs have changed.
				if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
				{
					ResetPerkDataOnBaseSkillAVChange(a_playerActor);
				}
			}
			else
			{
				for (const auto& p : glob.coopPlayers)
				{
					if (p->isActive && !p->isPlayer1)
					{
						autoScaleAndSetBaseAVs(p->coopActor.get());
					}
				}
			}

			return true;
		}

		
	}

	//===============================================================================================================================================

	void GlobalCoopData::ContactListener::ContactPointCallback(const RE::hkpContactPointEvent& a_event)
	{
		auto& glob = GetSingleton();
		if (glob.coopSessionActive) 
		{
			if (a_event.bodies[0] && a_event.bodies[1])
			{
				// Find collidable and handle for each colliding body.
				auto collidableA = a_event.bodies[0]->GetCollidable();
				auto collidableB = a_event.bodies[1]->GetCollidable();
				RE::ObjectRefHandle handleA{};
				RE::ObjectRefHandle handleB{};
				if (collidableA && collidableB)
				{
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
				}

				bool handlesValid = Util::HandleIsValid(handleA) && Util::HandleIsValid(handleB);
				bool oneRefrIsManaged = false;
				for (const auto& p : glob.coopPlayers)
				{
					if (p->isActive && p->IsRunning())
					{
						// Only one of the two colliding refrs is handled by a player's reference manipulation manager,
						// meaning it was dropped or thrown and hit another unmanaged object.
						oneRefrIsManaged = (p->tm->rmm->IsManaged(handleA, false) && !p->tm->rmm->IsManaged(handleB, false)) ||
										   (!p->tm->rmm->IsManaged(handleA, false) && p->tm->rmm->IsManaged(handleB, false));
						if (handlesValid && oneRefrIsManaged)
						{
							// Save the FIDs for the colliding refrs and queue this event for handling later by the player's reference manipulation manager.
							// Want to spend as little time in this callback as possible to prevent havok-related slowdowns.
							const auto fidPair = std::pair<RE::FormID, RE::FormID>(handleA.get().get()->formID, handleB.get().get()->formID);
							// REMOVE
							const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
							logger::debug("[GLOB] ContactPointCallback. {}: Lock: 0x{:X}.", p->coopActor->GetName(), hash);
							{
								std::unique_lock<std::mutex> lock(p->tm->rmm->contactEventsQueueMutex);
								logger::debug("[GLOB] ContactPointCallback. {}: Lock obtained: 0x{:X}.", p->coopActor->GetName(), hash);
								if (!p->tm->rmm->collidedRefrFIDPairs.contains(fidPair))
								{
									p->tm->rmm->collidedRefrFIDPairs.emplace(fidPair);
									p->tm->rmm->queuedReleasedRefrContactEvents.emplace_back(a_event);
								}
							}
						}
					}
				}
			}
		}
	}
}

#include "Serialization.h"
#include <Compatibility.h>
#include <GlobalCoopData.h>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	namespace SerializationCallbacks
	{	
		RE::TESForm* GetFormFromRetrievedFID(SKSE::SerializationInterface* a_intfc, RE::FormID& a_fid, RE::TESDataHandler* a_dataHandler)
		{
			// Get form from serialized FID.

			bool succ = a_intfc->ResolveFormID(a_fid, a_fid);
			if (a_fid != 0 && succ)
			{
				RE::TESForm* form = RE::TESForm::LookupByID(a_fid);

				// If not found in Skyrim.esm, look through all mod files for the form.
				if (!form && a_dataHandler)
				{
					for (auto file : a_dataHandler->files)
					{
						if (file)
						{
							const auto form = a_dataHandler->LookupForm(a_fid, file->fileName);
							if (form)
							{
								logger::debug("[SERIAL] GetFormFromRetrievedFID: Found form with FID 0x{:X}: {} in mod file {}.", 
									a_fid, form ? form->GetName() : "NONE",  file->fileName);
								return form;
							}
						}
					}
				}
				else
				{
					logger::debug("[SERIAL] GetFormFromRetrievedFID: Found form with FID 0x{:X}: {}.",
						a_fid, form ? form->GetName() : "NONE");
					return form;
				}
			}
			else if (!succ)
			{
				logger::error("[SERIAL] ERR: GetFormFromRetrievedFID: Could resolve new form ID from form ID (0x{:X}) retrieved from co-save.", a_fid);
				return nullptr;
			}

			return nullptr;
		}

		void Load(SKSE::SerializationInterface* a_intfc)
		{
			// Load all serialized data into our global serialized data structure.

			logger::debug("[SERIAL] Load: Read all serialized data from SKSE co-save.");
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[SERIAL] Load: Lock: 0x{:X}.", hash);
			{
				std::unique_lock<std::mutex> lock(serializationMutex);
				logger::debug("[SERIAL] Load: Lock obtained: 0x{:X}.", hash);

				// If the serialization interface is not valid, set the default retrieved data.
				if (!a_intfc)
				{
					logger::error("[SERIAL] ERR: Load: Could not get serialization interface. Setting default data for all players.");
					SetDefaultRetrievedData(a_intfc);
				}

				// Clear out current data before reading in new data.
				if (!glob.serializablePlayerData.empty())
				{
					glob.serializablePlayerData.clear();
				}

				RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();
				// Player form ID serialized as a key for each grouping of data.
				RE::FormID fid = 0;

				// Type, serialization version, and length of record data read out.
				uint32_t type;
				uint32_t version;
				uint32_t length;
				// Read each record which contains data of the read-out type for each player.
				while (a_intfc->GetNextRecordInfo(type, version, length))
				{
					if (version != !SerializableDataType::kSerializationVersion)
					{
						logger::error("[SERIAL] ERR: Load: Serialized data version ({}) does not match current version ({}) for entry of type {}, length {}.",
							version, !SerializableDataType::kSerializationVersion, TypeToString(type), length);
						continue;
					}

					// Per-player data.
					if (type == !SerializableDataType::kPlayerAvailablePerkPoints ||
						type == !SerializableDataType::kPlayerBaseHMSPointsList ||
						type == !SerializableDataType::kPlayerBaseSkillLevelsList ||
						type == !SerializableDataType::kPlayerCopiedMagicList ||
						type == !SerializableDataType::kPlayerEmoteIdleEvents ||
						type == !SerializableDataType::kPlayerEquippedObjectsList ||
						type == !SerializableDataType::kPlayerExtraPerkPoints ||
						type == !SerializableDataType::kPlayerFirstSavedLevel ||
						type == !SerializableDataType::kPlayerHMSPointsIncList ||
						type == !SerializableDataType::kPlayerLevel ||
						type == !SerializableDataType::kPlayerLevelXP ||
						type == !SerializableDataType::kPlayerSharedPerksUnlocked ||
						type == !SerializableDataType::kPlayerSkillIncreasesList ||
						type == !SerializableDataType::kPlayerSkillXPList ||
						type == !SerializableDataType::kPlayerUnlockedPerksList ||
						type == !SerializableDataType::kPlayerUsedPerkPoints)

					{
						for (auto i = 0; i < ALYSLC_MAX_PLAYER_COUNT; ++i)
						{
							if (!a_intfc->ReadRecordData(fid) || !a_intfc->ResolveFormID(fid, fid))
							{
								logger::error("[SERIAL] ERR: Load: Could not read player form ID ({}, 0x{:X}) or resolve form ID ({}) for record of type {}, version {}, length {} when attempting to load serialized data. Continuing to next player record of the same type.",
									!a_intfc->ReadRecordData(fid), fid, !a_intfc->ResolveFormID(fid, fid), TypeToString(type), version, length);
								continue;
							}
							else
							{
								logger::debug("[SERIAL] Load: About to retrieve serialized data for player with FID 0x{:X}.", fid);
								// Insert default data at first.
								if (glob.serializablePlayerData.empty() || !glob.serializablePlayerData.contains(fid))
								{
									float lvlXP = 0;
									uint32_t firstSavedLvl = 0.0f;
									uint32_t availablePerkPoints = 0;
									uint32_t lvl = 1;
									uint32_t sharedPerksUnlocked = 0;
									uint32_t usedPerkPoints = 0;
									uint32_t extraPerkPoints = 0;
									std::array<float, 3> baseHMSPoints{ 100.0f, 100.0f, 100.0f };
									std::array<float, 3> hmsPointIncreases{ 0.0f, 0.0f, 0.0f };
									SkillList skillList{ 0.0f };
									auto& baseSkillLevels = skillList;
									auto& skillXP = skillList;
									auto& skillIncreases = skillList;
									std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal> copiedMagic;
									copiedMagic.fill(nullptr);
									std::array<RE::BSFixedString, 8> cyclableEmoteIdleEvents = GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS;
									std::vector<RE::TESForm*> equippedForms{ !EquipIndex::kTotal, nullptr };
									std::vector<RE::BGSPerk*> unlockedPerksList;

									logger::debug("[SERIAL] Load: Added new serialized data set for player with FID 0x{:X}.", fid);
									glob.serializablePlayerData.insert
									(
										{ 
											fid,
											std::make_unique<ALYSLC::GlobalCoopData::SerializablePlayerData>
											(
												copiedMagic,
												cyclableEmoteIdleEvents,
												equippedForms,
												lvlXP,
												availablePerkPoints,
												firstSavedLvl,
												lvl,
												sharedPerksUnlocked,
												usedPerkPoints,
												extraPerkPoints,
												baseHMSPoints,
												hmsPointIncreases,
												baseSkillLevels,
												skillIncreases,
												skillXP,
												unlockedPerksList
											)
										}
									);
								}

								const auto& data = glob.serializablePlayerData.at(fid);
								if (type == !SerializableDataType::kPlayerAvailablePerkPoints)
								{
									RetrieveUInt32Data(a_intfc, data->availablePerkPoints, type);
									logger::debug("[SERIAL] Load: Player with FID 0x{:X} has {} available perk points to use still.",
										fid, data->availablePerkPoints);
								}
								else if (type == !SerializableDataType::kPlayerUsedPerkPoints)
								{
									RetrieveUInt32Data(a_intfc, data->usedPerkPoints, type);
									logger::debug("[SERIAL] Load: Player with FID 0x{:X} has used {} perk points.",
										fid, data->usedPerkPoints);
								}
								else if (type == !SerializableDataType::kPlayerSharedPerksUnlocked)
								{
									RetrieveUInt32Data(a_intfc, data->sharedPerksUnlocked, type);
									logger::debug("[SERIAL] Load: Player with FID 0x{:X} has unlocked {} shared perks.",
										fid, data->sharedPerksUnlocked);
								}
								else if (type == !SerializableDataType::kPlayerExtraPerkPoints)
								{
									RetrieveUInt32Data(a_intfc, data->extraPerkPoints, type);
									logger::debug("[SERIAL] Load: Player with FID 0x{:X} has {} extra perks.",
										fid, data->extraPerkPoints);
								}
								else if (type == !SerializableDataType::kPlayerFirstSavedLevel)
								{
									RetrieveUInt32Data(a_intfc, data->firstSavedLevel, type);
									logger::debug("[SERIAL] Load: Player with FID 0x{:X} was level {} when their data was first serialized.",
										fid, data->firstSavedLevel);
								}
								else if (type == !SerializableDataType::kPlayerLevel)
								{
									RetrieveUInt32Data(a_intfc, data->level, type);
									logger::debug("[SERIAL] Load: Player with FID 0x{:X} has saved level: {}.",
										fid, data->level);
								}
								else if (type == !SerializableDataType::kPlayerLevelXP)
								{
									RetrieveFloatData(a_intfc, data->levelXP, type);
									logger::debug("[SERIAL] Load: Player with FID 0x{:X} has saved level XP: {}.",
										fid, data->levelXP);
								}
								else if (type == !SerializableDataType::kPlayerBaseHMSPointsList)
								{
									for (auto j = 0; j < 3; ++j)
									{
										RetrieveFloatData(a_intfc, data->hmsBasePointsList[j], type);
										logger::debug("[SERIAL] Load: Player with FID 0x{:X} has a base {} level of {} at first-saved level {}.",
											fid, j == 0 ? "health" : j == 1 ? "magicka" : "stamina",
											data->hmsBasePointsList[j],
											data->firstSavedLevel);
									}
								}
								else if (type == !SerializableDataType::kPlayerHMSPointsIncList)
								{
									for (auto j = 0; j < 3; ++j)
									{
										RetrieveFloatData(a_intfc, data->hmsPointIncreasesList[j], type);
										logger::debug("[SERIAL] Load: Player with FID 0x{:X} has increased {} by {}.",
											fid, j == 0 ? "Health" : j == 1 ? "Magicka" : "Stamina",
											data->hmsPointIncreasesList[j]);
									}
								}
								else if (type == !SerializableDataType::kPlayerBaseSkillLevelsList)
								{
									for (auto j = 0; j < Skill::kTotal; ++j)
									{
										RetrieveFloatData(a_intfc, data->skillBaseLevelsList[j], type);
										logger::debug("[SERIAL] Load: Player with FID 0x{:X} has base skill {} of {}.",
											fid, std::format("{}", glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))), 
											data->skillBaseLevelsList[j]);
									}
								}
								else if (type == !SerializableDataType::kPlayerSkillIncreasesList)
								{
									for (auto j = 0; j < Skill::kTotal; ++j)
									{
										RetrieveFloatData(a_intfc, data->skillLevelIncreasesList[j], type);
										logger::debug("[SERIAL] Load: Player with FID 0x{:X} has increased skill {} by {}.",
											fid, std::format("{}", glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))), 
											data->skillLevelIncreasesList[j]);
									}
								}
								else if (type == !SerializableDataType::kPlayerSkillXPList)
								{
									for (auto j = 0; j < Skill::kTotal; ++j)
									{
										RetrieveFloatData(a_intfc, data->skillXPList[j], type);
										logger::debug("[SERIAL] Load: Player with FID 0x{:X} has gained {} XP for skill {}.",
											fid, data->skillXPList[j], glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j)));
									}
								}
								else if (type == !SerializableDataType::kPlayerEquippedObjectsList)
								{
									// Read in all saved equipped forms.
									uint32_t numEquippedForms = 0;
									RetrieveUInt32Data(a_intfc, numEquippedForms, type);
									logger::debug("[SERIAL] Load: Player with FID 0x{:X} has {} equipped forms.", fid, numEquippedForms);

									data->equippedForms.clear();
									RE::FormID equippedFID = 0;
									for (auto j = 0; j < numEquippedForms; ++j)
									{
										RetrieveUInt32Data(a_intfc, equippedFID, type);
										data->equippedForms.emplace_back(GetFormFromRetrievedFID(a_intfc, equippedFID, dataHandler));
									}
								}
								else if (type == !SerializableDataType::kPlayerCopiedMagicList)
								{
									// Read in saved copied magic spell forms.
									data->copiedMagic.fill(nullptr);
									RE::FormID magicFID{};
									for (uint8_t i = 0; i < !PlaceholderMagicIndex::kTotal; ++i) 
									{
										RetrieveUInt32Data(a_intfc, magicFID, type);
										if (auto form = GetFormFromRetrievedFID(a_intfc, magicFID, dataHandler); form && form->Is(RE::FormType::Spell, RE::FormType::Shout))
										{
											data->copiedMagic[i] = form;
										}
										else
										{
											data->copiedMagic[i] = nullptr;
										}

										logger::debug("[SERIAL] Load: Player with FID 0x{:X} has copied magic form {} (0x{:X}) in slot {}.",
											fid, 
											data->copiedMagic[i] ? data->copiedMagic[i]->GetName() : "NONE",
											data->copiedMagic[i] ? data->copiedMagic[i]->formID : 0xDEAD,
											i);
									}
								}
								else if (type == !SerializableDataType::kPlayerEmoteIdleEvents)
								{
									// Eight cyclable emote idles.
									RE::BSFixedString eventName = ""sv;
									uint32_t size = 0;
									for (uint8_t i = 0; i < data->cyclableEmoteIdleEvents.size(); ++i) 
									{
										RetrieveUInt32Data(a_intfc, size, type);
										if (size > 0) 
										{
											RetrieveStringData(a_intfc, eventName, type, size);
											data->cyclableEmoteIdleEvents[i] = eventName;
											logger::debug("[SERIAL] Load: Player with FID 0x{:X}'s saved cyclable emote idle event {} is {}. String length: {}.", 
												fid, i, eventName, size);
										}
									}
								}
								else if (type == !SerializableDataType::kPlayerUnlockedPerksList)
								{
									// Read in all unlocked perks.
									uint32_t numUnlockedPerks = 0;
									RetrieveUInt32Data(a_intfc, numUnlockedPerks, type);
									logger::debug("[SERIAL] Load: Player with FID 0x{:X} has {} unlocked perks.",
										fid, numUnlockedPerks);

									data->ClearUnlockedPerks();
									RE::TESForm* perkForm = nullptr;
									RE::BGSPerk* perk = nullptr;
									RE::FormID perkFID = 0;
									for (auto j = 0; j < numUnlockedPerks; ++j)
									{
										RetrieveUInt32Data(a_intfc, perkFID, type);
										perkForm = GetFormFromRetrievedFID(a_intfc, perkFID, dataHandler);
										perk = perkForm ? perkForm->As<RE::BGSPerk>() : nullptr;
										data->InsertUnlockedPerk(perk);
									}
								}
								else
								{
									logger::error("[SERIAL] ERR: Load: WHAT?! The mod author must've been careless again. Oh boy. No such serialized data type '{}'.", TypeToString(type));
								}
							}
						}
					}
				}

				// Set default data if not all data was retrieved successfully.
				if (glob.serializablePlayerData.size() == 0)
				{
					logger::debug("[SERIAL] Load: First time retrieval. Setting default data for all players.");
					SetDefaultRetrievedData(a_intfc);
				}
				else
				{
					logger::debug("[SERIAL] Load: Successfully retrieved serialized data for {} players from SKSE co-save.",
						ALYSLC_MAX_PLAYER_COUNT);
				}

				// NOTE: The game fails to save P1's perks properly at times,
				// either clearing all of them, or only saving the perks unlocked by P1 and not by any other player.
				// I have yet to find a reason why it does this or find a direct solution,
				// so the current workaround is to import P1's perks
				// to ensure that they can access their saved perks, even outside of co-op.
				// Please note that if the mod is uninstalled, P1 will have to respec all their perks manually,
				// as the function below will not fire to import all the saved perks.
				logger::debug("[SERIAL] LOAD: Import saved perks for P1.");
				GlobalCoopData::ImportUnlockedPerks(RE::PlayerCharacter::GetSingleton());
			}
		}

		void RetrieveFloatData(SKSE::SerializationInterface* a_intfc, float& a_data, const uint32_t& a_recordType)
		{
			// Attempt to read a float value.

			if (!a_intfc->ReadRecordData(a_data))
			{
				logger::error("[SERIAL] ERR: RetrieveFloatData: Could not read FLOAT record data ({}), record type: {}.",
					a_data, TypeToString(a_recordType));
			}
		}

		void RetrieveStringData(SKSE::SerializationInterface* a_intfc, RE::BSFixedString& a_data, const uint32_t& a_recordType, const uint32_t& a_size)
		{
			// Attempt to read char buffer data into fixed string.

			// Reserve a string of the serialized size (includes null terminator in size).
			std::string buff;
			buff.reserve(a_size);
			if (!a_intfc->ReadRecordData(buff.data(), a_size))
			{
				logger::error("[SERIAL] ERR: RetrieveStringData: Could not read STRING record data ({}, given size {}), record type: {}.",
					a_data, a_size, TypeToString(a_recordType));
			}
			else
			{
				// Assign the underlying buffer since direct assignment 
				// to the string does not seem to work.
				a_data = buff.data();
			}
		}

		void RetrieveUInt8Data(SKSE::SerializationInterface* a_intfc, uint8_t& a_data, const uint32_t& a_recordType)
		{
			// Attempt to read an unsigned 8-bit integer.

			if (!a_intfc->ReadRecordData(a_data))
			{
				logger::error("[SERIAL] ERR: RetrieveUInt8Data: Could not read UINT8 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType));
			}
		}

		void RetrieveUInt16Data(SKSE::SerializationInterface* a_intfc, uint16_t& a_data, const uint32_t& a_recordType)
		{
			// Attempt to read an unsigned 16-bit integer.

			if (!a_intfc->ReadRecordData(a_data))
			{
				logger::error("[SERIAL] ERR: RetrieveUInt16Data: Could not read UINT16 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType));
			}
		}

		void RetrieveUInt32Data(SKSE::SerializationInterface* a_intfc, uint32_t& a_data, const uint32_t& a_recordType)
		{
			// Attempt to read an unsigned 32-bit integer.

			if (!a_intfc->ReadRecordData(a_data))
			{
				logger::error("[SERIAL] ERR: RetrieveUInt32Data: Could not read UINT32 record data (0x{:X}), record type: {}.",
					a_data,
					TypeToString(a_recordType));
			}
		}

		void Revert(SKSE::SerializationInterface* a_intfc)
		{
			// Ensure no co-op session is active as the game reverts.

			logger::debug("[SERIAL] Revert: Stopping active co-op session.");
			if (glob.globalDataInit && glob.allPlayersInit) 
			{
				GlobalCoopData::TeardownCoopSession(true);
			}
		}

		void Save(SKSE::SerializationInterface* a_intfc)
		{
			// Save all our co-op serializable data to the SKSE co-save.
			logger::debug("[SERIAL] Save: Writing all serializable data to SKSE co-save.");
			if (!a_intfc)
			{
				logger::error("[SERIAL] ERR: Save: Could not get serialization interface ({}), co-op session active ({}). Cannot serialize co-op data.",
					(bool)!a_intfc, glob.coopSessionActive);
				return;
			}
			
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[SERIAL] Save: Lock: 0x{:X}.", hash);
			{
				std::unique_lock<std::mutex> lock(serializationMutex);
				logger::debug("[SERIAL] Save: Lock obtained: 0x{:X}.", hash);

				//=========================================
				// Each player's saved data.
				//=========================================

				// NOTE: Capitalized data types are for easier recognition by my monkey brain.

				// AVAILABLE PERK POINTS
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerAvailablePerkPoints, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerAvailablePerkPoints));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						logger::debug("[SERIAL] Save: Serialize AVAIL PERK POINTS for player with FID 0x{:X}: {}.", fid, data->availablePerkPoints);
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerAvailablePerkPoints);
						SerializePlayerUInt32Data(a_intfc, data->availablePerkPoints, !SerializableDataType::kPlayerAvailablePerkPoints);
					}
				}

				// USED PERK POINTS
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerUsedPerkPoints, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerUsedPerkPoints));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						logger::debug("[SERIAL] Save: Serialize USED PERK POINTS for player with FID 0x{:X}: {}.", fid, data->usedPerkPoints);
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerUsedPerkPoints);
						SerializePlayerUInt32Data(a_intfc, data->usedPerkPoints, !SerializableDataType::kPlayerUsedPerkPoints);
					}
				}

				// SHARED PERKS UNLOCKED
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerSharedPerksUnlocked, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerSharedPerksUnlocked));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						logger::debug("[SERIAL] Save: Serialize SHARED PERKS UNLOCKED for player with FID 0x{:X}: {}.", fid, data->sharedPerksUnlocked);
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerSharedPerksUnlocked);
						SerializePlayerUInt32Data(a_intfc, data->sharedPerksUnlocked, !SerializableDataType::kPlayerSharedPerksUnlocked);
					}
				}

				// EXTRA PERK POINTS
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerExtraPerkPoints, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerExtraPerkPoints));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						logger::debug("[SERIAL] Save: Serialize EXTRA PERK POINTS for player with FID 0x{:X}: {}.", fid, data->extraPerkPoints);
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerExtraPerkPoints);
						SerializePlayerUInt32Data(a_intfc, data->extraPerkPoints, !SerializableDataType::kPlayerExtraPerkPoints);
					}
				}

				// BASE LVL
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerFirstSavedLevel, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerFirstSavedLevel));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						logger::debug("[SERIAL] Save: Serialize BASE LVL for player with FID 0x{:X}: {}.", fid, data->firstSavedLevel);
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerFirstSavedLevel);
						SerializePlayerUInt32Data(a_intfc, data->firstSavedLevel, !SerializableDataType::kPlayerFirstSavedLevel);
					}
				}

				// LVL
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerLevel, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerLevel));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						logger::debug("[SERIAL] Save: Serialize LVL for player with FID 0x{:X}: {}.", fid, data->level);
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerLevel);
						SerializePlayerUInt32Data(a_intfc, data->level, !SerializableDataType::kPlayerLevel);
					}
				}

				// LVL XP
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerLevelXP, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerLevelXP));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						logger::debug("[SERIAL] Save: Serialize LVL XP for player with FID 0x{:X}: {}.", fid, data->levelXP);
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerLevelXP);
						SerializePlayerFloatData(a_intfc, data->levelXP, !SerializableDataType::kPlayerLevelXP);
					}
				}

				// HMS BASE POINTS LIST
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerBaseHMSPointsList, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerBaseHMSPointsList));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerBaseHMSPointsList);
						uint32_t numEntries = data->hmsBasePointsList.size();
						if (numEntries > 0)
						{
							for (uint8_t j = 0; j < numEntries; ++j)
							{
								logger::debug("[SERIAL] Save: Serialize HMS BASE POINTS LIST for player with FID 0x{:X}: Base {} is {}.", 
									fid, j == 0 ? "health" : j == 1 ? "magicka" : "stamina", data->hmsBasePointsList[j]);
								SerializePlayerFloatData(a_intfc, data->hmsBasePointsList[j], !SerializableDataType::kPlayerBaseHMSPointsList);
							}
						}
						else
						{
							logger::error("[SERIAL] ERR: Save: HMS BASE POINTS LIST is empty for player with FID 0x{:X}. Writing 100 as base value for health, magicka, and stamina.", fid);
							for (uint8_t j = 0; j < 3; ++j)
							{
								SerializePlayerFloatData(a_intfc, 100.0f, !SerializableDataType::kPlayerBaseHMSPointsList);
							}
						}
					}
				}

				// HMS INC LIST
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerHMSPointsIncList, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerHMSPointsIncList));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerHMSPointsIncList);
						uint32_t numEntries = data->hmsPointIncreasesList.size();
						if (numEntries > 0)
						{
							for (uint8_t j = 0; j < numEntries; ++j)
							{
								logger::debug("[SERIAL] Save: Serialize HMS INC LIST for player with FID 0x{:X}: {} increment is {}.", 
									fid, j == 0 ? "Health" : j == 1 ? "Magicka" : "Stamina", data->hmsPointIncreasesList[j]);
								SerializePlayerFloatData(a_intfc, data->hmsPointIncreasesList[j], !SerializableDataType::kPlayerHMSPointsIncList);
							}
						}
						else
						{
							logger::error("[SERIAL] ERR: Save: HMS INC LIST is empty for player with FID 0x{:X}. Writing 0 point increases for health, magicka, and stamina.", fid);
							for (uint8_t j = 0; j < 3; ++j)
							{
								SerializePlayerFloatData(a_intfc, 0.0f, !SerializableDataType::kPlayerHMSPointsIncList);
							}
						}
					}
				}

				// BASE SKILL LVL LIST
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerBaseSkillLevelsList, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerBaseSkillLevelsList));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerBaseSkillLevelsList);
						// Do not need to write the number of skill XP entries first, since this corresponds to the
						// number of skill schools and is constant from save to save.
						uint32_t numSkillLvlEntries = data->skillBaseLevelsList.size();
						if (numSkillLvlEntries > 0)
						{
							for (uint8_t j = 0; j < numSkillLvlEntries; ++j)
							{
								logger::debug("[SERIAL] Save: Serialize BASE SKILL LVL LIST for player with FID 0x{:X}: Skill {} has base value {}.", 
									fid, std::format("{}", glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))), data->skillBaseLevelsList[j]);
								SerializePlayerFloatData(a_intfc, data->skillBaseLevelsList[j], !SerializableDataType::kPlayerBaseSkillLevelsList);
							}
						}
						else
						{
							logger::error("[SERIAL] ERR: Save: BASE SKILL LVL LIST list is empty for player 0x{:X}. Getting and setting each skill again for the player.", fid);
							if (const auto playerActor = RE::TESForm::LookupByID(fid); playerActor && playerActor->As<RE::Actor>())
							{
								auto skillBaseList = Util::GetActorSkillAVs(playerActor->As<RE::Actor>());
								uint8_t numSkillLvlEntries = skillBaseList.size();
								for (uint8_t j = 0; j < numSkillLvlEntries; ++j)
								{
									logger::debug("[SERIAL] Save: Serialize BASE SKILL LVL LIST for player with FID 0x{:X}: Skill {} has base value {}.",
										fid, std::format("{}", glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))), skillBaseList[j]);
									SerializePlayerFloatData(a_intfc, skillBaseList[j], !SerializableDataType::kPlayerBaseSkillLevelsList);
								}
							}
							else
							{
								logger::error("[SERIAL] ERR: Save: Could not get player actor for FID 0x{:X}. Setting each skill level to a base level of 15 for the player.", fid);
								uint8_t numSkillLvlEntries = Skill::kTotal;
								for (uint8_t j = 0; j < numSkillLvlEntries; ++j)
								{
									SerializePlayerFloatData(a_intfc, 15.0f, !SerializableDataType::kPlayerBaseSkillLevelsList);
								}
							}
						}
					}
				}

				// SKILL LVL INC LIST
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerSkillIncreasesList, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerSkillIncreasesList));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerSkillIncreasesList);
						// Do not need to write the number of skill XP entries first, since this corresponds to the
						// number of skill schools and is constant from save to save.
						uint32_t numSkillIncEntries = data->skillLevelIncreasesList.size();
						if (numSkillIncEntries > 0)
						{
							for (uint8_t j = 0; j < numSkillIncEntries; ++j)
							{
								logger::debug("[SERIAL] Save: Serialize SKILL LVL INC LIST for player with FID 0x{:X}: Skill {} has increment {}.",
									fid, std::format("{}", glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))), data->skillLevelIncreasesList[j]);
								SerializePlayerFloatData(a_intfc, data->skillLevelIncreasesList[j], !SerializableDataType::kPlayerSkillIncreasesList);
							}
						}
						else
						{
							logger::error("[SERIAL] ERR: Save: SKILL LVL INC LIST is empty for player 0x{:X}. Writing 0 level increase for each skill.", fid);
							for (uint8_t j = 0; j < numSkillIncEntries; ++j)
							{
								SerializePlayerFloatData(a_intfc, 0.0f, !SerializableDataType::kPlayerSkillIncreasesList);
							}

							if (const auto playerActor = RE::TESForm::LookupByID(fid); playerActor)
							{
								logger::error("[SERIAL] ERR: Save: Re-serializing BASE LVL as 0 for player with FID 0x{:X} to force auto-scaling for all skill AVs until next level up during a co-op session.", fid);
								if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerFirstSavedLevel, !SerializableDataType::kSerializationVersion))
								{
									logger::error("[SERIAL] ERR: Save: Could not open record of type {}.",
										TypeToString(!SerializableDataType::kPlayerFirstSavedLevel));
								}
								else
								{
									for (auto& [fid2, data2] : glob.serializablePlayerData)
									{
										if (fid2 != fid)
										{
											logger::debug("[SERIAL] Save: Re-serialize BASE LVL for player with FID 0x{:X}: {}.", fid2, data2->firstSavedLevel);
											SerializePlayerUInt32Data(a_intfc, fid2, !SerializableDataType::kPlayerFirstSavedLevel);
											SerializePlayerUInt32Data(a_intfc, data2->firstSavedLevel, !SerializableDataType::kPlayerFirstSavedLevel);
											continue;
										}

										logger::debug("[SERIAL] Save: Serialize BASE LVL for player with FID 0x{:X} as 0 to force AV auto-scaling.", fid2);
										SerializePlayerUInt32Data(a_intfc, fid2, !SerializableDataType::kPlayerFirstSavedLevel);
										SerializePlayerUInt32Data(a_intfc, 0, !SerializableDataType::kPlayerFirstSavedLevel);
									}
								}
							}
						}
					}
				}

				// SKILL XP LIST
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerSkillXPList, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerSkillXPList));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerSkillXPList);
						// Do not need to write the number of skill XP entries first, since this corresponds to the
						// number of skill schools and is constant from save to save.
						uint32_t numSkillXPEntries = data->skillXPList.size();
						if (numSkillXPEntries > 0)
						{
							for (uint8_t j = 0; j < numSkillXPEntries; ++j)
							{
								logger::debug("[SERIAL] Save: Serialize SKILL XP LIST for player with FID 0x{:X}: Skill {} has {} XP.",
									fid, std::format("{}", glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))), data->skillXPList[j]);
								SerializePlayerFloatData(a_intfc, data->skillXPList[j], !SerializableDataType::kPlayerSkillXPList);
							}
						}
						else
						{
							logger::error("[SERIAL] ERR: Save: SKILL XP LIST is empty for player with FID 0x{:X}. Writing 0 XP for each skill, which will reset skill XP levels to P1's corresponding skill XPs on load.", fid);
							for (uint8_t j = 0; j < numSkillXPEntries; ++j)
							{
								SerializePlayerFloatData(a_intfc, 0.0f, !SerializableDataType::kPlayerSkillXPList);
							}
						}
					}
				}

				// EQUIPPED OBJECTS
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerEquippedObjectsList, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerEquippedObjectsList));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerEquippedObjectsList);
						// Write number of equipped objects first, as this varies from save to save.
						uint32_t numEquippedForms = data->equippedForms.size();
						SerializePlayerUInt32Data(a_intfc, numEquippedForms, !SerializableDataType::kPlayerEquippedObjectsList);
						logger::debug("[SERIAL] Save: Serialize EQUIPPED OBJECTS LIST for 0x{:X}. Num equipped forms: {}.", fid, numEquippedForms);

						// Write each equipped form's FID next.
						if (numEquippedForms > 0)
						{
							for (uint16_t j = 0; j < numEquippedForms; ++j)
							{
								const auto form = data->equippedForms[j];
								if (form)
								{
									logger::debug("[SERIAL] Save: Serialize EQUIPPED OBJECT {} (0x{:X}) for player with FID 0x{:X}.",
										form->GetName(), form->formID, fid);
									SerializePlayerUInt32Data(a_intfc, form->formID, !SerializableDataType::kPlayerEquippedObjectsList);
								}
								else
								{
									// Empty slot, write 0 for form ID.
									SerializePlayerUInt32Data(a_intfc, 0, !SerializableDataType::kPlayerEquippedObjectsList);
								}
							}
						}
					}
				}

				// COPIED SPELLS PAIR
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerCopiedMagicList, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerCopiedMagicList));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerCopiedMagicList);
						RE::FormID magicFID{};
						const auto& copiedMagic = data->copiedMagic;
						for (uint8_t i = 0; i < !PlaceholderMagicIndex::kTotal; ++i)
						{
							magicFID = copiedMagic[i] && copiedMagic[i]->Is(RE::FormType::Spell, RE::FormType::Shout) ? copiedMagic[i]->formID : 0;
							SerializePlayerUInt32Data(a_intfc, magicFID, !SerializableDataType::kPlayerCopiedMagicList);
							logger::debug("[SERIAL] Save: Player with FID 0x{:X} has COPIED SPELL form {} (0x{:X}) in slot {}.",
								fid,
								copiedMagic[i] ? copiedMagic[i]->GetName() : "NONE",
								copiedMagic[i] ? copiedMagic[i]->formID : 0xDEAD,
								i);
						}
					}
				}

				// CYCLABLE EMOTE IDLE EVENTS LIST
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerEmoteIdleEvents, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerEmoteIdleEvents));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerEmoteIdleEvents);
						const auto& cyclableEmoteIdleEvents = data->cyclableEmoteIdleEvents;
						uint32_t size = 0;
						for (uint8_t i = 0; i < cyclableEmoteIdleEvents.size(); ++i)
						{
							// Serialize string length and then the string's buffer for each saved emote idle event name.
							// +1 for the null terminator at the end of the string.
							size = cyclableEmoteIdleEvents[i].length() * sizeof(char) + 1;
							SerializePlayerUInt32Data(a_intfc, size, !SerializableDataType::kPlayerEmoteIdleEvents);
							SerializePlayerStringData(a_intfc, cyclableEmoteIdleEvents[i], !SerializableDataType::kPlayerEmoteIdleEvents);
							logger::debug("[SERIAL] Save: Player with FID 0x{:X} has cyclable emote idle event {} (size {}) in slot {}.",
								fid, cyclableEmoteIdleEvents[i], size, i);
						}
					}
				}

				// UNLOCKED PERKS LIST
				if (!a_intfc->OpenRecord(!SerializableDataType::kPlayerUnlockedPerksList, !SerializableDataType::kSerializationVersion))
				{
					logger::error("[SERIAL] ERR: Save: Could open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerUnlockedPerksList));
				}
				else
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data(a_intfc, fid, !SerializableDataType::kPlayerUnlockedPerksList);
						// Write number of unlocked perks first, as this varies from save to save.
						const auto& unlockedPerksList = data->GetUnlockedPerksList();
						uint32_t numUnlockedPerks = unlockedPerksList.size();
						logger::debug("[SERIAL] Save: Serialize UNLOCKED PERKS LIST for 0x{:X}. Number of unlocked perks: {}.", fid, numUnlockedPerks);
						SerializePlayerUInt32Data(a_intfc, numUnlockedPerks, !SerializableDataType::kPlayerUnlockedPerksList);

						// Serialize the FID of each unlocked perk next.
						if (numUnlockedPerks > 0)
						{
							for (uint8_t j = 0; j < numUnlockedPerks; ++j)
							{
								logger::debug("[SERIAL] Save: Serialize UNLOCKED PERK {} (0x{:X}) for player with FID 0x{:X}.",
									unlockedPerksList[j]->GetName(), unlockedPerksList[j]->formID, fid);
								SerializePlayerUInt32Data(a_intfc, unlockedPerksList[j]->formID, !SerializableDataType::kPlayerUnlockedPerksList);
							}
						}
					}
				}
			}
		}

		void SerializePlayerFloatData(SKSE::SerializationInterface* a_intfc, const float& a_data, const uint32_t& a_recordType)
		{
			// Attempt to write float value to SKSE co-save.

			if (!a_intfc->WriteRecordData(a_data))
			{
				logger::error("[SERIAL] ERR: SerializePlayerFloatData: Could not write FLOAT record data ({}), record type: {}.",
					a_data, TypeToString(a_recordType));
			}
		}

		void SerializePlayerStringData(SKSE::SerializationInterface* a_intfc, const RE::BSFixedString& a_data, const uint32_t& a_recordType)
		{
			// Attempt to write data from fixed string's buffer to the SKSE co-save.

			// +1 for the null terminator at the end of the string.
			if (!a_intfc->WriteRecordData(a_data.data(), a_data.length() * sizeof(char) + 1))
			{
				logger::error("[SERIAL] ERR: SerializePlayerStringData: Could not write STRING record data ({}, size: {}), record type: {}.",
					a_data.data(), a_data.length() * sizeof(char), TypeToString(a_recordType));
			}
		}

		void SerializePlayerUInt8Data(SKSE::SerializationInterface* a_intfc, const uint8_t& a_data, const uint32_t& a_recordType)
		{
			// Attempt to write an unsigned 8-bit integer to the SKSE co-save.

			if (!a_intfc->WriteRecordData(a_data))
			{
				logger::error("[SERIAL] ERR: SerializePlayerUInt8Data: Could not write UINT8 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType));
			}
		}

		void SerializePlayerUInt16Data(SKSE::SerializationInterface* a_intfc, const uint16_t& a_data, const uint32_t& a_recordType)
		{
			// Attempt to write an unsigned 16-bit integer to the SKSE co-save.

			if (!a_intfc->WriteRecordData(a_data))
			{
				logger::error("[SERIAL] ERR: SerializePlayerUInt16Data: Could not write UINT16 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType));
			}
		}

		void SerializePlayerUInt32Data(SKSE::SerializationInterface* a_intfc, const uint32_t& a_data, const uint32_t& a_recordType)
		{
			// Attempt to write an unsigned 32-bit integer to the SKSE co-save.

			if (!a_intfc->WriteRecordData(a_data))
			{
				logger::error("[SERIAL] ERR: SerializePlayerUInt32Data: Could not write UINT32 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType));
			}
		}

		void SetDefaultRetrievedData(SKSE::SerializationInterface* a_intfc)
		{
			// Set default data to write to the SKSE co-save. 
			// Done when no data has been serialized yet or when the serialization interface is unavailable.

			logger::debug("[SERIAL] SetDefaultRetrievedData.");
			RE::PlayerCharacter* p1 = RE::PlayerCharacter::GetSingleton();
			RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler || !p1)
			{
				logger::error("[SERIAL] ERR: SetDefaultRetrievedData: Could not get data handler: {}, P1: {} to set default retrieved data.",
					(bool)!dataHandler, (bool)!p1);
				return;
			}
			
			constexpr size_t numSkills = (size_t)Skill::kTotal;
			std::array<float, 3> hmsBasePointsList{ };
			std::array<float, 3> hmsIncList{ };
			std::array<float, numSkills> skillBaseLvlList{ };
			std::array<float, numSkills> skillIncList{ };
			std::array<float, numSkills> skillXPList{ };

			// Serialize the co-op player's current level as their base level.
			// Co-op actors start with 100.0 base HMS points, level 15 skills, 
			// and 0 skill XP and level/HMS point increases across the board.
			hmsBasePointsList.fill(100.0f);
			hmsIncList.fill(0.0f);
			skillBaseLvlList.fill(15.0f);
			skillIncList.fill(0.0f);
			skillXPList.fill(0.0f);
			uint32_t sharedPerksUnlocked = 0;

			float p1Level = p1->GetLevel();
			logger::debug("[SERIAL] SetDefaultRetrievedData: P1's level: {}. Will set as first saved level for all players.", p1Level);

			// Set skill base AVs.
			skillBaseLvlList = Util::GetActorSkillAVs(p1);

			// REMOVE when done debugging.
			for (auto i = 0; i < skillBaseLvlList.size(); ++i)
			{
				auto currentSkill = static_cast<Skill>(i);
				if (glob.SKILL_TO_AV_MAP.contains(currentSkill))
				{
					auto currentAV = glob.SKILL_TO_AV_MAP.at(currentSkill);
					logger::debug("[SERIAL] SetDefaultRetrievedData: P1's {} skill base level: {}.", std::format("{}", currentAV), skillBaseLvlList[i]);
				}
			}

			// Set initial skill XP.
			auto currentAV = RE::ActorValue::kNone;
			for (auto i = 0; i < Skill::kTotal; ++i)
			{
				currentAV = glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
				skillXPList[i] = p1->skills->data->skills[i].xp;

				// REMOVE when done debugging.
				logger::debug("[SERIAL] SetDefaultRetrievedData: P1's {} skill XP inc: {}.", std::format("{}", currentAV), skillXPList[i]);
			}

			if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
			{
				// Set base P1 HMS values to their starting values if Enderal is not installed.
				// All perks are also cleared, meaning HMS values must be re-assigned along with perks through the Stats Menu.
				p1->SetBaseActorValue(RE::ActorValue::kHealth, 100.0f);
				p1->SetBaseActorValue(RE::ActorValue::kMagicka, 100.0f);
				p1->SetBaseActorValue(RE::ActorValue::kStamina, 100.0f);
				p1->SetActorValue(RE::ActorValue::kHealth, 100.0f);
				p1->SetActorValue(RE::ActorValue::kMagicka, 100.0f);
				p1->SetActorValue(RE::ActorValue::kStamina, 100.0f);
			}

			// Insert P1 first.
			glob.serializablePlayerData.insert
			(
				{
					p1->formID, 
					std::make_unique<ALYSLC::GlobalCoopData::SerializablePlayerData>
					(
						std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal>(), 
						GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS,
						std::vector<RE::TESForm*>{!EquipIndex::kTotal, nullptr},
						p1->skills->data->xp, 
						0,
						0,
						p1Level,
						sharedPerksUnlocked,
						0,
						0, 
						hmsBasePointsList,
						hmsIncList,
						skillBaseLvlList,
						skillIncList,
						skillXPList, 
						std::vector<RE::BGSPerk*>()
					) 
				}
			);

			// Insert co-op companions.
			std::array<RE::Actor*, (size_t)(ALYSLC_MAX_PLAYER_COUNT - 1)> coopPlayers{ nullptr, nullptr, nullptr };
			if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
			{
				auto index = dataHandler->GetModIndex(GlobalCoopData::PLUGIN_NAME);
				logger::debug("[SERIAL] SetDefaultRetrievedData: '{}' is loaded at mod index {}.", GlobalCoopData::PLUGIN_NAME, index.has_value() ? index.value() : -1);
				coopPlayers[0] = dataHandler->LookupForm(0x22FD, GlobalCoopData::PLUGIN_NAME)->As<RE::Actor>();
				coopPlayers[1] = dataHandler->LookupForm(0x22FE, GlobalCoopData::PLUGIN_NAME)->As<RE::Actor>();
				coopPlayers[2] = dataHandler->LookupForm(0x22FF, GlobalCoopData::PLUGIN_NAME)->As<RE::Actor>();
			}
			else
			{
				logger::error("[SERIAL] ERR: SetDefaultRetrievedData: Could not get data handler to search for co-op players' forms.");
			}

			for (auto i = 0; i < coopPlayers.size(); ++i) 
			{
				if (coopPlayers[i]) 
				{
					auto fid = coopPlayers[i] ? coopPlayers[i]->formID : 0;
					if (fid)
					{
						// Set default cleared data first.
						// Same as P1's.
						hmsBasePointsList.fill(100.0f);
						hmsIncList.fill(0.0f);
						skillBaseLvlList.fill(15.0f);
						skillIncList.fill(0.0f);
						skillXPList.fill(0.0f);
						uint32_t sharedPerksUnlocked = 0;
							
						// Set initial skill base AVs.
						auto skillBaseLvlList = Util::GetActorSkillAVs(coopPlayers[i]);
						glob.serializablePlayerData.insert
						(
							{ 
								fid,
								std::make_unique<ALYSLC::GlobalCoopData::SerializablePlayerData>
								(
									std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal>(),
									GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS,
									std::vector<RE::TESForm*>{ !EquipIndex::kTotal, nullptr },
									p1->skills->data->xp,
									0,
									0,
									p1Level,
									0,
									0,
									0,
									hmsBasePointsList,
									hmsIncList,
									skillBaseLvlList,
									skillIncList,
									skillXPList,
									std::vector<RE::BGSPerk*>()
								) 
							}
						);
					}
					else
					{
						logger::error("[SERIAL] ERR: SetDefaultRetrievedData: Could not get __CoopCharacter{}'s form ID.", i + 1);
					}
				}
				else
				{
					logger::error("[SERIAL] ERR: SetDefaultRetrievedData: Could not get __CoopCharacter{}. Game will likely crash on summon.", i + 1);
				}
			}

			logger::debug("[SERIAL] SetDefaultRetrievedData: Default data set and ready to serialize!");
		}

		// Credits to po3 for the decode function from here:
		// https://github.com/powerof3/PapyrusExtenderSSE/blob/master/src/Serialization/Manager.cpp#L10
		std::string TypeToString(uint32_t a_type)
		{
			constexpr std::size_t SIZE = sizeof(uint32_t);

			std::string sig;
			sig.resize(SIZE);
			const char* iter = reinterpret_cast<char*>(&a_type);
			for (std::size_t i = 0, j = SIZE - 1; i < SIZE; ++i, --j)
			{
				sig[j] = iter[i];
			}

			return sig;
		}
	};
};

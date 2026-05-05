#include "Serialization.h"
#include <Compatibility.h>
#include <GlobalCoopData.h>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	using SerializablePlayerData = ALYSLC::GlobalCoopData::SerializablePlayerData;
	namespace Serialization
	{	
		RE::TESForm* GetFormFromRetrievedFID
		(
			SKSE::SerializationInterface* a_intfc, 
			RE::FormID& a_fid,
			RE::TESDataHandler* a_dataHandler
		)
		{
			// Get form from the provided serialized FID.

			bool succ = a_intfc->ResolveFormID(a_fid, a_fid);
			if (a_fid != 0 && succ)
			{
				RE::TESForm* form = RE::TESForm::LookupByID(a_fid);
				if (form)
				{
					DBG
					(
						"Found form with FID 0x{:X}: {}.", a_fid, form ? form->GetName() : "NONE"
					);
					return form;
				}

				if (!a_dataHandler)
				{
					return nullptr;
				}
				
				// If not found in Skyrim.esm, look through all mod files for the form.
				for (auto file : a_dataHandler->files)
				{
					if (!file)
					{
						continue;
					}

					// Note to self:
					// Raw FID for light plugins does NOT include the small file compile index.
					const auto form = 
					(
						file->IsLight() ? 
						a_dataHandler->LookupForm(a_fid & 0x00000FFF, file->fileName) :
						a_dataHandler->LookupForm(a_fid & 0x00FFFFFF, file->fileName)
					);
					if (form)
					{
						DBG
						(
							"Found form with FID 0x{:X}: {} in mod file {}.", 
							a_fid, form ? form->GetName() : "NONE",  file->fileName
						);
						return form;
					}
				}
			}
			else if (!succ)
			{
				ERR
				(
					"Could not resolve new form ID from retrieved form ID (0x{:X}).", a_fid
				);
				return nullptr;
			}
			
			if (a_fid != 0)
			{
				ERR
				(
					"Could not find form for retrieved form ID (0x{:X}).", a_fid
				);
			}

			return nullptr;
		}

		void Load(SKSE::SerializationInterface* a_intfc)
		{
			// Load all serialized data into our global serialized data structure.

			DBG("Read all serialized data from SKSE co-save.");
			{
				std::unique_lock<std::mutex> lock(serializationMutex);
				DBG
				(
					"Lock obtained. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);

				// If the serialization interface is not valid, set to the default retrieved data.
				if (!a_intfc)
				{
					ERR
					(
						"Could not get serialization interface. "
						"Setting default data for all players."
					);
					return;
				}

				// Clear out current data before reading in new data.
				if (!glob.serializablePlayerData.empty())
				{
					glob.serializablePlayerData.clear();
				}

				RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();
				if (!dataHandler)
				{
					ERR
					(
						"Could not get data handler. Cannot load serialized data."
					);
					return;
				}

				// Player form ID serialized as a key for each data type.
				RE::FormID fid = 0;
				// Type, serialization version, and length of record data read out.
				uint32_t type{ };
				uint32_t version{ };
				uint32_t length{ };
				// Read each record which contains data of the read-out type for each player.
				while (a_intfc->GetNextRecordInfo(type, version, length))
				{
					if (version != !SerializableDataType::kSerializationVersion)
					{
						ERR
						(
							"Serialized data version ({}) does not match current version ({}) "
							"for entry of type {}, length {}.",
							version, 
							!SerializableDataType::kSerializationVersion, 
							TypeToString(type), 
							length
						);
						continue;
					}

					// Skip invalid types.
					if (type != !SerializableDataType::kPlayerCharacterCoopID &&
						type != !SerializableDataType::kPlayerAvailablePerkPoints &&
						type != !SerializableDataType::kPlayerBaseHMSPointsList &&
						type != !SerializableDataType::kPlayerBaseSkillLevelsList &&
						type != !SerializableDataType::kPlayerCopiedMagicList &&
						type != !SerializableDataType::kPlayerEmoteIdleEvents &&
						type != !SerializableDataType::kPlayerEquippedObjectsList &&
						type != !SerializableDataType::kPlayerMagFavoritesList &&
						type != !SerializableDataType::kPlayerHotkeyedFormsList &&
						type != !SerializableDataType::kPlayerExtraPerkPoints &&
						type != !SerializableDataType::kPlayerFirstSavedLevel &&
						type != !SerializableDataType::kPlayerHMSPointsIncList &&
						type != !SerializableDataType::kPlayerLevel &&
						type != !SerializableDataType::kPlayerLevelXP &&
						type != !SerializableDataType::kPlayerSkillIncreasesList &&
						type != !SerializableDataType::kPlayerSkillLegendaryList &&
						type != !SerializableDataType::kPlayerSkillXPList &&
						type != !SerializableDataType::kPlayerTakenSharedPerks &&
						type != !SerializableDataType::kPlayerUnlockedPerksList &&
						type != !SerializableDataType::kPlayerUsedPerkPoints &&
						type != !SerializableDataType::kPlayerRaceMenuPresetName &&
						type != !SerializableDataType::kPlayerCharacterChosenRace)
					{
						DBG
						(
							"Skipping invalid data type {} for player with FID 0x{:X}.",
							type, fid
						);
						continue;
					}

					// Per-character data.
					for (auto i = 0; i < ALYSLC_COMPANION_CHARACTERS_COUNT + 1; ++i)
					{
						if (!a_intfc->ReadRecordData(fid) || !a_intfc->ResolveFormID(fid, fid))
						{
							// P1 is always inserted first and has an FID of 0x14.
							if (i == 0)
							{
								fid = 0x14;
							}
							else
							{
								// Get the FID for the missing companion player character.
								fid = dataHandler->LookupFormID
								(
									GlobalCoopData::PLAYER_CHARACTER_FIDS[i], 
									GlobalCoopData::PLUGIN_NAME
								);
							}

							if (fid == 0)
							{
								ERR
								(
									"Could not get companion character {}'s FID. '{}'",
									i + 1,
									GlobalCoopData::PLUGIN_NAME
								);
								continue;
							}
							
							ERR
							(
								"Could not read player form ID ({}, 0x{:X}) "
								"or resolve form ID ({}) for record of type {}, version {}, "
								"length {} when attempting to load serialized data. "
								"Continuing to next player record of the same type.",
								!a_intfc->ReadRecordData(fid), 
								fid,
								!a_intfc->ResolveFormID(fid, fid), 
								TypeToString(type), 
								version, 
								length
							);

							// If not inserted into the global co-op data map already,
							// insert the default data now.
							// This will happen if the number of selectable 
							// companion player characters changes 
							// or if there is a new serializable data type.
							// Done to avoid clearing all serialized data for all players
							// when either of these things change.
							// Other players with a serialized FID record 
							// will retain their saved data for this record type.
							const auto iter = glob.serializablePlayerData.find(fid);
							if (iter == glob.serializablePlayerData.end())
							{
								glob.serializablePlayerData.insert
								(
									{ 
										fid,
										std::make_unique<SerializablePlayerData>()
									}
								);
							}

							continue;
						}

						DBG
						(
							"About to retrieve serialized data for player with FID 0x{:X}.", 
							fid
						);
						// Insert default data at first.
						if (glob.serializablePlayerData.empty() ||
							!glob.serializablePlayerData.contains(fid))
						{
							DBG
							(
								"Added new serialized data set for player with FID 0x{:X}.",
								fid
							);
							glob.serializablePlayerData.insert
							(
								{ 
									fid,
									std::make_unique<SerializablePlayerData>()
								}
							);
						}

						const auto& data = glob.serializablePlayerData.at(fid);
						if (type == !SerializableDataType::kPlayerCharacterCoopID)
						{
							uint32_t playerCharacterID = 0;
							RetrieveUInt32Data(a_intfc, playerCharacterID, type);
							// Serialized as unsigned but deserialized as signed.
							data->SetPlayerCharacterID(playerCharacterID);
							DBG
							(
								"Player with FID 0x{:X}'s "
								"character ID is {}.", fid, data->GetPlayerCharacterID()
							);
						}
						else if (type == !SerializableDataType::kPlayerAvailablePerkPoints)
						{
							RetrieveUInt32Data(a_intfc, data->availablePerkPoints, type);
							DBG
							(
								"Player with FID 0x{:X} "
								"has {} available perk points to use still.",
								fid, data->availablePerkPoints
							);
						}
						else if (type == !SerializableDataType::kPlayerUsedPerkPoints)
						{
							RetrieveUInt32Data(a_intfc, data->usedPerkPoints, type);
							DBG
							(
								"Player with FID 0x{:X} has used {} perk points.",
								fid, data->usedPerkPoints
							);
						}
						else if (type == !SerializableDataType::kPlayerExtraPerkPoints)
						{
							RetrieveUInt32Data(a_intfc, data->extraPerkPoints, type);
							DBG
							(
								"Player with FID 0x{:X} has {} extra perks.",
								fid, data->extraPerkPoints
							);
						}
						else if (type == !SerializableDataType::kPlayerFirstSavedLevel)
						{
							RetrieveUInt32Data(a_intfc, data->firstSavedLevel, type);
							DBG
							(
								"Player with FID 0x{:X} "
								"was level {} when their data was first serialized.",
								fid, data->firstSavedLevel
							);
						}
						else if (type == !SerializableDataType::kPlayerLevel)
						{
							RetrieveUInt32Data(a_intfc, data->level, type);
							DBG
							(
								"Player with FID 0x{:X} has saved level: {}.", fid, data->level
							);
						}
						else if (type == !SerializableDataType::kPlayerLevelXP)
						{
							RetrieveFloatData(a_intfc, data->levelXP, type);
							DBG
							(
								"Player with FID 0x{:X} has saved level XP: {}.",
								fid, data->levelXP
							);
						}
						else if (type == !SerializableDataType::kPlayerBaseHMSPointsList)
						{
							for (auto j = 0; j < 3; ++j)
							{
								RetrieveFloatData(a_intfc, data->hmsBasePointsList[j], type);
								DBG
								(
									"Player with FID 0x{:X} "
									"has a base {} level of {} at first-saved level {}.",
									fid, 
									j == 0 ? "health" : j == 1 ? "magicka" : "stamina",
									data->hmsBasePointsList[j],
									data->firstSavedLevel
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerHMSPointsIncList)
						{
							for (auto j = 0; j < 3; ++j)
							{
								RetrieveFloatData
								(
									a_intfc, data->hmsPointIncreasesList[j], type
								);
								DBG
								(
									"Player with FID 0x{:X} has increased {} by {}.",
									fid, 
									j == 0 ? "Health" : j == 1 ? "Magicka" : "Stamina",
									data->hmsPointIncreasesList[j]
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerBaseSkillLevelsList)
						{
							for (auto j = 0; j < Skill::kTotal; ++j)
							{
								RetrieveFloatData(a_intfc, data->skillBaseLevelsList[j], type);
								DBG
								(
									"Player with FID 0x{:X} has base skill {} of {}.",
									fid, 
									Util::GetActorValueName
									(
										glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))
									), 
									data->skillBaseLevelsList[j]
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerSkillIncreasesList)
						{
							for (auto j = 0; j < Skill::kTotal; ++j)
							{
								RetrieveFloatData
								(
									a_intfc, data->skillLevelIncreasesList[j], type
								);
								DBG
								(
									"Player with FID 0x{:X} has increased skill {} by {}.",
									fid, 
									Util::GetActorValueName
									(
										glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))
									), 
									data->skillLevelIncreasesList[j]
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerSkillLegendaryList)
						{
							for (auto j = 0; j < Skill::kTotal; ++j)
							{
								RetrieveUInt32Data(a_intfc, data->skillLegendaryList[j], type);
								DBG
								(
									"Player with FID 0x{:X} has made {} Legendary {} times.",
									fid, 
									Util::GetActorValueName
									(
										glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))
									), 
									data->skillLegendaryList[j]
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerSkillXPList)
						{
							for (auto j = 0; j < Skill::kTotal; ++j)
							{
								RetrieveFloatData(a_intfc, data->skillXPList[j], type);
								DBG
								(
									"Player with FID 0x{:X} has gained {} XP for skill {}.",
									fid,
									data->skillXPList[j], 
									Util::GetActorValueName
									(
										glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))
									)
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerEquippedObjectsList)
						{
							// Read in all saved equipped forms.
							uint32_t numEquippedForms = 0;
							RetrieveUInt32Data(a_intfc, numEquippedForms, type);
							DBG
							(
								"Player with FID 0x{:X} has {} equipped forms.",
								fid, numEquippedForms
							);

							data->equippedForms.clear();
							RE::FormID equippedFID = 0;
							for (auto j = 0; j < numEquippedForms; ++j)
							{
								RetrieveUInt32Data(a_intfc, equippedFID, type);
								data->equippedForms.emplace_back
								(
									GetFormFromRetrievedFID(a_intfc, equippedFID, dataHandler)
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerMagFavoritesList)
						{
							// Read in all saved favorited magical forms.
							uint32_t numMagForms = 0;
							RetrieveUInt32Data(a_intfc, numMagForms, type);
							DBG
							(
								"Player with FID 0x{:X} has {} favorited magical forms.", 
								fid, numMagForms
							);

							data->favoritedMagForms.clear();
							RE::FormID equippedFID = 0;
							for (auto j = 0; j < numMagForms; ++j)
							{
								RetrieveUInt32Data(a_intfc, equippedFID, type);
								data->favoritedMagForms.emplace_back
								(
									GetFormFromRetrievedFID(a_intfc, equippedFID, dataHandler)
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerHotkeyedFormsList)
						{
							// Read in all saved hotkeyed forms.
							RE::FormID hotkeyedFID = 0;
							RE::TESForm* hotkeyedForm = nullptr;
							for (auto j = 0; j < data->hotkeyedForms.size(); ++j)
							{
								RetrieveUInt32Data(a_intfc, hotkeyedFID, type);
								if (hotkeyedFID) 
								{
									hotkeyedForm = GetFormFromRetrievedFID
									(
										a_intfc, hotkeyedFID, dataHandler
									);
								}
								else
								{
									// Empty slot, still assign.
									hotkeyedForm = nullptr;
								}

								data->hotkeyedForms[j] = hotkeyedForm;
								DBG
								(
									"Player with FID 0x{:X} has {} hotkeyed in slot {}.", 
									fid, 
									hotkeyedForm ? hotkeyedForm->GetName() : "NONE", 
									j + 1
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerCopiedMagicList)
						{
							// Read in saved copied magic spell forms.
							data->copiedMagic.fill(nullptr);
							RE::FormID magicFID{ };
							for (uint8_t i = 0; i < !PlaceholderMagicIndex::kTotal; ++i) 
							{
								RetrieveUInt32Data(a_intfc, magicFID, type);
								auto form = GetFormFromRetrievedFID
								(
									a_intfc, magicFID, dataHandler
								); 
								if (form && form->Is(RE::FormType::Spell, RE::FormType::Shout))
								{
									data->copiedMagic[i] = form;
								}
								else
								{
									// Empty slot, still assign.
									data->copiedMagic[i] = nullptr;
								}

								DBG
								(
									"Player with FID 0x{:X} "
									"has copied magic form {} (0x{:X}) in slot {}.",
									fid, 
									data->copiedMagic[i] ?
									data->copiedMagic[i]->GetName() : 
									"NONE",
									data->copiedMagic[i] ? 
									data->copiedMagic[i]->formID : 
									0xDEAD,
									i
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerEmoteIdleEvents)
						{
							// Eight cyclable emote idles' event names.
							RE::BSFixedString eventName = "NONE";
							uint32_t size = 0;
							for (uint8_t i = 0; i < data->cyclableEmoteIdleEvents.size(); ++i) 
							{
								// Length of serialized event name string.
								// NOTE:
								// Had issues deserializing the empty string 
								// (reading memory beyond null terminator), 
								// so read in as "NONE" instead.
								RetrieveUInt32Data(a_intfc, size, type);
								if (size <= 1) 
								{
									data->cyclableEmoteIdleEvents[i] = "NONE";
									continue;
								}

								RetrieveStringData(a_intfc, eventName, type, size);
								data->cyclableEmoteIdleEvents[i] = eventName;
								DBG
								(
									"Player with FID 0x{:X}'s "
									"saved cyclable emote idle event {} is {}. "
									"String length: {}.", 
									fid, i, eventName, size
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerTakenSharedPerks)
						{
							// Read in all unlocked shared perks.
							uint32_t numUnlockedPerks = 0;
							RetrieveUInt32Data(a_intfc, numUnlockedPerks, type);
							DBG
							(
								"Player with FID 0x{:X} has personally unlocked {} shared perks.",
								fid, numUnlockedPerks
							);

							// Clear and assign.
							data->ClearTakenSharedPerks();
							RE::TESForm* perkForm = nullptr;
							RE::BGSPerk* perk = nullptr;
							RE::FormID perkFID = 0;
							for (auto j = 0; j < numUnlockedPerks; ++j)
							{
								RetrieveUInt32Data(a_intfc, perkFID, type);
								perkForm = GetFormFromRetrievedFID
								(
									a_intfc, perkFID, dataHandler
								);
								perk = perkForm ? perkForm->As<RE::BGSPerk>() : nullptr;
								data->InsertTakenSharedPerk(perk);
							}

							// Set total.
							data->sharedPerksTaken = numUnlockedPerks;
						}
						else if (type == !SerializableDataType::kPlayerUnlockedPerksList)
						{
							// Read in all unlocked perks.
							uint32_t numUnlockedPerks = 0;
							RetrieveUInt32Data(a_intfc, numUnlockedPerks, type);
							DBG
							(
								"Player with FID 0x{:X} has {} unlocked perks.",
								fid, numUnlockedPerks
							);

							// Clear and assign.
							data->ClearUnlockedPerks();
							RE::TESForm* perkForm = nullptr;
							RE::BGSPerk* perk = nullptr;
							RE::FormID perkFID = 0;
							for (auto j = 0; j < numUnlockedPerks; ++j)
							{
								RetrieveUInt32Data(a_intfc, perkFID, type);
								perkForm = GetFormFromRetrievedFID
								(
									a_intfc, perkFID, dataHandler
								);
								perk = perkForm ? perkForm->As<RE::BGSPerk>() : nullptr;
								data->InsertUnlockedPerk(perk);
							}

							// Set total.
							data->prevTotalUnlockedPerks = numUnlockedPerks;
						}
						else if (type == !SerializableDataType::kPlayerRaceMenuPresetName)
						{
							// One '.jslot' preset file name.
							RE::BSFixedString presetName = "NONE";
							uint32_t size = 0;
							// Length of serialized event name string (+1 from null terminator).
							// NOTE:
							// Had issues deserializing the empty string 
							// (reading memory beyond null terminator), 
							// so read in as "NONE" instead.
							RetrieveUInt32Data(a_intfc, size, type);
							if (size <= 1)
							{
								data->raceMenuPresetName = "NONE";
							}
							else
							{
								RetrieveStringData(a_intfc, presetName, type, size);
								data->raceMenuPresetName = presetName;
								DBG
								(
									"Player with FID 0x{:X}'s "
									"applied RaceMenu preset is {}. "
									"String length: {}.", 
									fid, presetName, size
								);
							}
						}
						else if (type == !SerializableDataType::kPlayerCharacterChosenRace)
						{
							RE::TESForm* raceForm = nullptr;
							RE::FormID raceFID = 0;
							RetrieveUInt32Data(a_intfc, raceFID, type);
							raceForm = GetFormFromRetrievedFID
							(
								a_intfc, raceFID, dataHandler
							);
							data->chosenRace = raceForm ? raceForm->As<RE::TESRace>() : nullptr;
							DBG
							(
								"Player with FID 0x{:X}'s "
								"chosen race is {} (0x{:X}, editor ID {}). ",
								fid, 
								data->chosenRace ? data->chosenRace->GetName() : "NONE",
								data->chosenRace ? data->chosenRace->formID : 0xDEAD,
								Util::GetEditorID(data->chosenRace)
							);
						}
					}
				}

				// Set default data if no data was retrieved successfully.
				if (glob.serializablePlayerData.size() == 0)
				{
					DBG("First time retrieval. Setting default data for all players.");
					SetDefaultRetrievedData();
				}
				else
				{
					DBG
					(
						"Successfully retrieved serialized data for {} "
						"player characters from SKSE co-save.",
						ALYSLC_COMPANION_CHARACTERS_COUNT + 1
					);
				}
			}
		}

		void RetrieveFloatData
		(
			SKSE::SerializationInterface* a_intfc, float& a_data, const uint32_t& a_recordType
		)
		{
			// Attempt to read a float value.

			if (!a_intfc->ReadRecordData(a_data))
			{
				ERR
				(
					"Could not read FLOAT record data ({}), record type: {}.",
					a_data, TypeToString(a_recordType)
				);
			}
		}

		void RetrieveStringData
		(
			SKSE::SerializationInterface* a_intfc, 
			RE::BSFixedString& a_data, 
			const uint32_t& a_recordType, 
			const uint32_t& a_size
		)
		{
			// Attempt to read char buffer data into a fixed string.

			// Reserve a string of the serialized size (includes null terminator in size).
			std::string buff{ };
			buff.reserve(a_size);
			if (!a_intfc->ReadRecordData(buff.data(), a_size))
			{
				ERR
				(
					"Could not read STRING record data ({}, given size {}), record type: {}.",
					a_data, a_size, TypeToString(a_recordType)
				);
			}
			else
			{
				// Assign the underlying buffer since direct assignment 
				// to the string does not work.
				a_data = buff.data();
			}
		}

		void RetrieveUInt8Data
		(
			SKSE::SerializationInterface* a_intfc, uint8_t& a_data, const uint32_t& a_recordType
		)
		{
			// Attempt to read an unsigned 8-bit integer.

			if (!a_intfc->ReadRecordData(a_data))
			{
				ERR
				(
					"Could not read UINT8 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType)
				);
			}
		}

		void RetrieveUInt16Data
		(
			SKSE::SerializationInterface* a_intfc, uint16_t& a_data, const uint32_t& a_recordType
		)
		{
			// Attempt to read an unsigned 16-bit integer.

			if (!a_intfc->ReadRecordData(a_data))
			{
				ERR
				(
					"Could not read UINT16 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType)
				);
			}
		}

		void RetrieveUInt32Data
		(
			SKSE::SerializationInterface* a_intfc, uint32_t& a_data, const uint32_t& a_recordType
		)
		{
			// Attempt to read an unsigned 32-bit integer.

			if (!a_intfc->ReadRecordData(a_data))
			{
				ERR
				(
					"Could not read UINT32 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType)
				);
			}
		}

		void Revert(SKSE::SerializationInterface* a_intfc)
		{
			// Ensure no co-op session is active as the game reverts.

			if (glob.globalDataInit && glob.allPlayersInit) 
			{
				DBG("Stopping active co-op session.");
				GlobalCoopData::TearDownCoopSession(true);
			}
		}

		void Save(SKSE::SerializationInterface* a_intfc)
		{
			// Save all our co-op serializable data to the SKSE co-save.
			DBG("Writing all serializable data to SKSE co-save.");
			if (!a_intfc)
			{
				ERR
				(
					"Could not get serialization interface ({}), "
					"co-op session active ({}). Cannot serialize co-op data.",
					(bool)!a_intfc, glob.coopSessionActive
				);
				return;
			}
			
			{
				std::unique_lock<std::mutex> lock(serializationMutex);
				DBG
				(
					"Lock obtained. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);

				//=========================================
				// Each player's saved data.
				//=========================================

				// NOTE:
				// Capitalized data type comments are for easier recognition by my monkey brain.

				// PLAYER CHARACTER CO-OP ID
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerCharacterCoopID, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						DBG
						(
							"Serialize PLAYER CHARACTER CO-OP ID for player with FID 0x{:X}: {}.",
							fid, data->GetPlayerCharacterID()
						);
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerCharacterCoopID
						);
						// Serialized as unsigned but deserialized as signed.
						SerializePlayerUInt32Data
						(
							a_intfc, 
							data->GetPlayerCharacterID(), 
							!SerializableDataType::kPlayerCharacterCoopID
						);
					}
				}

				// AVAILABLE PERK POINTS
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerAvailablePerkPoints, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						DBG
						(
							"Serialize AVAIL PERK POINTS for player with FID 0x{:X}: {}.",
							fid, data->availablePerkPoints
						);
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerAvailablePerkPoints
						);
						SerializePlayerUInt32Data
						(
							a_intfc, 
							data->availablePerkPoints, 
							!SerializableDataType::kPlayerAvailablePerkPoints
						);
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerAvailablePerkPoints)
					);
				}

				// USED PERK POINTS
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerUsedPerkPoints,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						DBG
						(
							"Serialize USED PERK POINTS for player with FID 0x{:X}: {}.", 
							fid, data->usedPerkPoints
						);
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerUsedPerkPoints
						);
						SerializePlayerUInt32Data
						(
							a_intfc, 
							data->usedPerkPoints, 
							!SerializableDataType::kPlayerUsedPerkPoints
						);
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerUsedPerkPoints)
					);
				}

				// EXTRA PERK POINTS
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerExtraPerkPoints, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						DBG
						(
							"Serialize EXTRA PERK POINTS for player with FID 0x{:X}: {}.",
							fid, data->extraPerkPoints
						);
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerExtraPerkPoints
						);
						SerializePlayerUInt32Data
						(
							a_intfc, 
							data->extraPerkPoints,
							!SerializableDataType::kPlayerExtraPerkPoints
						);
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerExtraPerkPoints)
					);
				}

				// BASE LVL
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerFirstSavedLevel,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						DBG
						(
							"Serialize BASE LVL for player with FID 0x{:X}: {}.", 
							fid, data->firstSavedLevel
						);
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerFirstSavedLevel
						);
						SerializePlayerUInt32Data
						(
							a_intfc,
							data->firstSavedLevel, 
							!SerializableDataType::kPlayerFirstSavedLevel
						);
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerFirstSavedLevel)
					);
				}

				// LVL
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerLevel, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						DBG
						(
							"Serialize LVL for player with FID 0x{:X}: {}.", fid, data->level
						);
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerLevel
						);
						SerializePlayerUInt32Data
						(
							a_intfc, data->level, !SerializableDataType::kPlayerLevel
						);
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerLevel)
					);
				}

				// LVL XP
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerLevelXP, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						DBG
						(
							"Serialize LVL XP for player with FID 0x{:X}: {}.", fid, data->levelXP
						);
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerLevelXP
						);
						SerializePlayerFloatData
						(
							a_intfc, data->levelXP, !SerializableDataType::kPlayerLevelXP
						);
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerLevelXP)
					);
				}

				// HMS BASE POINTS LIST
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerBaseHMSPointsList,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerBaseHMSPointsList
						);
						uint32_t numEntries = data->hmsBasePointsList.size();
						if (numEntries > 0)
						{
							for (uint8_t j = 0; j < numEntries; ++j)
							{
								DBG
								(
									"Serialize HMS BASE POINTS LIST "
									"for player with FID 0x{:X}: Base {} is {}.", 
									fid,
									j == 0 ? "health" : j == 1 ? "magicka" : "stamina", 
									data->hmsBasePointsList[j]
								);
								SerializePlayerFloatData
								(
									a_intfc,
									data->hmsBasePointsList[j], 
									!SerializableDataType::kPlayerBaseHMSPointsList
								);
							}
						}
						else
						{
							ERR
							(
								"HMS BASE POINTS LIST is empty "
								"for player with FID 0x{:X}. Saving 100 as base value "
								"for health, magicka, and stamina.", 
								fid
							);
							for (uint8_t j = 0; j < 3; ++j)
							{
								SerializePlayerFloatData
								(
									a_intfc, 
									100.0f, 
									!SerializableDataType::kPlayerBaseHMSPointsList
								);
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerBaseHMSPointsList)
					);
				}

				// HMS INC LIST
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerHMSPointsIncList,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerHMSPointsIncList
						);
						uint32_t numEntries = data->hmsPointIncreasesList.size();
						if (numEntries > 0)
						{
							for (uint8_t j = 0; j < numEntries; ++j)
							{
								DBG
								(
									"Serialize HMS INC LIST "
									"for player with FID 0x{:X}: {} increment is {}.", 
									fid, 
									j == 0 ? "Health" : j == 1 ? "Magicka" : "Stamina", 
									data->hmsPointIncreasesList[j]
								);
								SerializePlayerFloatData
								(
									a_intfc, 
									data->hmsPointIncreasesList[j], 
									!SerializableDataType::kPlayerHMSPointsIncList
								);
							}
						}
						else
						{
							ERR
							(
								"HMS INC LIST is empty "
								"for player with FID 0x{:X}. Saving 0 point increases "
								"for health, magicka, and stamina.",
								fid
							);
							for (uint8_t j = 0; j < 3; ++j)
							{
								SerializePlayerFloatData
								(
									a_intfc, 0.0f, !SerializableDataType::kPlayerHMSPointsIncList
								);
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerHMSPointsIncList)
					);
				}

				// BASE SKILL LVL LIST
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerBaseSkillLevelsList,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerBaseSkillLevelsList
						);
						// Do not need to write the number of skill XP entries first, 
						// since this corresponds to the fixed number of skills 
						// and is constant from save to save.
						uint32_t numSkillLvlEntries = data->skillBaseLevelsList.size();
						if (numSkillLvlEntries > 0)
						{
							for (uint8_t j = 0; j < numSkillLvlEntries; ++j)
							{
								DBG
								(
									"Serialize BASE SKILL LVL LIST "
									"for player with FID 0x{:X}: Skill {} has base value {}.", 
									fid, 
									Util::GetActorValueName
									(
										glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))
									), 
									data->skillBaseLevelsList[j]
								);
								SerializePlayerFloatData
								(
									a_intfc, 
									data->skillBaseLevelsList[j], 
									!SerializableDataType::kPlayerBaseSkillLevelsList
								);
							}
						}
						else
						{
							ERR
							(
								"BASE SKILL LVL LIST list is empty "
								"for player 0x{:X}. Getting and setting each skill again "
								"for the player.",
								fid
							);
							const auto playerActor = RE::TESForm::LookupByID<RE::Actor>(fid); 
							if (playerActor)
							{
								auto skillBaseList = Util::GetActorSkillLevels
								(
									playerActor->As<RE::Actor>()
								);
								uint8_t numSkillLvlEntries = skillBaseList.size();
								for (uint8_t j = 0; j < numSkillLvlEntries; ++j)
								{
									DBG
									(
										"Serialize BASE SKILL LVL LIST "
										"for player with FID 0x{:X}: "
										"Skill {} has base value {}.",
										fid, 
										Util::GetActorValueName
										(
											glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))
										), 
										skillBaseList[j]
									);
									SerializePlayerFloatData
									(
										a_intfc, 
										skillBaseList[j], 
										!SerializableDataType::kPlayerBaseSkillLevelsList
									);
								}
							}
							else
							{
								ERR
								(
									"Could not get player actor "
									"for FID 0x{:X}. "
									"Setting each skill level to a base level of 15 "
									"for the player.",
									fid
								);
								uint8_t numSkillLvlEntries = Skill::kTotal;
								for (uint8_t j = 0; j < numSkillLvlEntries; ++j)
								{
									SerializePlayerFloatData
									(
										a_intfc, 
										15.0f, 
										!SerializableDataType::kPlayerBaseSkillLevelsList
									);
								}
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerBaseSkillLevelsList)
					);
				}

				// SKILL LVL INC LIST
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerSkillIncreasesList, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerSkillIncreasesList
						);
						// Do not need to write the number of skill XP entries first, 
						// since this corresponds to the number of skill schools 
						// and is constant from save to save.
						uint32_t numSkillIncEntries = data->skillLevelIncreasesList.size();
						if (numSkillIncEntries > 0)
						{
							for (uint8_t j = 0; j < numSkillIncEntries; ++j)
							{
								DBG
								(
									"Serialize SKILL LVL INC LIST "
									"for player with FID 0x{:X}: Skill {} has increment {}.",
									fid,
									Util::GetActorValueName
									(
										glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))
									), 
									data->skillLevelIncreasesList[j]
								);
								SerializePlayerFloatData
								(
									a_intfc,
									data->skillLevelIncreasesList[j], 
									!SerializableDataType::kPlayerSkillIncreasesList
								);
							}
						}
						else
						{
							ERR
							(
								"SKILL LVL INC LIST is empty "
								"for player 0x{:X}. Saving 0 level increase for each skill.",
								fid
							);
							for (uint8_t j = 0; j < numSkillIncEntries; ++j)
							{
								SerializePlayerFloatData
								(
									a_intfc,
									0.0f,
									!SerializableDataType::kPlayerSkillIncreasesList
								);
							}

							const auto playerActor = RE::TESForm::LookupByID<RE::Actor>(fid); 
							if (playerActor)
							{
								ERR
								(
									"Re-serializing BASE LVL as 0 "
									"for player with FID 0x{:X} to force auto-scaling "
									"for all skill AVs until next level up "
									"during a co-op session.", 
									fid
								);
								if (a_intfc->OpenRecord
								(
									!SerializableDataType::kPlayerFirstSavedLevel,
									!SerializableDataType::kSerializationVersion
								))
								{
									// Have to serialize all first saved levels again
									// for all other players.
									for (auto& [fid2, data2] : glob.serializablePlayerData)
									{
										if (fid2 != fid)
										{
											DBG
											(
												"Re-serialize BASE LVL "
												"for player with FID 0x{:X}: {}.",
												fid2, data2->firstSavedLevel
											);
											SerializePlayerUInt32Data
											(
												a_intfc,
												fid2, 
												!SerializableDataType::kPlayerFirstSavedLevel
											);
											SerializePlayerUInt32Data
											(
												a_intfc, 
												data2->firstSavedLevel, 
												!SerializableDataType::kPlayerFirstSavedLevel
											);
											continue;
										}

										DBG
										(
											"Serialize BASE LVL "
											"for player with FID 0x{:X} as 0 "
											"to force AV auto-scaling.", 
											fid2
										);
										SerializePlayerUInt32Data
										(
											a_intfc,
											fid2, 
											!SerializableDataType::kPlayerFirstSavedLevel
										);
										SerializePlayerUInt32Data
										(
											a_intfc,
											0, 
											!SerializableDataType::kPlayerFirstSavedLevel
										);
									}
								}
								else
								{
									ERR
									(
										"Could not open record of type {}.",
										TypeToString(!SerializableDataType::kPlayerFirstSavedLevel)
									);
								}
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerSkillIncreasesList)
					);
					
				}

				// SKILL LEGENDARY COUNT LIST
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerSkillLegendaryList, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerSkillLegendaryList
						);
						// Do not need to write the number of legendary count entries first,
						// since this corresponds to the number of skill schools 
						// and is constant from save to save.
						uint32_t numLegendaryCountEntries = data->skillLegendaryList.size();
						if (numLegendaryCountEntries > 0)
						{
							for (uint8_t j = 0; j < numLegendaryCountEntries; ++j)
							{
								DBG
								(
									"Serialize SKILL LEGENDARY COUNT LIST "
									"for player with FID 0x{:X}: "
									"Skill {} has been made Legendary {} times.",
									fid, 
									Util::GetActorValueName
									(
										glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))
									),
									data->skillLegendaryList[j]
								);
								SerializePlayerUInt32Data
								(
									a_intfc, 
									data->skillLegendaryList[j], 
									!SerializableDataType::kPlayerSkillLegendaryList
								);
							}
						}
						else
						{
							ERR
							(
								"SKILL LEGENDARY COUNT LIST is empty "
								"for player with FID 0x{:X}."
								"Saving 0 Legendary levels for each skill.",
								fid
							);
							for (uint8_t j = 0; j < numLegendaryCountEntries; ++j)
							{
								SerializePlayerUInt32Data
								(
									a_intfc, 0, !SerializableDataType::kPlayerSkillLegendaryList
								);
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerSkillLegendaryList)
					);
				}

				// SKILL XP LIST
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerSkillXPList, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerSkillXPList
						);
						// Do not need to write the number of skill XP entries first,
						// since this corresponds to the number of skill schools 
						// and is constant from save to save.
						uint32_t numSkillXPEntries = data->skillXPList.size();
						if (numSkillXPEntries > 0)
						{
							for (uint8_t j = 0; j < numSkillXPEntries; ++j)
							{
								DBG
								(
									"Serialize SKILL XP LIST "
									"for player with FID 0x{:X}: Skill {} has {} XP.",
									fid, 
									Util::GetActorValueName
									(
										glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(j))
									),
									data->skillXPList[j]
								);
								SerializePlayerFloatData
								(
									a_intfc, 
									data->skillXPList[j], 
									!SerializableDataType::kPlayerSkillXPList
								);
							}
						}
						else
						{
							ERR
							(
								"SKILL XP LIST is empty "
								"for player with FID 0x{:X}. Saving 0 XP for each skill, "
								"which will reset skill XP levels to P1's corresponding skill XPs "
								"on load.",
								fid
							);
							for (uint8_t j = 0; j < numSkillXPEntries; ++j)
							{
								SerializePlayerFloatData
								(
									a_intfc, 0.0f, !SerializableDataType::kPlayerSkillXPList
								);
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerSkillXPList)
					);
				}

				// EQUIPPED OBJECTS
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerEquippedObjectsList, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerEquippedObjectsList
						);
						// Write the number of equipped objects first, 
						// as this varies from save to save.
						uint32_t numEquippedForms = data->equippedForms.size();
						SerializePlayerUInt32Data
						(
							a_intfc,
							numEquippedForms, 
							!SerializableDataType::kPlayerEquippedObjectsList
						);
						DBG
						(
							"Serialize EQUIPPED OBJECTS LIST "
							"for 0x{:X}. Num equipped forms: {}.", 
							fid, numEquippedForms
						);

						// Write each equipped form's FID next.
						if (numEquippedForms > 0)
						{
							for (uint16_t j = 0; j < numEquippedForms; ++j)
							{
								const auto form = data->equippedForms[j];
								if (form)
								{
									DBG
									(
										"Serialize EQUIPPED OBJECT {} (0x{:X}) "
										"for player with FID 0x{:X}.",
										form->GetName(), form->formID, fid
									);
									SerializePlayerUInt32Data
									(
										a_intfc, 
										form->formID,
										!SerializableDataType::kPlayerEquippedObjectsList
									);
								}
								else
								{
									// Empty slot, write 0 for FID.
									SerializePlayerUInt32Data
									(
										a_intfc,
										0, 
										!SerializableDataType::kPlayerEquippedObjectsList
									);
								}
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerEquippedObjectsList)
					);
					
				}

				// FAVORITED MAGICAL OBJECTS
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerMagFavoritesList, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerMagFavoritesList
						);
						// Write the number of magical forms first,
						// as this varies from save to save.
						uint32_t numMagForms = data->favoritedMagForms.size();
						SerializePlayerUInt32Data
						(
							a_intfc, numMagForms, !SerializableDataType::kPlayerMagFavoritesList
						);
						DBG
						(
							"Serialize MAGICAL FAVORITES LIST "
							"for 0x{:X}. Num magical forms: {}.",
							fid, numMagForms
						);

						// Write each form's FID next.
						if (numMagForms > 0)
						{
							for (uint16_t j = 0; j < numMagForms; ++j)
							{
								const auto form = data->favoritedMagForms[j];
								if (form)
								{
									DBG
									(
										"Serialize MAGICAL FORM {} (0x{:X}) "
										"for player with FID 0x{:X}.",
										form->GetName(), form->formID, fid
									);
									SerializePlayerUInt32Data
									(
										a_intfc, 
										form->formID, 
										!SerializableDataType::kPlayerMagFavoritesList
									);
								}
								else
								{
									// Empty slot, write 0 for FID.
									SerializePlayerUInt32Data
									(
										a_intfc, 
										0, 
										!SerializableDataType::kPlayerMagFavoritesList
									);
								}
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerMagFavoritesList)
					);
					
				}

				// HOTKEYED FORMS
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerHotkeyedFormsList, 
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerHotkeyedFormsList
						);
						// Write each hotkeyed form's FID.
						for (uint16_t j = 0; j < data->hotkeyedForms.size(); ++j)
						{
							const auto form = data->hotkeyedForms[j];
							if (form)
							{
								DBG
								(
									"Serialize HOTKEYED FORM {} (0x{:X}) "
									"in slot {} for player with FID 0x{:X}.",
									form->GetName(),
									form->formID,
									j + 1,
									fid
								);
								SerializePlayerUInt32Data
								(
									a_intfc, 
									form->formID, 
									!SerializableDataType::kPlayerHotkeyedFormsList
								);
							}
							else
							{
								// Empty slot, write 0 for FID.
								SerializePlayerUInt32Data
								(
									a_intfc, 
									0, 
									!SerializableDataType::kPlayerHotkeyedFormsList
								);
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerHotkeyedFormsList)
					);
				}

				// COPIED SPELLS LIST
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerCopiedMagicList,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerCopiedMagicList
						);
						RE::FormID magicFID{ };
						const auto& copiedMagic = data->copiedMagic;
						for (uint8_t i = 0; i < !PlaceholderMagicIndex::kTotal; ++i)
						{
							magicFID = 
							(
								(
									copiedMagic[i] && 
									copiedMagic[i]->Is(RE::FormType::Spell, RE::FormType::Shout)
								) ? 
								copiedMagic[i]->formID :
								0
							);
							SerializePlayerUInt32Data
							(
								a_intfc, magicFID, !SerializableDataType::kPlayerCopiedMagicList
							);
							DBG
							(
								"Player with FID 0x{:X} "
								"has COPIED SPELL form {} (0x{:X}) in slot {}.",
								fid,
								copiedMagic[i] ? copiedMagic[i]->GetName() : "NONE",
								copiedMagic[i] ? copiedMagic[i]->formID : 0xDEAD,
								i
							);
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerCopiedMagicList)
					);
				}

				// CYCLABLE EMOTE IDLE EVENTS LIST
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerEmoteIdleEvents,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerEmoteIdleEvents
						);
						const auto& cyclableEmoteIdleEvents = data->cyclableEmoteIdleEvents;
						uint32_t size = 0;
						for (uint8_t i = 0; i < cyclableEmoteIdleEvents.size(); ++i)
						{
							// Serialize string length and then the string's buffer 
							// for each saved emote idle event name.
							// +1 for the null terminator at the end of the string.
							// NOTE:
							// Had issues deserializing the empty string 
							// (reading memory beyond null terminator), 
							// so save as "NONE" instead.
							size = cyclableEmoteIdleEvents[i].length() * sizeof(char) + 1;
							SerializePlayerUInt32Data
							(
								a_intfc, 
								cyclableEmoteIdleEvents[i].empty() ? 
								strlen("NONE") :
								size,
								!SerializableDataType::kPlayerEmoteIdleEvents
							);
							SerializePlayerStringData
							(
								a_intfc, 
								cyclableEmoteIdleEvents[i].empty() ? 
								"NONE" :
								cyclableEmoteIdleEvents[i], 
								!SerializableDataType::kPlayerEmoteIdleEvents
							);
							DBG
							(
								"Player with FID 0x{:X} "
								"has cyclable emote idle event {} (size {}) in slot {}.",
								fid, cyclableEmoteIdleEvents[i], size, i
							);
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerEmoteIdleEvents)
					);
				}
				
				// TAKEN SHARED PERKS SET
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerTakenSharedPerks,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerTakenSharedPerks
						);
						// Write number of unlocked perks first, as this varies from save to save.
						const auto& takenSharedPerksSet = data->GetTakenSharedPerksSet();
						uint32_t numUnlockedPerks = takenSharedPerksSet.size();
						DBG
						(
							"Serialize TAKEN SHARED PERKS SET "
							"for 0x{:X}. Number of unlocked perks: {}.", 
							fid, numUnlockedPerks
						);
						SerializePlayerUInt32Data
						(
							a_intfc, 
							numUnlockedPerks, 
							!SerializableDataType::kPlayerTakenSharedPerks
						);

						// Serialize the FID of each unlocked shared perk next.
						if (numUnlockedPerks > 0)
						{
							for (auto iter = takenSharedPerksSet.begin(); 
								iter != takenSharedPerksSet.end(); 
								++iter)
							{
								const auto perk = *iter;
								DBG
								(
									"Serialize TAKEN SHARED PERK {} (0x{:X}) "
									"for player with FID 0x{:X}.",
									perk ? 
									perk->GetName() :
									"NONE", 
									perk ? perk->formID : 0, 
									fid
								);
								SerializePlayerUInt32Data
								(
									a_intfc, 
									perk ? perk->formID : 0,
									!SerializableDataType::kPlayerTakenSharedPerks
								);
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerTakenSharedPerks)
					);
				}

				// UNLOCKED PERKS LIST
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerUnlockedPerksList,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerUnlockedPerksList
						);
						// Write number of unlocked perks first, as this varies from save to save.
						const auto& unlockedPerksList = data->GetUnlockedPerksList();
						uint32_t numUnlockedPerks = unlockedPerksList.size();
						DBG
						(
							"Serialize UNLOCKED PERKS LIST "
							"for 0x{:X}. Number of unlocked perks: {}.", 
							fid, numUnlockedPerks
						);
						SerializePlayerUInt32Data
						(
							a_intfc, 
							numUnlockedPerks, 
							!SerializableDataType::kPlayerUnlockedPerksList
						);

						// Serialize the FID of each unlocked perk next.
						if (numUnlockedPerks > 0)
						{
							for (uint8_t j = 0; j < numUnlockedPerks; ++j)
							{
								DBG
								(
									"Serialize UNLOCKED PERK {} (0x{:X}) "
									"for player with FID 0x{:X}.",
									unlockedPerksList[j] ? 
									unlockedPerksList[j]->GetName() :
									"NONE", 
									unlockedPerksList[j] ? unlockedPerksList[j]->formID : 0, 
									fid
								);
								SerializePlayerUInt32Data
								(
									a_intfc, 
									unlockedPerksList[j] ? unlockedPerksList[j]->formID : 0,
									!SerializableDataType::kPlayerUnlockedPerksList
								);
							}
						}
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerUnlockedPerksList)
					);
				}

				// RACE MENU PRESET NAME
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerRaceMenuPresetName,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerRaceMenuPresetName
						);
						// Serialize string length and then the string's buffer 
						// for each saved emote idle event name.
						// +1 for the null terminator at the end of the string.
						// NOTE:
						// Had issues deserializing the empty string 
						// (reading memory beyond null terminator), so save as "NONE" instead.
						uint32_t size = data->raceMenuPresetName.length() * sizeof(char) + 1;
						SerializePlayerUInt32Data
						(
							a_intfc,
							data->raceMenuPresetName.empty() ? strlen("NONE") + 1 : size, 
							!SerializableDataType::kPlayerRaceMenuPresetName
						);
						SerializePlayerStringData
						(
							a_intfc, 
							data->raceMenuPresetName.empty() ?
							"NONE" :
							data->raceMenuPresetName, 
							!SerializableDataType::kPlayerRaceMenuPresetName
						);
						DBG
						(
							"Player with FID 0x{:X} "
							"has applied RaceMenu preset name {} (size {}).",
							fid, data->raceMenuPresetName, size
						);
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerRaceMenuPresetName)
					);
				}

				// CHARACTER CHOSEN RACE
				if (a_intfc->OpenRecord
				(
					!SerializableDataType::kPlayerCharacterChosenRace,
					!SerializableDataType::kSerializationVersion
				))
				{
					for (auto& [fid, data] : glob.serializablePlayerData)
					{
						SerializePlayerUInt32Data
						(
							a_intfc, fid, !SerializableDataType::kPlayerCharacterChosenRace
						);
						DBG
						(
							"Serialize CHOSEN RACE {} (0x{:X}, editor ID {}) "
							"for player with FID 0x{:X}.",
							data->chosenRace ? 
							data->chosenRace->GetName() :
							"NONE", 
							data->chosenRace ? data->chosenRace->formID : 0, 
							Util::GetEditorID(data->chosenRace),
							fid
						);
						SerializePlayerUInt32Data
						(
							a_intfc, 
							data->chosenRace ? data->chosenRace->formID : 0,
							!SerializableDataType::kPlayerCharacterChosenRace
						);
					}
				}
				else
				{
					ERR
					(
						"Could not open record of type {}.",
						TypeToString(!SerializableDataType::kPlayerCharacterChosenRace)
					);
				}
			}
		}

		void SerializePlayerFloatData
		(
			SKSE::SerializationInterface* a_intfc, 
			const float& a_data, 
			const uint32_t& a_recordType
		)
		{
			// Attempt to write float value to SKSE co-save.

			if (!a_intfc->WriteRecordData(a_data))
			{
				ERR
				(
					"Could not write FLOAT record data ({}), record type: {}.",
					a_data, TypeToString(a_recordType)
				);
			}
		}

		void SerializePlayerStringData
		(
			SKSE::SerializationInterface* a_intfc,
			const RE::BSFixedString& a_data,
			const uint32_t& a_recordType
		)
		{
			// Attempt to write data from fixed string's buffer to the SKSE co-save.

			// +1 for the null terminator at the end of the string.
			if (!a_intfc->WriteRecordData(a_data.data(), a_data.length() * sizeof(char) + 1))
			{
				ERR
				(
					"Could not write STRING record data ({}, size: {}), record type: {}.",
					a_data.data(), a_data.length() * sizeof(char), TypeToString(a_recordType)
				);
			}
		}

		void SerializePlayerUInt8Data
		(
			SKSE::SerializationInterface* a_intfc,
			const uint8_t& a_data,
			const uint32_t& a_recordType
		)
		{
			// Attempt to write an unsigned 8-bit integer to the SKSE co-save.

			if (!a_intfc->WriteRecordData(a_data))
			{
				ERR
				(
					"Could not write UINT8 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType)
				);
			}
		}

		void SerializePlayerUInt16Data
		(
			SKSE::SerializationInterface* a_intfc, 
			const uint16_t& a_data, 
			const uint32_t& a_recordType
		)
		{
			// Attempt to write an unsigned 16-bit integer to the SKSE co-save.

			if (!a_intfc->WriteRecordData(a_data))
			{
				ERR
				(
					"Could not write UINT16 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType)
				);
			}
		}

		void SerializePlayerUInt32Data
		(
			SKSE::SerializationInterface* a_intfc, 
			const uint32_t& a_data, 
			const uint32_t& a_recordType
		)
		{
			// Attempt to write an unsigned 32-bit integer to the SKSE co-save.

			if (!a_intfc->WriteRecordData(a_data))
			{
				ERR
				(
					"Could not write UINT32 record data (0x{:X}), record type: {}.",
					a_data, TypeToString(a_recordType)
				);
			}
		}

		void SetDefaultRetrievedData()
		{
			// Set default data to write to the SKSE co-save. 
			// Done when no data has been serialized yet 
			// or when the serialization interface is unavailable.

			DBG("SetDefaultRetrievedData.");
			RE::PlayerCharacter* p1 = RE::PlayerCharacter::GetSingleton();
			RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler || !p1)
			{
				return;
			}
			
			// Clear out current data before setting fresh data.
			if (!glob.serializablePlayerData.empty())
			{
				glob.serializablePlayerData.clear();
			}

			// Default data.
			constexpr size_t numSkills = (size_t)Skill::kTotal;
			std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal> copiedMagic{ };
			std::array<float, 3> hmsBasePointsList{ };
			std::array<float, 3> hmsIncList{ };
			std::array<RE::TESForm*, 8> hotkeyedFormsList{ };
			std::array<float, numSkills> skillBaseLvlList{ };
			std::array<float, numSkills> skillIncList{ };
			std::array<uint32_t, numSkills> skillLegendaryCountList{ };
			std::array<float, numSkills> skillXPList{ };

			// Default filled arrays.
			// Serialize the co-op player's current level as their base level.
			// Co-op actors start with 100.0 base HMS points, level 15 skills, 
			// and 0 skill XP, Legendary levels, and level/HMS point increases across the board.
			copiedMagic.fill(nullptr);
			hmsBasePointsList.fill(100.0f);
			hmsIncList.fill(0.0f);
			hotkeyedFormsList.fill(nullptr);
			skillBaseLvlList.fill(15.0f);
			skillIncList.fill(0.0f);
			skillLegendaryCountList.fill(0);
			skillXPList.fill(0.0f);

			// Set skill base AVs.
			skillBaseLvlList = Util::GetActorSkillLevels(p1);
			
#ifdef ALYSLC_DEBUG_MODE
			for (auto i = 0; i < skillBaseLvlList.size(); ++i)
			{
				auto currentSkill = static_cast<Skill>(i);
				const auto iter = glob.SKILL_TO_AV_MAP.find(currentSkill);
				if (iter != glob.SKILL_TO_AV_MAP.end())
				{
					auto currentAV = iter->second;
					DBG
					(
						"P1's {} skill base level: {}.", 
						Util::GetActorValueName(currentAV), skillBaseLvlList[i]
					);
				}
			}
#endif

			// Set initial skill XP.
			auto currentAV = RE::ActorValue::kNone;
			for (auto i = 0; i < Skill::kTotal; ++i)
			{
				currentAV = glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
				skillXPList[i] = p1->skills->data->skills[i].xp;
			}

			if (!ALYSLC::EnderalCompat::g_installed) 
			{
				// Set base P1 HMS values to their starting values if Enderal is not installed.
				// All perks are also cleared, meaning HMS values must be re-assigned 
				// along with perks by leveling up again through the Stats Menu.
				p1->SetBaseActorValue(RE::ActorValue::kHealth, 100.0f);
				p1->SetBaseActorValue(RE::ActorValue::kMagicka, 100.0f);
				p1->SetBaseActorValue(RE::ActorValue::kStamina, 100.0f);
				p1->SetActorValue(RE::ActorValue::kHealth, 100.0f);
				p1->SetActorValue(RE::ActorValue::kMagicka, 100.0f);
				p1->SetActorValue(RE::ActorValue::kStamina, 100.0f);
			}
			
			// Set current level for all players to P1's current level.
			// First saved level set to 0.
			auto p1Level = p1->GetLevel();
			// Player 1's character ID is always 0.
			uint32_t playerCharacterID = 0;
			// Insert P1 first.
			glob.serializablePlayerData.insert
			(
				{
					p1->formID, 
					std::make_unique<SerializablePlayerData>
					(
						copiedMagic, 
						GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS,
						std::vector<RE::TESForm*>{ !EquipIndex::kTotal, nullptr },
						std::vector<RE::TESForm*>(),
						p1->skills->data->xp, 
						playerCharacterID,
						0,
						0,
						p1Level,
						0,
						0,
						0, 
						hmsBasePointsList,
						hmsIncList,
						hotkeyedFormsList,
						skillLegendaryCountList,
						skillBaseLvlList,
						skillIncList,
						skillXPList, 
						std::vector<RE::BGSPerk*>(),
						std::vector<RE::BGSPerk*>(),
						"NONE",
						p1->charGenRace
					) 
				}
			);

			// Insert co-op companions.
			std::array<RE::Actor*, (size_t)(ALYSLC_COMPANION_CHARACTERS_COUNT)> coopPlayers
			{
				nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
			};
			// Inserted in order of actor base's editor ID trailing index:
			// 1. NPC with '__CoopCharacter1' as its actor base editor ID.
			// 2. NPC with '__CoopCharacter2' as its actor base editor ID.
			// 3. NPC with '__CoopCharacter3' as its actor base editor ID.
			// 4. NPC with '__CoopCharacter4' as its actor base editor ID.
			// 5. NPC with '__CoopCharacter5' as its actor base editor ID.
			// 6. NPC with '__CoopCharacter6' as its actor base editor ID.
			// 7. NPC with '__CoopCharacter7' as its actor base editor ID.
			// 8. NPC with '__CoopCharacter8' as its actor base editor ID.
			// 9. NPC with '__CoopCharacter9' as its actor base editor ID.
			for (const auto actor : dataHandler->GetFormArray<RE::Actor>())
			{
				DBG
				(
					"Actor {} (0x{:X}",
					actor ? actor->GetName() : "NONE", actor ? actor->formID : 0xDEAD
				); 
			}

			for (auto i = 0; i < coopPlayers.size(); ++i) 
			{
				auto fid = dataHandler->LookupFormID
				(
					GlobalCoopData::PLAYER_CHARACTER_FIDS[i + 1],
					GlobalCoopData::PLUGIN_NAME
				);
				DBG
				(
					"Co-op character {}: 0x{:X} -> 0x{:X}.",
					i + 1,
					GlobalCoopData::PLAYER_CHARACTER_FIDS[i + 1],
					fid
				); 
				coopPlayers[i] = dataHandler->LookupForm<RE::Actor>
				(
					GlobalCoopData::PLAYER_CHARACTER_FIDS[i + 1],
					GlobalCoopData::PLUGIN_NAME
				);
				if (coopPlayers[i]) 
				{
					auto fid = coopPlayers[i]->formID;
					if (fid)
					{
						// Set default cleared data first.
						hmsBasePointsList.fill(100.0f);
						hmsIncList.fill(0.0f);
						hotkeyedFormsList.fill(nullptr);
						skillLegendaryCountList.fill(0);
						skillBaseLvlList.fill(15.0f);
						skillIncList.fill(0.0f);
						// No skill XP to start, unlike for P1.
						skillXPList.fill(0.0f);
							
						// Set initial skill base AVs.
						auto skillBaseLvlList = Util::GetActorSkillLevels(coopPlayers[i]);
						// Companion player's character IDs are based on 
						// their actor base's editor ID trailing index.
						// 1 = NPC with '__CoopCharacter1' as its actor base editor ID.
						// 2 = NPC with '__CoopCharacter2' as its actor base editor ID.
						// 3 = NPC with '__CoopCharacter3' as its actor base editor ID.
						playerCharacterID = i + 1;
						glob.serializablePlayerData.insert
						(
							{ 
								fid,
								std::make_unique<SerializablePlayerData>
								(
									copiedMagic,
									GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS,
									std::vector<RE::TESForm*>{ !EquipIndex::kTotal, nullptr },
									std::vector<RE::TESForm*>(),
									p1->skills->data->xp,
									playerCharacterID,
									0,
									0,
									p1Level,
									0,
									0,
									0,
									hmsBasePointsList,
									hmsIncList,
									hotkeyedFormsList,
									skillLegendaryCountList,
									skillBaseLvlList,
									skillIncList,
									skillXPList,
									std::vector<RE::BGSPerk*>(),
									std::vector<RE::BGSPerk*>(),
									"NONE",
									coopPlayers[i]->GetRace()
								) 
							}
						);
					}
					else
					{
						ERR("Could not get __CoopCharacter{}'s form ID.", i + 1);
					}
				}
				else
				{
					ERR
					(
						"Could not get __CoopCharacter{}. "
						"Game will likely crash when summoning other players.",
						i + 1
					);
				}
			}

			DBG("Default data set and ready to serialize!");
		}

		// Credits to po3 for the decode function from here:
		// https://github.com/powerof3/PapyrusExtenderSSE/blob/master/src/Serialization/Manager.cpp#L10
		std::string TypeToString(uint32_t a_type)
		{
			constexpr std::size_t SIZE = sizeof(uint32_t);

			std::string sig{ };
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

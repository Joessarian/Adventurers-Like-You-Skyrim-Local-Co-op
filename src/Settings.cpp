#include "Settings.h"
#include <GlobalCoopData.h>
#include <Compatibility.h>
#include <fmt/xchar.h>

namespace ALYSLC
{
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	void Settings::ImportAllSettings()
	{
		// Import all MCM settings.

		CSimpleIniA ini{ };
		ini.SetUnicode();

		// Import defaults.
		SI_Error err = SI_OK;
		if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
		{
			if (err = ini.LoadFile(defaultFilePathEnderal.c_str()); err != SI_OK) 
			{
				SPDLOG_ERROR
				(
					"[Settings] ERR ({}): ImportAllSettings: "
					"Could not load default Enderal settings file '{}'. "
					"Ensure the file exists at that location. "
					"About to import the user's modifiable settings.", 
					err, defaultFilePathEnderal.string()
				);
			}
			else
			{
				SPDLOG_INFO
				(
					"[Settings] ImportAllSettings: "
					"Successfully opened default Enderal settings configuration file '{}'.",
					defaultFilePathEnderal.string()
				);
			}
			
		}
		else
		{
			if (err = ini.LoadFile(defaultFilePathSSE.c_str()); err != SI_OK)
			{
				SPDLOG_ERROR
				(
					"[Settings] ERR ({}): ImportAllSettings: "
					"Could not load default Skyrim SE settings file '{}'. "
					"Ensure the file exists at that location. "
					"About to import the user's modifiable settings.", 
					err, defaultFilePathEnderal.string()
				);
			}
			else
			{
				SPDLOG_INFO
				(
					"[Settings] ImportAllSettings: "
					"Successfully opened default Skyrim settings configuration file '{}'.", 
					defaultFilePathSSE.string()
				);
			}
		}

		// Are the default binds error-free?
		bool defBindsSucc = true;
		if (err == SI_OK) 
		{
			// General, then player settings, then binds.
			ImportGeneralSettings(ini);
			ImportPlayerSpecificSettings(ini);
			defBindsSucc = ImportBinds(ini);
		}

		ini.Reset();

		// Import player-modified settings.
		CSimpleIniA ini2{ };
		ini2.SetUnicode();
		if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
		{
			if (err = ini2.LoadFile(userSettingsFilePathEnderal.c_str()); err != SI_OK)
			{
				SPDLOG_ERROR
				(
					"[Settings] ERR: ({}): ImportAllSettings: "
					"Could not load Enderal player settings config file '{}'. "
					"If you've made changes to the default settings, "
					"ensure the file exists at that location. "
					"Now using the plugin's hardcoded default settings.", 
					err, userSettingsFilePathEnderal.string()
				);
			}
			else
			{
				SPDLOG_INFO
				(
					"[Settings] ImportAllSettings: "
					"Successfully opened user-modified Enderal settings configuration file '{}'.",
					userSettingsFilePathEnderal.string()
				);
			}
		}
		else
		{
			if (err = ini2.LoadFile(userSettingsFilePathSSE.c_str()); err != SI_OK)
			{
				SPDLOG_ERROR
				(
					"[Settings] ERR: ({}): ImportAllSettings: "
					"Could not load Skyrim SE player settings config file '{}'. "
					"If you've made changes to the default settings, "
					"ensure the file exists at that location. "
					"Now using the plugin's hardcoded default settings.", 
					err, userSettingsFilePathSSE.string()
				);
			}
			else
			{
				SPDLOG_INFO
				(
					"[Settings] ImportAllSettings: "
					"Successfully opened user-modified Skyrim settings configuration file '{}'.",
					userSettingsFilePathSSE.string()
				);
			}
		}

		// Are the player-customized binds error-free?
		bool customBindsSucc = true;
		if (err == SI_OK)
		{
			// General, then player settings, then binds.
			ImportGeneralSettings(ini2);
			ImportPlayerSpecificSettings(ini2);
			customBindsSucc = ImportBinds(ini2);
		}

		ini2.Reset();

		// Inform the player of invalid binds.
		if (!defBindsSucc || !customBindsSucc) 
		{
			RE::DebugMessageBox
			(
				fmt::format
				(
					"[ALYSLC] Invalid binds found:\nDefault settings.ini: "
					"{}\n{}: {}\nTroubleshooting info messages are in the 'ALYSLC.log' file "
					"and have the [BINDS] tag.\nAll invalid binds were disabled.", 
					!defBindsSucc ? "[Invalid]" : "[Valid]", 
					ALYSLC::EnderalCompat::g_enderalSSEInstalled ? 
					"ALYSLC Enderal.ini" : 
					"ALYSLC.ini",
					!customBindsSucc ? "[Invalid]" : "[Valid]"
				).c_str()
			);
		}

		if (glob.globalDataInit && glob.coopSessionActive)
		{
			// Refresh managers to apply changes to most settings.
			for (const auto& p : glob.coopPlayers) 
			{
				if (!p || !p->isActive)
				{
					continue;
				}

				p->taskRunner->AddTask
				(
					[&p]()
					{ 
						p->SetCoopPlayerFlags();
						p->RefreshPlayerManagersTask(); 
					}
				);
			}
		}

		SPDLOG_DEBUG("[Settings] ImportAllSettings: Done importing all settings.");
	}

	bool Settings::ImportBinds(CSimpleIniA& a_ini)
	{
		// Import binds for each player.

		bool allBindsValid = true;
		if (!glob.paInfoHolder || !glob.paInfoHolder.get()) 
		{
			// Binds are not invalid, but are not importable before global co-op data is set.
			return true;
		}

		auto& paInfo = glob.paInfoHolder;
		// Fill with default values.
		paInfo->playerPAParamsLists.fill(paInfo->defPAParamsList);

		// Composing InputActions for each player action.
		// Read in binds and then break down into only composing inputs afterward.
		std::array<std::vector<InputAction>, (size_t)(InputAction::kActionTotal)> 
		tempComposingActionsLists{ };
		tempComposingActionsLists.fill(std::vector<InputAction>());
		// For troubleshooting purposes.
		// Binds in this set are invalid and will be disabled.
		std::set<InputAction> actionsWithBindErrors{ };
		// Prevent multiple assignments of the same action per bind by using a set.
		std::set<InputAction> composingActions{ };
		// INI section name.
		std::string sectionName{ "" };
		// Input action name used as a part of the bind's INI key.
		std::string actionName{ "NONE" };
		// Current action to import the bind for.
		InputAction action = InputAction::kNone;
		// Index of the action offset by the first action's index.
		// Used to index into player bind structures.
		uint32_t bindIndex = 0;
		// Inputs can be pressed in any order for this bind.
		bool anyInputOrder = false;
		// Must quickly tap the bind at least twice to trigger the action.
		bool consecTap = false;
		// Action's bind must be held for a minimum amount of time before triggering.
		bool minHoldTime = false;
		// Go through each player's binds.
		for (auto pIndex = 0; pIndex < ALYSLC_MAX_PLAYER_COUNT; ++pIndex) 
		{
			// NOTE: 
			// These structures will already be populated by the default data assigned 
			// in the PA Info Holder constructor.
			auto& playerPAParams = paInfo->playerPAParamsLists[pIndex];
			// Have the default composing actions list for all actions to import from 
			// if the player has not changed a particular bind.
			const auto& defPACompActionsList = paInfo->DEF_PA_COMP_INPUT_ACTIONS_LIST;
			// Get the INI section name based on the player index.
			sectionName = fmt::format("Player{}", pIndex + 1);
			// Go though all player actions.
			for (auto actionIndex = !InputAction::kFirstAction; 
				actionIndex <= !InputAction::kLastAction; 
				++actionIndex) 
			{
				bindIndex = actionIndex - !InputAction::kFirstAction;
				action = static_cast<InputAction>(actionIndex);
				actionName = InputActionToString(action);
				// PA parameter to modify.
				auto& param = playerPAParams[bindIndex];

				//==============
				// [Bind Flags]:
				//==============
				
				// Get defaults as a backup if the player has not made changes to these settings.
				anyInputOrder = param.triggerFlags.all
				(
					TriggerFlag::kDoNotUseCompActionsOrdering
				);
				consecTap = param.perfType == PerfType::kOnConsecTap;
				minHoldTime = param.triggerFlags.all
				(
					TriggerFlag::kMinHoldTime
				);

				// Read in the 2-3 modifiable bind flags.
				ReadBoolSetting
				(
					a_ini, 
					sectionName.data(), 
					fmt::format("bAction{}AnyInputOrder", actionName).c_str(), 
					anyInputOrder
				);
				// The bind's default perform type to compare the player's changes to.
				const auto& defPerfType = paInfo->defPAParamsList[bindIndex].perfType;
				// Only these default perf types support a switch to consec tap. 
				// Otherwise, the setting will not exist for this bind.
				if (defPerfType == PerfType::kOnRelease || 
					defPerfType == PerfType::kOnPress || 
					defPerfType == PerfType::kOnConsecTap) 
				{
					ReadBoolSetting
					(
						a_ini,
						sectionName.data(),
						fmt::format("bAction{}ConsecTap", actionName).c_str(), 
						consecTap
					);
				}

				ReadBoolSetting
				(
					a_ini, 
					sectionName.data(),
					fmt::format("bAction{}MinHoldTime", actionName).c_str(), 
					minHoldTime
				);

				// Can't have both min hold time and consecutive tap flags set, 
				// since their input-hold time conditions oppose one another.
				// Add to invalid binds list and disable.
				if (minHoldTime && consecTap) 
				{
					param.perfType = PerfType::kDisabled;
					actionsWithBindErrors.insert(action);
					SPDLOG_ERROR
					(
						"[BINDS] ERR: P{}: [INVALID]: "
						"{} has both the 'Minimum Hold Time' and 'On Consecutive Tap' flags set. "
						"Since these flags are mutually exclusive, the bind will be disabled.",
						pIndex + 1, action
					);
				}
				else
				{
					// If any input order is supported, set trigger flag, reset otherwise.
					if (anyInputOrder)
					{
						param.triggerFlags.set
						(
							TriggerFlag::kDoNotUseCompActionsOrdering
						);
					}
					else
					{
						param.triggerFlags.reset
						(
							TriggerFlag::kDoNotUseCompActionsOrdering
						);
					}

					if (consecTap)
					{
						// Set new perform type.
						param.perfType = PerfType::kOnConsecTap;
					}
					else if (defPerfType == PerfType::kOnConsecTap)
					{
						// If the default bind was set to consec tap, 
						// but the custom one unsets this flag,
						// set the new bind perform type as 'OnRelease'.
						// Otherwise, do not change the perf type for this bind, 
						// which will use the default bind's perform type.
						param.perfType = PerfType::kOnRelease;
					}

					if (minHoldTime)
					{
						// Actions with a minimum hold time must also ignore conflicting actions 
						// while the bind is held.
						// Otherwise, they won't trigger from being blocked by conflicting actions.
						param.triggerFlags.set
						(
							TriggerFlag::kMinHoldTime, TriggerFlag::kIgnoreConflictingActions
						);

						// Switch perform type to 'OnPress' so that the action triggers 
						// right after the bind is held for longer than the hold threshold,
						// instead of on release of the bind or after tapping twice.
						if (defPerfType == PerfType::kOnConsecTap || 
							defPerfType == PerfType::kOnRelease) 
						{
							param.perfType = PerfType::kOnPress;
						}
					}
					else
					{
						// Remove hold time flag and keep default perform type.
						param.triggerFlags.reset(TriggerFlag::kMinHoldTime);
						// If the default bind did not have a minimum hold time 
						// and did not already ignore conflicting actions, reset the flag here.
						if (param.triggerFlags.none
							(
								TriggerFlag::kMinHoldTime, TriggerFlag::kIgnoreConflictingActions
							)) 
						{
							param.triggerFlags.reset(TriggerFlag::kIgnoreConflictingActions);
						}
					}
				}

				//==========================
				// [Composing InputActions]:
				//==========================

				// Current composing action.
				InputAction compAction{ InputAction::kNone };
				// Is the composing action enabled at all?
				bool composingActionEnabled = false;
				// Has the composing action already been assigned?
				// Can't assign the same composing action twice in the same bind.
				bool alreadyAssigned = false;
				// Composing action slot (1 - 4) which can be assigned an InputAction.
				uint8_t compActionSlot = 1;
				// Assigned InputAction index (NOT offset from first action).
				uint32_t assignedInputActionIndex = !InputAction::kNone;
				// Clear out the composing actions lists first.
				tempComposingActionsLists[bindIndex].clear();
				composingActions.clear();
				do
				{
					// Check if the composing action slot and the assigned input action index
					// were marked as modified.
					// Get the composing action's enabled state if it exists.
					bool slotModified = ReadBoolSetting
					(
						a_ini, 
						sectionName.data(), 
						fmt::format("bAction{}{}", actionName, compActionSlot).c_str(),
						composingActionEnabled
					);
					// Set perf type to disabled if this is the first slot.
					// Nothing else to assign afterward.
					if (compActionSlot == 1 && slotModified && !composingActionEnabled)
					{
						param.perfType = PerfType::kDisabled;
						SPDLOG_DEBUG
						(
							"[BINDS] P{}: {}'s first composing action was disabled. "
							"Disabling the bind. "
							"Binds that depend on this bind will not be affected.",
							pIndex + 1, actionName
						);
						// NOTE:
						// Treat the first assigned input action as enabled
						// to continue checking subsequent slots, which, if enabled,
						// will have their actions added to the composing input actions list
						// for this disabled bind.
						// This is done to ensure these disabled actions 
						// can act as composing actions for other enabled binds.
						composingActionEnabled= true;
					}

					// Get the composing action's assigned input action index if it exists.
					bool slotInputActionIndexModified = ReadUInt32Setting
					(
						a_ini, 
						sectionName.data(),
						fmt::format("uAction{}{}", actionName, compActionSlot).c_str(), 
						assignedInputActionIndex, 
						!InputAction::kNone,
						!InputAction::kTotal
					);
					if (slotModified && slotInputActionIndexModified) 
					{
						// Slot was toggled on/off by the player 
						// and its assigned InputAction was also modified.
						// Otherwise, skip the slot since it's disabled.
						if (composingActionEnabled)
						{
							// Not zero-indexed when read in from .ini, 
							// since players indicate no assigned action with 0 (None).
							// Subtract 1 to get enum index.
							--assignedInputActionIndex;
							// No composing action assigned if outside the valid InputAction range.
							compAction = 
							(
								assignedInputActionIndex >= !InputAction::kFirst &&
								assignedInputActionIndex <= !InputAction::kLastAction ? 
								static_cast<InputAction>(assignedInputActionIndex) : 
								InputAction::kNone
							);
							// If the composing action was already assigned before, skip it.
							alreadyAssigned = 
							(
								compAction != InputAction::kNone &&
								!composingActions.empty() && 
								composingActions.contains(compAction)
							);

							if (assignedInputActionIndex != !InputAction::kNone && 
								assignedInputActionIndex <= !InputAction::kLastAction && 
								!alreadyAssigned)
							{
								// Assigned a new, valid InputAction.
								tempComposingActionsLists[bindIndex].emplace_back(compAction);
								composingActions.emplace(compAction);
							}
							else if (assignedInputActionIndex > !InputAction::kLastAction)
							{
								SPDLOG_ERROR
								(
									"[BINDS] ERR: P{}: [INVALID]: "
									"{} -> composing action #{} is enabled "
									"but was not assigned a valid input action ({}). "
									"Disabling the bind.",
									pIndex + 1,
									actionName, 
									compActionSlot,
									assignedInputActionIndex
								);
								// Invalid action, disable and add bind to invalid list.
								param.perfType = PerfType::kDisabled;
								actionsWithBindErrors.insert(action);
							}

							SPDLOG_DEBUG
							(
								"[Settings] ImportBinds: P{}: "
								"Bind for action {}: slot: {}, enabled: {}, "
								"already assigned: {}, assigned input action: {}.", 
								pIndex + 1,
								action, 
								compActionSlot,
								composingActionEnabled,
								alreadyAssigned,
								assignedInputActionIndex
							);
						}
					}
					else if (slotModified && !slotInputActionIndexModified)
					{
						// Slot was toggled on/off by the player 
						// but the assigned InputAction was not modified.
						if (composingActionEnabled) 
						{
							// Shift down to index into the default composing InputActions list.
							if (compActionSlot - 1 < defPACompActionsList[bindIndex].size())
							{
								// Not modified, so assign the default InputAction 
								// if the slot was assigned one and it wasn't already assigned.
								compAction = defPACompActionsList[bindIndex][compActionSlot - 1];
								alreadyAssigned = 
								(
									!composingActions.empty() && 
									composingActions.contains(compAction)
								);
								if (!alreadyAssigned && compAction != InputAction::kNone)
								{
									tempComposingActionsLists[bindIndex].emplace_back(compAction);
									composingActions.emplace(compAction);
								}
							}
						}
					}
					else if (!slotModified && slotInputActionIndexModified)
					{
						// Slot was not toggled on/off by the player, 
						// but the assigned InputAction was modified.
						
						// Was the default composing action in this slot defined?
						// If not, since the player changed a composing action 
						// in a slot that is not enabled,
						// the assigned composing action still will not be enabled.
						// Shift down to index into default composing InputActions list.
						composingActionEnabled = 
						(
							compActionSlot - 1 < defPACompActionsList[bindIndex].size()
						);
						// Not zero-indexed when read in from .ini, 
						// since players indicate no assigned action with 0 (None).
						// Subtract 1 to get enum index.
						--assignedInputActionIndex;
						// No composing action assigned if outside the valid InputAction range.
						compAction = 
						(
							assignedInputActionIndex >= !InputAction::kFirst && 
							assignedInputActionIndex <= !InputAction::kLastAction ? 
							static_cast<InputAction>(assignedInputActionIndex) : 
							InputAction::kNone
						);
						
						// Was enabled by default.
						// Otherwise, the slot's action will be skipped since the slot is disabled, 
						// despite being assigned an action.
						if (composingActionEnabled)
						{
							alreadyAssigned = 
							(
								compAction != InputAction::kNone && 
								!composingActions.empty() && 
								composingActions.contains(compAction)
							);
							if (assignedInputActionIndex != !InputAction::kNone && 
								assignedInputActionIndex <= !InputAction::kLastAction && 
								!alreadyAssigned)
							{
								// Assigned a new, valid InputAction.
								tempComposingActionsLists[bindIndex].emplace_back(compAction);
								composingActions.emplace(compAction);
							}
							else if (assignedInputActionIndex > !InputAction::kLastAction)
							{
								SPDLOG_ERROR
								(
									"[BINDS] ERR: P{}: [INVALID]: "
									"{} -> composing action #{} is enabled "
									"but was not assigned a valid input action ({}). "
									"Disabling the bind.",
									pIndex + 1,
									actionName,
									compActionSlot, 
									assignedInputActionIndex
								);
								// Invalid action, disable and add bind to invalid list.
								param.perfType = PerfType::kDisabled;
								actionsWithBindErrors.insert(action);
							}
						}
					}
					else
					{
						// Slot was not toggled on/off 
						// and its assigned InputAction was also not modified.

						// Retrieve default-assigned action in this slot, if any.
						// Shift down to index into default composing InputActions list.
						composingActionEnabled = 
						(
							compActionSlot - 1 < defPACompActionsList[bindIndex].size()
						);
						if (composingActionEnabled)
						{
							// Not modified, so assign the default InputAction
							// if the slot was assigned one and it wasn't already assigned.
							compAction = defPACompActionsList[bindIndex][compActionSlot - 1];
							alreadyAssigned = 
							(
								!composingActions.empty() && composingActions.contains(compAction)
							);
							if (!alreadyAssigned && compAction != InputAction::kNone)
							{
								tempComposingActionsLists[bindIndex].emplace_back(compAction);
								composingActions.emplace(compAction);
							}
						}
					}

					// Move to the next slot.
					++compActionSlot;

					// Loop while the current slot's action is enabled 
					// and the current slot is not past the last slot.
					// NOTE: 
					// Discontinuous action assignment, 
					// e.g. 
					// Slot 1: enabled with valid action, 
					// Slot 2: disabled,
					// Slot 3: enabled with valid action,
					// is not supported, as only Slot 1's InputAction will be assigned.
					// Any actions assigned in enabled slots 
					// after the first disabled slot are ignored.
				} while (composingActionEnabled && 
					     compActionSlot <= GlobalCoopData::MAX_ACTIONS_PER_BIND);

				// Notify the player of their invalid binds.
				if (actionsWithBindErrors.contains(action))
				{
					SPDLOG_ERROR
					(
						"[BINDS] ERR: P{}: [INVALID]: Action {}'s bind will be disabled. "
						"Please ensure the composing actions for this bind "
						"were assigned valid values.", 
						pIndex + 1, actionName
					);
				}
			}

			//====================
			// [Composing Inputs]:
			//====================

			// Break down all binds' composing InputAction lists into composing input lists.
			uint8_t recursionDepth = 0;
			uint32_t numComposingPlayerActions = 0;
			// Set of composing inputs for the current action.
			std::set<InputAction> composingInputsSet{ };
			for (auto actionIndex = !InputAction::kFirstAction; 
				actionIndex <= !InputAction::kLastAction;
				++actionIndex)
			{
				// Get action to decompose.
				action = static_cast<InputAction>(actionIndex);
				// Get bind index to index info PA params list.
				bindIndex = actionIndex - !InputAction::kFirstAction;
				// PA parameter to modify.
				auto& param = playerPAParams[bindIndex];
				// Reset outparam recursion and composing player actions count.
				recursionDepth = numComposingPlayerActions = 0;
				// Clear before getting all inputs.
				composingInputsSet.clear();
				// Get and set composing inputs for the current action.
				param.composingInputs = paInfo->GetComposingInputs
				(
					action, 
					tempComposingActionsLists, 
					composingInputsSet,
					numComposingPlayerActions,
					recursionDepth
				);

				// Recursion depth exceeded, so disable the bind.
				if (recursionDepth >= uMaxComposingInputsCheckRecursionDepth)
				{
					SPDLOG_ERROR
					(
						"[BINDS] P{}: [INVALID] action {}'s bind may be circular "
						"(max of {} assigned inputs exceeded). "
						"Please ensure the composing actions for this bind "
						"are not themselves composed of this bind's actions. Disabling the bind.",
						pIndex + 1, action, Settings::uMaxComposingInputsCheckRecursionDepth
					);
					param.perfType = PerfType::kDisabled;
					actionsWithBindErrors.insert(action);
					continue;
				}

				// Lastly, set input mask and copy over composing player actions count.
				param.inputMask = paInfo->GetInputMask(param.composingInputs);
				param.composingPlayerActionsCount = numComposingPlayerActions;
				SPDLOG_DEBUG
				(
					"[Settings] ImportBinds: P{}: "
					"Bind for action {} has {} composing inputs and {} composing actions.", 
					pIndex + 1,
					action, 
					param.composingInputs.size(),
					param.composingPlayerActionsCount
				);
			}

			// Check if all binds are still valid after assignment to the params list.
			allBindsValid &= actionsWithBindErrors.empty();
			// Clear before moving on to the next player's binds.
			actionsWithBindErrors.clear();
		}

		SPDLOG_DEBUG("[Settings] ImportBinds: All binds valid: {}.", allBindsValid);
		return allBindsValid;
	}

	void Settings::ImportGeneralSettings(CSimpleIniA& a_ini)
	{
		// Import settings not linked with a specific player.
		
		// Camera
		ReadBoolSetting(a_ini, "Camera", "bAutoRotateCamPitch", bAutoRotateCamPitch);
		ReadBoolSetting(a_ini, "Camera", "bAutoRotateCamYaw", bAutoRotateCamYaw);
		ReadBoolSetting(a_ini, "Camera", "bAutoAdjustCamZoom", bAutoAdjustCamZoom);
		ReadBoolSetting(a_ini, "Camera", "bCamCollisions", bCamCollisions);
		ReadBoolSetting(a_ini, "Camera", "bFadeLargerObstructions", bFadeLargerObstructions);
		ReadBoolSetting(a_ini, "Camera", "bFadeObstructions", bFadeObstructions);
		ReadBoolSetting(a_ini, "Camera", "bFocalPlayerMode", bFocalPlayerMode);
		ReadBoolSetting(a_ini, "Camera", "bOriginPointSmoothing", bOriginPointSmoothing);
		ReadBoolSetting(a_ini, "Camera", "bProximityFadeOnly", bProximityFadeOnly);
		ReadBoolSetting(a_ini, "Camera", "bRemoveExteriorOcclusion", bRemoveExteriorOcclusion);
		ReadBoolSetting(a_ini, "Camera", "bRemoveInteriorOcclusion", bRemoveInteriorOcclusion);
		ReadBoolSetting(a_ini, "Camera", "bTargetPosSmoothing", bTargetPosSmoothing);
		ReadFloatSetting(a_ini, "Camera", "fCamExteriorFOV", fCamExteriorFOV, 0.0f, 180.0f);
		ReadFloatSetting(a_ini, "Camera", "fCamInteriorFOV", fCamInteriorFOV, 0.0f, 180.0f);
		ReadUInt32Setting
		(
			a_ini, 
			"Camera",
			"uAutoRotateCriteria", 
			uAutoRotateCriteria,
			0,
			!CamAutoRotateCriteria::kTotal - 1
		);
		ReadUInt32Setting
		(
			a_ini, 
			"Camera",
			"uLockOnAssistance",
			uLockOnAssistance, 
			0, 
			!CamLockOnAssistanceLevel::kTotal - 1
		);

		// Cheats
		ReadBoolSetting(a_ini, "Cheats", "bAddAnimEventSkillPerks", bAddAnimEventSkillPerks);
		ReadBoolSetting(a_ini, "Cheats", "bInfiniteCarryweight", bInfiniteCarryweight);
		ReadBoolSetting(a_ini, "Cheats", "bPreventFallDamage", bPreventFallDamage);
		ReadBoolSetting(a_ini, "Cheats", "bSpeedUpDodgeAnimations", bSpeedUpDodgeAnimations);
		ReadBoolSetting(a_ini, "Cheats", "bSpeedUpEquipAnimations", bSpeedUpEquipAnimations);

		// Emote Idles
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle1", sEmoteIdlesList[0]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle2", sEmoteIdlesList[1]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle3", sEmoteIdlesList[2]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle4", sEmoteIdlesList[3]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle5", sEmoteIdlesList[4]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle6", sEmoteIdlesList[5]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle7", sEmoteIdlesList[6]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle8", sEmoteIdlesList[7]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle9", sEmoteIdlesList[8]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle10", sEmoteIdlesList[9]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle11", sEmoteIdlesList[10]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle12", sEmoteIdlesList[11]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle13", sEmoteIdlesList[12]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle14", sEmoteIdlesList[13]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle15", sEmoteIdlesList[14]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle16", sEmoteIdlesList[15]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle17", sEmoteIdlesList[16]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle18", sEmoteIdlesList[17]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle19", sEmoteIdlesList[18]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle20", sEmoteIdlesList[19]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle21", sEmoteIdlesList[20]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle22", sEmoteIdlesList[21]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle23", sEmoteIdlesList[22]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle24", sEmoteIdlesList[23]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle25", sEmoteIdlesList[24]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle26", sEmoteIdlesList[25]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle27", sEmoteIdlesList[26]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle28", sEmoteIdlesList[27]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle29", sEmoteIdlesList[28]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle30", sEmoteIdlesList[29]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle31", sEmoteIdlesList[30]);
		ReadStringSetting(a_ini, "EmoteIdles", "sEmoteIdle32", sEmoteIdlesList[31]);

		// Extra Mechanics
		ReadBoolSetting
		(
			a_ini, 
			"ExtraMechanics",
			"bAimPitchAffectsFlopTrajectory",
			bAimPitchAffectsFlopTrajectory
		);
		ReadBoolSetting
		(
			a_ini, 
			"ExtraMechanics",
			"bAutoGrabNearbyLootableObjectsOnHold",
			bAutoGrabNearbyLootableObjectsOnHold
		);
		ReadBoolSetting(a_ini, "ExtraMechanics", "bCanGrabActors", bCanGrabActors);
		ReadBoolSetting(a_ini, "ExtraMechanics", "bCanGrabOtherPlayers", bCanGrabOtherPlayers);
		ReadBoolSetting(a_ini, "ExtraMechanics", "bEnableArmsRotation", bEnableArmsRotation);
		ReadBoolSetting(a_ini, "ExtraMechanics", "bEnableFlopping", bEnableFlopping);
		ReadBoolSetting
		(
			a_ini, "ExtraMechanics", "bEnableObjectManipulation", bEnableObjectManipulation
		);
		ReadBoolSetting
		(
			a_ini,
			"ExtraMechanics", 
			"bGrabIncomingProjectilesOnHold",
			bGrabIncomingProjectilesOnHold
		);
		ReadBoolSetting
		(
			a_ini, "ExtraMechanics", "bRemoveGrabbedActorAutoGetUp", bRemoveGrabbedActorAutoGetUp
		);
		ReadBoolSetting
		(
			a_ini, 
			"ExtraMechanics", 
			"bSimpleThrownObjectCollisionCheck",
			bSimpleThrownObjectCollisionCheck
		);
		ReadBoolSetting
		(
			a_ini, "ExtraMechanics", "bSlapsStopAttacksAndBlocking", bSlapsStopAttacksAndBlocking
		);
		ReadBoolSetting
		(
			a_ini, "ExtraMechanics", "bToggleGrabbedRefrCollisions", bToggleGrabbedRefrCollisions
		);
		ReadFloatSetting
		(
			a_ini, 
			"ExtraMechanics", 
			"fArmCollisionForceMult", 
			fArmCollisionForceMult, 
			0.0f, 
			10.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"ExtraMechanics", 
			"fBaseMaxThrownObjectReleaseSpeed", 
			fBaseMaxThrownObjectReleaseSpeed, 
			0.0f, 
			50000.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"ExtraMechanics", 
			"fFrameWindowToGrabIncomingProjectiles", 
			fFrameWindowToGrabIncomingProjectiles, 
			0.0f, 
			30.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"ExtraMechanics", 
			"fSecsBeforeAutoGrabbingNextLootableObject", 
			fSecsBeforeAutoGrabbingNextLootableObject, 
			0.0f, 
			5.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"ExtraMechanics", 
			"fSecsToReleaseObjectsAtMaxSpeed", 
			fSecsToReleaseObjectsAtMaxSpeed, 
			0.01f, 
			5.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"ExtraMechanics", 
			"fSlapKnockdownForearmSpeedThresholdMult", 
			fSlapKnockdownForearmSpeedThresholdMult, 
			0.0f, 
			5.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"ExtraMechanics", 
			"fSlapKnockdownHandSpeedThresholdMult", 
			fSlapKnockdownHandSpeedThresholdMult, 
			0.0f, 
			5.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"ExtraMechanics", 
			"fSlapKnockdownShoulderSpeedThresholdMult", 
			fSlapKnockdownShoulderSpeedThresholdMult, 
			0.0f, 
			5.0f
		);
		ReadUInt32Setting
		(
			a_ini, 
			"ExtraMechanics", 
			"uMaxGrabbedReferences", 
			uMaxGrabbedReferences, 
			0,
			101
		);
		ReadUInt32Setting
		(
			a_ini, 
			"ExtraMechanics", 
			"uSlapKnockdownCriteria", 
			uSlapKnockdownCriteria, 
			0,
			!SlapKnockdownCriteria::kTotal - 1
		);

		// Menus and UI
		ReadBoolSetting(a_ini, "MenusAndUI", "bCrosshairTextFade", bCrosshairTextFade);
		ReadBoolSetting(a_ini, "MenusAndUI", "bTwoPlayerLockpicking", bTwoPlayerLockpicking);
		ReadBoolSetting
		(
			a_ini,
			"MenusAndUI", 
			"bUninitializedDialogueWithClosestPlayer",
			bUninitializedDialogueWithClosestPlayer
		);
		ReadFloatSetting
		(
			a_ini, "MenusAndUI", "fAutoEndDialogueRadius", fAutoEndDialogueRadius, 100.0f, 2000.0f
		);
		ReadFloatSetting
		(
			a_ini,
			"MenusAndUI",
			"fCamLockOnIndicatorLength", 
			fCamLockOnIndicatorLength, 
			1.0f,
			200.0f
		);
		ReadFloatSetting
		(
			a_ini,
			"MenusAndUI", 
			"fCrosshairMaxTraversablePixelsPerSec",
			fCrosshairMaxTraversablePixelsPerSec, 
			160.0f,
			7680.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"MenusAndUI",
			"fCamLockOnIndicatorThickness", 
			fCamLockOnIndicatorThickness, 
			1.0f, 
			20.0f
		);
		ReadFloatSetting
		(
			a_ini,
			"MenusAndUI",
			"fCrosshairTextAnchorPointHeightRatio", 
			fCrosshairTextAnchorPointHeightRatio,
			0.0f, 
			1.0f
		);
		ReadFloatSetting
		(
			a_ini,
			"MenusAndUI",
			"fCrosshairTextAnchorPointWidthRatio", 
			fCrosshairTextAnchorPointWidthRatio,
			0.0f, 
			1.0f
		);
		ReadFloatSetting
		(
			a_ini, "MenusAndUI", "fCrosshairTextMargin", fCrosshairTextMargin, 0.0f, 1000.0f
		);
		ReadFloatSetting
		(
			a_ini, "MenusAndUI", "fCrosshairTextMaxAlpha", fCrosshairTextMaxAlpha, 0.0f, 100.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"MenusAndUI", 
			"fPlayerMenuControlOverlayOutlineThickness",
			fPlayerMenuControlOverlayOutlineThickness, 
			1.0f, 
			50.0f
		);
		ReadUInt32Setting
		(
			a_ini, "MenusAndUI", "uCrosshairTextFontSize", uCrosshairTextFontSize, 0, 256
		);

		// Movement
		ReadFloatSetting(a_ini, "Movement", "fBaseRotationMult", fBaseRotationMult, 0.25f, 4.0f);
		ReadFloatSetting(a_ini, "Movement", "fBaseSpeed", fBaseSpeed, 0.0f, 300.f);
		ReadFloatSetting(a_ini, "Movement", "fBashingRotMult", fBashingRotMult, 0.1f, 1.0f);
		ReadFloatSetting(a_ini, "Movement", "fBlockingRotMult", fBlockingRotMult, 0.1f, 1.0f);
		ReadFloatSetting(a_ini, "Movement", "fCastingMovMult", fCastingMovMult, 0.1f, 1.0f);
		ReadFloatSetting(a_ini, "Movement", "fCastingRotMult", fCastingRotMult, 0.1f, 1.0f);
		ReadFloatSetting(a_ini, "Movement", "fJumpingRotMult", fJumpingRotMult, 0.1f, 1.0f);
		ReadFloatSetting
		(
			a_ini, 
			"Movement",
			"fJumpAdditionalLaunchSpeed", 
			fJumpAdditionalLaunchSpeed, 
			0.0f, 
			5000.0f
		);
		ReadFloatSetting
		(
			a_ini, "Movement", "fJumpingGravityMult", fJumpingGravityMult, 0.1f, 10.0f
		);
		ReadFloatSetting
		(
			a_ini, "Movement", "fJumpingMaxSlopeAngle", fJumpingMaxSlopeAngle, 0.0f, 90.0f, true
		);
		ReadFloatSetting
		(
			a_ini, "Movement", "fMeleeAttackMovMult", fMeleeAttackMovMult, 0.1f, 1.0f
		);
		ReadFloatSetting
		(
			a_ini, "Movement", "fMeleeAttackRotMult", fMeleeAttackRotMult, 0.1f, 1.0f
		);
		ReadFloatSetting
		(
			a_ini, "Movement", "fRangedAttackMovMult", fRangedAttackMovMult, 0.1f, 1.0f
		);
		ReadFloatSetting
		(
			a_ini, "Movement", "fRangedAttackRotMult", fRangedAttackRotMult, 0.1f, 1.0f
		);
		ReadFloatSetting(a_ini, "Movement", "fRidingRotMult", fRidingRotMult, 0.1f, 1.0f);
		ReadFloatSetting(a_ini, "Movement", "fSneakRotMult", fSneakRotMult, 0.1f, 1.0f);
		ReadFloatSetting(a_ini, "Movement", "fSprintingMovMult", fSprintingMovMult, 0.1f, 10.0f);
		ReadFloatSetting(a_ini, "Movement", "fSprintingRotMult", fSprintingRotMult, 0.1f, 1.0f);
		ReadFloatSetting
		(
			a_ini, "Movement", "fSecsAfterGatherToFall", fSecsAfterGatherToFall, 0.01f, 2.0f
		);

		// New Systems
		ReadBoolSetting
		(
			a_ini, "NewSystems", "bCanKillmoveOtherPlayers", bCanKillmoveOtherPlayers
		);
		ReadBoolSetting(a_ini, "NewSystems", "bCanRevivePlayer1", bCanRevivePlayer1);
		ReadBoolSetting(a_ini, "NewSystems", "bHoldToCycle", bHoldToCycle);
		ReadBoolSetting(a_ini, "NewSystems", "bUseDashDodgeSystem", bUseDashDodgeSystem);
		ReadBoolSetting(a_ini, "NewSystems", "bUseKillmovesSystem", bUseKillmovesSystem);
		ReadBoolSetting(a_ini, "NewSystems", "bUseReviveSystem", bUseReviveSystem);
		ReadBoolSetting
		(
			a_ini, 
			"NewSystems",
			"bUseUnarmedKillmovesForSpellcasting", 
			bUseUnarmedKillmovesForSpellcasting
		);
		ReadFloatSetting(a_ini, "NewSystems", "fKillmoveChance", fKillmoveChance, 0.0f, 1.0f);
		ReadFloatSetting
		(
			a_ini, "NewSystems", "fKillmoveHealthFraction", fKillmoveHealthFraction, 0.0f, 1.0f
		);
		ReadFloatSetting
		(
			a_ini,
			"NewSystems",
			"fMaxDashDodgeSpeedmult", 
			fMaxDashDodgeSpeedmult,
			1.0f, 
			1200.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"NewSystems", 
			"fMinDashDodgeSpeedmult",
			fMinDashDodgeSpeedmult,
			1.0f, 
			1200.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"NewSystems", 
			"fSecsBeforeActivationCycling",
			fSecsBeforeActivationCycling, 
			0.0f,
			3.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"NewSystems", 
			"fSecsBetweenActivationChecks", 
			fSecsBetweenActivationChecks, 
			0.1f,
			3.0f
		);
		ReadFloatSetting
		(
			a_ini, "NewSystems", "fSecsCyclingInterval", fSecsCyclingInterval, 0.1f, 3.0f
		);
		ReadFloatSetting
		(
			a_ini, "NewSystems", "fSecsDefMinHoldTime", fSecsDefMinHoldTime, 0.1f, 1.0f
		);
		ReadFloatSetting
		(
			a_ini, "NewSystems", "fSecsReviveTime", fSecsReviveTime, 1.0f, 60.0f
		);
		ReadFloatSetting
		(
			a_ini, "NewSystems", "fSecsUntilDownedDeath", fSecsUntilDownedDeath, 1.0f, 300.0f
		);
		ReadUInt32Setting
		(
			a_ini,
			"NewSystems", 
			"uAmmoAutoEquipMode", 
			uAmmoAutoEquipMode,
			0, 
			!AmmoAutoEquipMode::kTotal - 1
		);
		ReadUInt32Setting
		(
			a_ini, 
			"NewSystems",
			"uDashDodgeBaseAnimFrameCount", 
			uDashDodgeBaseAnimFrameCount,
			0, 
			54
		);

		// Progression
		ReadBoolSetting
		(
			a_ini, 
			"Progression",
			"bStackCoopPlayerSkillAVAutoScaling",
			bStackCoopPlayerSkillAVAutoScaling
		);
		ReadFloatSetting
		(
			a_ini, 
			"Progression",
			"fLevelUpXPThresholdMult", 
			fLevelUpXPThresholdMult, 
			0.1f, 
			4.0f
		);
		ReadFloatSetting
		(
			a_ini, 
			"Progression",
			"fPerkPointsPerLevelUp", 
			fPerkPointsPerLevelUp, 
			0.0f, 
			10.0f
		);
		ReadUInt32Setting
		(
			a_ini,
			"Progression",
			"uFlatPerkPointsIncrease",
			uFlatPerkPointsIncrease,
			0,
			255
		);
		ReadBoolSetting
		(
			a_ini,
			"Progression",
			"bEveryoneGetsALootedEnderalSkillbook", 
			bEveryoneGetsALootedEnderalSkillbook
		);
		ReadBoolSetting
		(
			a_ini, 
			"Progression",
			"bScaleCraftingPointsWithNumPlayers",
			bScaleCraftingPointsWithNumPlayers
		);
		ReadBoolSetting
		(
			a_ini, 
			"Progression", 
			"bScaleLearningPointsWithNumPlayers",
			bScaleLearningPointsWithNumPlayers
		);
		ReadBoolSetting
		(
			a_ini, 
			"Progression", 
			"bScaleMemoryPointsWithNumPlayers", 
			bScaleMemoryPointsWithNumPlayers
		);
		ReadFloatSetting
		(
			a_ini, 
			"Progression", 
			"fAdditionalGoldPerPlayerMult", 
			fAdditionalGoldPerPlayerMult,
			0.0f,
			10.0f
		);

		SPDLOG_DEBUG("[Settings] ImportGeneralSettings: Done importing.");
	}

	void Settings::ImportPlayerSpecificSettings(CSimpleIniA& a_ini)
	{
		// Import settings that vary from player to player.
		
		// Section name changes based on player index.
		std::string sectionName{ ""sv };
		for (auto pIndex = 0; pIndex < ALYSLC_MAX_PLAYER_COUNT; ++pIndex) 
		{
			sectionName = fmt::format("Player{}", pIndex + 1);

			// Stat Modifiers
			ReadFloatSetting
			(
				a_ini,
				sectionName.data(),
				"fHealthRegenMult",
				(float&)vfHealthRegenMult[pIndex],
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini,
				sectionName.data(), 
				"fMagickaCostMult",
				(float&)vfMagickaCostMult[pIndex],
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fMagickaRegenMult",
				(float&)vfMagickaRegenMult[pIndex], 
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini,
				sectionName.data(),
				"fStaminaCostMult", 
				(float&)vfStaminaCostMult[pIndex],
				0.0f, 
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fStaminaRegenMult", 
				(float&)vfStaminaRegenMult[pIndex],
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fHealthRegenCombatRatio",
				(float&)vfHealthRegenCombatRatio[pIndex], 
				0.0f,
				1.0f
			);
			ReadFloatSetting
			(
				a_ini,
				sectionName.data(),
				"fMagickaRegenCombatRatio",
				(float&)vfMagickaRegenCombatRatio[pIndex],
				0.0f,
				1.0f
			);
			ReadFloatSetting
			(
				a_ini,
				sectionName.data(),
				"fStaminaRegenCombatRatio", 
				(float&)vfStaminaRegenCombatRatio[pIndex], 
				0.0f, 
				1.0f
			);

			// Damage Modifiers
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fDamageDealtMult", 
				(float&)vfDamageDealtMult[pIndex],
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fDamageReceivedMult", 
				(float&)vfDamageReceivedMult[pIndex], 
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fFlopDamageMult", 
				(float&)vfFlopDamageMult[pIndex],
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fSlapKnockdownDamageMult", 
				(float&)vfSlapKnockdownDamageMult[pIndex], 
				0.0f, 
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fThrownObjectDamageMult", 
				(float&)vfThrownObjectDamageMult[pIndex], 
				0.0f, 
				100.0f
			);
			
			// Extra Mechanics Cost Modifiers
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fFlopHealthCostMult", 
				(float&)vfFlopHealthCostMult[pIndex],
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fObjectManipulationMagickaCostMult", 
				(float&)vfObjectManipulationMagickaCostMult[pIndex],
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fSlapStaminaCostMult", 
				(float&)vfSlapStaminaCostMult[pIndex],
				0.0f,
				100.0f
			);

			// Controller
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fAnalogDeadzoneRatio", 
				(float&)vfAnalogDeadzoneRatio[pIndex], 
				0.0f, 
				1.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fTriggerDeadzoneRatio", 
				(float&)vfTriggerDeadzoneRatio[pIndex], 
				0.0f,
				1.0f
			);

			// Progression
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fSkillXPMult", 
				(float&)vfSkillXPMult[pIndex], 
				0.0f,
				10.0f
			);

			// Targeting
			ReadBoolSettingToIndex
			(
				a_ini, 
				sectionName.data(), 
				"bCanTargetOtherPlayers", 
				vbCanTargetOtherPlayers,
				pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini,
				sectionName.data(), 
				"bCrosshairMagnetismForActors",
				vbCrosshairMagnetismForActors,
				pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini, 
				sectionName.data(),
				"bCrosshairMagnetismForObjRefs",
				vbCrosshairMagnetismForObjRefs, 
				pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini, sectionName.data(), "bFriendlyFire", vbFriendlyFire, pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini, sectionName.data(), "bUseAimCorrection", vbUseAimCorrection, pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini,
				sectionName.data(),
				"bScreenspaceBasedAimCorrectionCheck", 
				vbScreenspaceBasedAimCorrectionCheck,
				pIndex
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fAimCorrectionFOV", 
				(float&)vfAimCorrectionFOV[pIndex],
				0.0f,
				360.0f, 
				true
			);
			ReadFloatSetting
			(
				a_ini,
				sectionName.data(), 
				"fMaxAimPitchAdjustmentRate", 
				(float&)vfMaxAimPitchAdjustmentRate[pIndex],
				0.0f,
				720.0f, 
				true
			);
			// Skip 'kAimDirection', hence the '- 2'.
			ReadUInt32Setting
			(
				a_ini, 
				sectionName.data(),
				"uProjectileTrajectoryType", 
				(uint32_t&)vuProjectileTrajectoryType[pIndex], 
				0, 
				!ProjectileTrajType::kTotal - 2
			);

			// UI
			ReadBoolSettingToIndex
			(
				a_ini, 
				sectionName.data(), 
				"bAnimatedCrosshair",
				vbAnimatedCrosshair,
				pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini, 
				sectionName.data(), 
				"bFadeInactiveCrosshair", 
				vbFadeInactiveCrosshair, 
				pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini, 
				sectionName.data(), 
				"bRecenterInactiveCrosshair",
				vbRecenterInactiveCrosshair,
				pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini, 
				sectionName.data(),
				"bSkyrimStyleCrosshair", 
				vbSkyrimStyleCrosshair,
				pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini, 
				sectionName.data(),
				"bEnableAimPitchIndicator", 
				vbEnableAimPitchIndicator,
				pIndex
			);
			ReadBoolSettingToIndex
			(
				a_ini, 
				sectionName.data(),
				"bEnablePredictedProjectileTrajectoryCurves", 
				vbEnablePredictedProjectileTrajectoryCurves,
				pIndex
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fCrosshairGapRadius",
				(float&)vfCrosshairGapRadius[pIndex],
				0.0f, 
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fCrosshairLength", 
				(float&)vfCrosshairLength[pIndex],
				0.0f,
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fCrosshairThickness", 
				(float&)vfCrosshairThickness[pIndex],
				0.0f, 
				10.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fCrosshairHorizontalSensitivity",
				(float&)vfCrosshairHorizontalSensitivity[pIndex],
				0.0f, 
				1.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fCrosshairVerticalSensitivity", 
				(float&)vfCrosshairVerticalSensitivity[pIndex], 
				0.0f, 
				1.0f
			);
			ReadFloatSetting
			(
				a_ini,
				sectionName.data(), 
				"fPlayerIndicatorLength", 
				(float&)vfPlayerIndicatorLength[pIndex], 
				1.0f, 
				100.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fPlayerIndicatorThickness", 
				(float&)vfPlayerIndicatorThickness[pIndex], 
				1.0f, 
				10.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fPredictedProjectileTrajectoryCurveThickness", 
				(float&)vfPredictedProjectileTrajectoryCurveThickness[pIndex], 
				1.0f, 
				10.0f
			);

			// Default RGB values:
			// P1: Red, P2: Green, P3: Cyan, P4: Yellow.
			uint32_t rgb = 
			(
				pIndex == 0 ? 0xFF0000 : 
				pIndex == 1 ? 0x00FF00 :
				pIndex == 2 ? 0x00FFFF :
				pIndex == 3 ? 0xFFFF00 :
				0xFFFFFF
			);

			// Set RGB and alpha values separately.
			uint32_t alpha = 0xFF;
			ReadRGBStringSetting(a_ini, sectionName.data(), "sOverlayRGBValue", rgb);
			ReadUInt32Setting(a_ini, sectionName.data(), "uOverlayAlpha", alpha, 0, 0xFF);
			vuOverlayRGBAValues[pIndex] = (rgb << 8) | alpha;

			rgb = 0;
			alpha = 0xFF;
			ReadRGBStringSetting(a_ini, sectionName.data(), "sCrosshairInnerOutlineRGB", rgb);
			ReadUInt32Setting
			(
				a_ini, sectionName.data(), "uCrosshairInnerOutlineAlpha", alpha, 0, 0xFF
			);
			vuCrosshairInnerOutlineRGBAValues[pIndex] = (rgb << 8) | alpha;

			rgb = 0xFFFFFF;
			alpha = 0xFF;
			ReadRGBStringSetting(a_ini, sectionName.data(), "sCrosshairOuterOutlineRGB", rgb);
			ReadUInt32Setting
			(
				a_ini, sectionName.data(), "uCrosshairOuterOutlineAlpha", alpha, 0, 0xFF
			);
			vuCrosshairOuterOutlineRGBAValues[pIndex] = (rgb << 8) | alpha;

			ReadUInt32Setting
			(
				a_ini,
				sectionName.data(),
				"uPlayerIndicatorVisibilityType",
				(uint32_t&)vuPlayerIndicatorVisibilityType[pIndex],
				0, 
				!PlayerIndicatorVisibilityType::kTotal - 1
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fMinSecsCrosshairTargetTraversal", 
				(float&)vfMinSecsCrosshairTargetTraversal[pIndex], 
				0.0f, 
				2.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fSecsBeforeRemovingInactiveCrosshair", 
				(float&)vfSecsBeforeRemovingInactiveCrosshair[pIndex], 
				0.5f,
				10.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(),
				"fSecsToOscillateCrosshair", 
				(float&)vfSecsToOscillateCrosshair[pIndex], 
				0.1f, 
				10.0f
			);
			ReadFloatSetting
			(
				a_ini, 
				sectionName.data(), 
				"fSecsToRotateCrosshair", 
				(float&)vfSecsToRotateCrosshair[pIndex], 
				0.1f,
				5.0f
			);
		}

		SPDLOG_DEBUG("[Settings] ImportPlayerSpecificSettings: Done importing.");
	}

	bool Settings::ReadBoolSetting
	(
		CSimpleIniA& a_ini, const char* a_section, const char* a_settingKey, bool& a_settingOut
	)
	{
		// Read boolean setting with the given key from the given section into the outparam.
		// Returns true if successful.

		if (auto keyValidPtr = a_ini.GetValue(a_section, a_settingKey); keyValidPtr) 
		{
			a_settingOut = a_ini.GetBoolValue(a_section, a_settingKey);
			return true;
		}
		else
		{
			SPDLOG_ERROR
			(
				"[Settings] ERR: ReadBoolSetting: Invalid section::key '{}::{}'.", 
				a_section, a_settingKey
			);
			return false;
		}
	}

	bool Settings::ReadBoolSettingToIndex
	(
		CSimpleIniA& a_ini, 
		const char* a_section, 
		const char* a_settingKey, 
		std::vector<bool>& a_settingsList,
		uint8_t&& a_index
	)
	{
		// Read a boolean setting with the given key from the given section 
		// into the given index in the outparam list.
		// Returns true if successful.

		if (a_index < a_settingsList.size()) 
		{
			if (auto keyValidPtr = a_ini.GetValue(a_section, a_settingKey); keyValidPtr)
			{
				bool value = a_ini.GetBoolValue(a_section, a_settingKey);
				a_settingsList[a_index] = value;
				return true;
			}
			else
			{
				SPDLOG_ERROR
				(
					"[Settings] ERR: ReadBoolSettingToIndex: Invalid section::key '{}::{}'.", 
					a_section, a_settingKey
				);
				return false;
			}
		}
		else
		{
			SPDLOG_ERROR
			(
				"[Settings] ERR: ReadBoolSettingToIndex: "
				"Settings list for ({}:{}) is not large enough for requested index ({}).", 
				a_settingKey, a_section, a_index
			);
			return false;
		}
	}

	bool Settings::ReadFloatSetting
	(
		CSimpleIniA& a_ini, 
		const char* a_section, 
		const char* a_settingKey, 
		float& a_settingOut, 
		float&& a_lowerLimit,
		float&& a_upperLimit,
		bool&& a_convertToRadians
	)
	{
		// Read a float setting with the given key from the given section into the outparam.
		// Also clamp the result to the given lower and upper bounds, 
		// and/or convert the result to radians.
		// Returns true if successful.

		if (auto keyValidPtr = a_ini.GetValue(a_section, a_settingKey); keyValidPtr)
		{
			a_settingOut = std::clamp
			(
				static_cast<float>(a_ini.GetDoubleValue(a_section, a_settingKey)),
				a_lowerLimit, 
				a_upperLimit
			);
			if (a_convertToRadians) 
			{
				a_settingOut *= TO_RADIANS;
			}

			return true;
		}
		else
		{
			SPDLOG_ERROR
			(
				"[Settings] ERR: ReadFloatSetting: Invalid section::key '{}::{}'.", 
				a_section, a_settingKey
			);
			return false;
		}
	}

	bool Settings::ReadInt32Setting
	(
		CSimpleIniA& a_ini,
		const char* a_section, 
		const char* a_settingKey,
		int32_t& a_settingOut, 
		int32_t&& a_lowerLimit,
		int32_t&& a_upperLimit
	)
	{
		// Read a signed 32-bit integer setting with the given key from the given section 
		// into the outparam.
		// Also clamp the result to the given lower and upper bounds.
		// Returns true if successful.

		if (auto keyValidPtr = a_ini.GetValue(a_section, a_settingKey); keyValidPtr)
		{
			a_settingOut = static_cast<int32_t>(a_ini.GetLongValue(a_section, a_settingKey));
			return true;
		}
		else
		{
			SPDLOG_ERROR
			(
				"[Settings] ERR: ReadInt32Setting: Invalid section::key '{}::{}'.", 
				a_section, a_settingKey
			);
			return false;
		}
	}

	bool Settings::ReadRGBStringSetting
	(
		CSimpleIniA& a_ini, const char* a_section, const char* a_settingKey, uint32_t& a_settingOut
	)
	{
		// Read an unsigned 32-bit integer setting with the given key from the given section 
		// and parse an RGB value from it into the outparam.
		// Returns true if successful.

		if (auto keyValidPtr = a_ini.GetValue(a_section, a_settingKey); keyValidPtr)
		{
			std::string str{ keyValidPtr };
			std::regex pattern{ "[0-9a-fA-F]{6}" };
			std::sregex_iterator firstMatchIter{ str.begin(), str.end(), pattern };
			// Matches RGB regex.
			if (firstMatchIter != std::sregex_iterator()) 
			{
				a_settingOut = std::stoi((*firstMatchIter).str(), nullptr, 16);
			}
			else
			{
				SPDLOG_ERROR
				(
					"[Settings] ERR: ReadRGBStringSetting: ({}:{}) has value '{}' "
					"but could not be converted to hex color format 'RRGGBB'.",
					a_settingKey, a_section, a_settingOut
				);
			}

			return true;
		}
		else
		{
			SPDLOG_ERROR
			(
				"[Settings] ERR: ReadRGBStringSetting: Invalid section::key '{}::{}'.", 
				a_section, a_settingKey
			);
			return false;
		}
	}

	bool Settings::ReadStringSetting
	(
		CSimpleIniA& a_ini, 
		const char* a_section, 
		const char* a_settingKey, 
		RE::BSFixedString& a_settingOut
	)
	{
		// Read a string setting with the given key from the given section into the outparam.
		// Returns true if successful.

		if (auto keyValidPtr = a_ini.GetValue(a_section, a_settingKey); keyValidPtr)
		{
			a_settingOut = keyValidPtr;
			return true;
		}
		else
		{
			SPDLOG_ERROR
			(
				"[Settings] ERR: ReadStringSetting: Invalid section::key '{}::{}'.",
				a_section, a_settingKey
			);
			return false;
		}
	}

	bool Settings::ReadUInt32Setting
	(
		CSimpleIniA& a_ini, 
		const char* a_section,
		const char* a_settingKey,
		uint32_t& a_settingOut, 
		uint32_t&& a_lowerLimit,
		uint32_t&& a_upperLimit
	)
	{ 
		// Read an unsigned 32-bit integer setting with the given key 
		// from the given section into the outparam.
		// Also clamp the result to the given lower and upper bounds.
		// Returns true if successful.

		if (auto keyValidPtr = a_ini.GetValue(a_section, a_settingKey); keyValidPtr)
		{
			a_settingOut = static_cast<uint32_t>(a_ini.GetLongValue(a_section, a_settingKey));
			return true;
		}
		else
		{
			SPDLOG_ERROR
			(
				"[Settings] ERR: ReadUInt32Setting: Invalid section::key '{}::{}'.",
				a_section, a_settingKey
			);
			return false;
		}
	}
}

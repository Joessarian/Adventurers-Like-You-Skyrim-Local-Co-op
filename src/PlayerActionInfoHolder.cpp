#include "PlayerActionInfoHolder.h"
#include <GlobalCoopData.h>

namespace ALYSLC
{
	PlayerActionInfoHolder::PlayerActionInfoHolder()
	{
		// Generate base player action params list and assign as the default for each player
		// until binds are imported later.

		defPAParamsList = PAParamsList();
		// List of inputs for the current player action.
		ComposingInputActionList inputsList{};
		// Subtract from each input action's index to get the player action index,
		// since player actions are defined after all buttons in the enum.
		const auto actionOffset = !InputAction::kFirstAction;
		// Player action index, not input action index.
		uint32_t index = 0;
		// Out param for the number of composing player actions for the current player action.
		uint32_t numComposingPlayerActions = 0;
		// Out param for the max decomposition recursion depth reached for the current player action.
		uint8_t recursionDepth = 0;
		for (auto i = !InputAction::kFirstAction; i <= !InputAction::kLastAction; ++i) 
		{
			index = i - actionOffset;
			recursionDepth = 0;
			numComposingPlayerActions = 0;
			// Get default composing input list.
			inputsList = GetComposingInputs(static_cast<InputAction>(i), DEF_PA_COMP_INPUT_ACTIONS_LIST, numComposingPlayerActions, recursionDepth);
			// If max recursion depth is reached, the bind is invalid and will be disabled.
			if (recursionDepth >= Settings::uMaxComposingInputsCheckRecursionDepth) 
			{
				logger::error("[PAIH] ERR: PlayerActionInfoHolder: Default {} bind is invalid. Possibly a circular bind. Disabling.", static_cast<InputAction>(i));
				inputsList.clear();
				numComposingPlayerActions = 0;
			}

			// Construct default params list for this action.
			defPAParamsList[index] = PlayerActionParams
			(
				inputsList.empty() ? PerfType::kDisabled : ACTION_BASE_PERF_TYPES[index],
				ACTION_BASE_TRIGGER_FLAGS[index],
				inputsList,
				GetInputMask(inputsList),
				numComposingPlayerActions
			);
		}

		// Set to defaults initially and then import custom binds later with the other settings.
		playerPAParamsLists.fill(defPAParamsList);
		ALYSLC::Log("[PAIH] Finished initializing.");
	}

	const uint32_t PlayerActionInfoHolder::GetInputMask(const std::vector<InputAction>& a_composingInputs) const noexcept
	{
		// Get input mask from a list of composing inputs, not player actions.
		// Each input index is left-shifted a number of times equal to the index
		// and OR-ed together to get the full mask.
		uint32_t mask = 0;
		for (auto i = 0; i < a_composingInputs.size(); ++i)
		{
			// Input action is a player action, not an input. Return a mask of 0.
			if (!a_composingInputs[i] >= !InputAction::kInputTotal || a_composingInputs[i] == InputAction::kInvalid) 
			{
				ALYSLC::Log("[PAIH] ERR: GetInputMask: Composing input action {} is a player action and was not broken down. Mask set to 0.", a_composingInputs[i]);
				return 0;
			}

			const auto& input = a_composingInputs[i];
			// Set the bit in the mask.
			mask |= 1 << !input;
		}

		return mask;
	}

	std::vector<InputAction> PlayerActionInfoHolder::GetComposingInputs(const InputAction& a_playerAction, const PACompInputActionLists& a_paCompInputActionsLists, uint32_t& a_numComposingPlayerActionsOut, uint8_t& a_recursionDepthOut) const noexcept
	{
		// Get a list of composing inputs by recursively decomposing the given player action 
		// using the given composing input actions lists.
		// Only recurse up to the pre-defined max depth.
		// Also send the total number of decomposed player actions and recursion depth reached
		// through the out params.

		// List of composing inputs. No player actions allowed.
		std::vector<InputAction> composingInputs;
		// List of composing input actions for the current player action.
		const auto& composingInputActions = a_paCompInputActionsLists[!a_playerAction - !InputAction::kFirstAction];
		// Player action is disabled if it has no composing input actions, so return empty list.
		if (composingInputActions.empty())
		{
			a_recursionDepthOut = 0;
			a_numComposingPlayerActionsOut = 0;
			return std::vector<InputAction>();
		}

		++a_recursionDepthOut;
		// Recursion depth exceeded.
		if (a_recursionDepthOut >= Settings::uMaxComposingInputsCheckRecursionDepth)
		{
			logger::error("[PAIH] ERR: GetComposingInputs. Invalid bind for {}: max recursion depth reached when attempting to break down player action bind into composing inputs. Possibly a circular bind. Bind will be disabled.", a_playerAction);
			a_numComposingPlayerActionsOut = 0;
			return std::vector<InputAction>();
		}

		for (auto i = 0; i < composingInputActions.size(); ++i)
		{
			auto inputAction = composingInputActions[i];
			if (inputAction < InputAction::kInputTotal)
			{
				// Is an input index, so insert right away.
				composingInputs.emplace_back(inputAction);
			}
			else if (inputAction >= InputAction::kFirstAction && inputAction <= InputAction::kLastAction)
			{
				// Is a player action.
				++a_numComposingPlayerActionsOut;
				// Recursively break down the player action until only input indices remain.
				if (const auto actionInputs = GetComposingInputs(inputAction, a_paCompInputActionsLists, a_numComposingPlayerActionsOut, a_recursionDepthOut); !actionInputs.empty() && a_recursionDepthOut < Settings::uMaxComposingInputsCheckRecursionDepth)
				{
					composingInputs.insert(composingInputs.end(), actionInputs.begin(), actionInputs.end());
				}
			}
		}

		if (a_recursionDepthOut >= Settings::uMaxComposingInputsCheckRecursionDepth)
		{
			// Recursion depth exceeded.
			a_numComposingPlayerActionsOut = 0;
			return std::vector<InputAction>();
		}
		else
		{
			// Player action successfully broken down into inputs.
			return composingInputs;
		}
	}

	PlayerActionParams::PlayerActionParams() :
		perfType(PerfType::kOnRelease), 
		triggerFlags(TriggerFlag::kDefault),
		composingInputs({}),
		inputMask(0),
		composingPlayerActionsCount(0)
	{ }

	PlayerActionParams::PlayerActionParams(PerfType a_perfType, TriggerFlags a_triggerFlags, ComposingInputActionList a_composingInputs, uint32_t a_inputMask, uint32_t a_composingPlayerActionsCount) :
		perfType(a_perfType), 
		triggerFlags(a_triggerFlags), 
		composingInputs(a_composingInputs), 
		inputMask(a_inputMask),
		composingPlayerActionsCount(a_composingPlayerActionsCount)
	{ }

	PlayerActionParams::PlayerActionParams(const PlayerActionParams& a_other) :
		perfType(a_other.perfType), 
		triggerFlags(a_other.triggerFlags),
		composingInputs(a_other.composingInputs),
		inputMask(a_other.inputMask),
		composingPlayerActionsCount(a_other.composingPlayerActionsCount)
	{ }

	PlayerActionParams::PlayerActionParams(PlayerActionParams&& a_other) :
		perfType(std::move(a_other.perfType)),
		triggerFlags(std::move(a_other.triggerFlags)),
		composingInputs(std::move(a_other.composingInputs)),
		inputMask(std::move(a_other.inputMask)),
		composingPlayerActionsCount(std::move(a_other.composingPlayerActionsCount))
	{ }

	PlayerActionParams& PlayerActionParams::operator=(const PlayerActionParams& a_other)
	{
		perfType = a_other.perfType;
		triggerFlags = a_other.triggerFlags;
		composingInputs = a_other.composingInputs;
		inputMask = a_other.inputMask;
		composingPlayerActionsCount = a_other.composingPlayerActionsCount;
		return *this;
	}

	PlayerActionParams& PlayerActionParams::operator=(PlayerActionParams&& a_other)
	{
		perfType = std::move(a_other.perfType);
		triggerFlags = std::move(a_other.triggerFlags);
		composingInputs = std::move(a_other.composingInputs);
		inputMask = std::move(a_other.inputMask);
		composingPlayerActionsCount = std::move(a_other.composingPlayerActionsCount);
		return *this;
	}
}

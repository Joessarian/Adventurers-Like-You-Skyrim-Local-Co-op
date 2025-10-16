#include "ModAPI.h"
#include <GlobalCoopData.h>

namespace ALYSLC_API
{
	//=============================================================================================
	// [V1]
	//=============================================================================================

	RE::ActorHandle ALYSLCInterface::GetALYSLCPlayerByDID(int32_t a_deviceID) const noexcept
	{
		// Return the actor handle for the player with the given input device ID,
		// or an empty handle if no co-op session is active, 
		// if the player with the given device ID is currently inactive,
		// or if the device ID is invalid.

		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		if (!glob.globalDataInit ||
			!glob.allPlayersInit ||
			!glob.coopSessionActive || 
			a_deviceID < 0)
		{
			return RE::ActorHandle();
		}

		
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			if (p->deviceID == a_deviceID)
			{
				return p->coopActor->GetHandle();
			}
		}

		return RE::ActorHandle();
	}

	RE::ActorHandle ALYSLCInterface::GetALYSLCPlayerByPID(int32_t a_playerID) const noexcept
	{
		// Return the actor handle for the player with the given player ID,
		// or an empty handle if no co-op session is active, 
		// if the player with the given player ID is currently inactive,
		// or if the player ID is invalid.

		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		if (!glob.globalDataInit ||
			!glob.allPlayersInit ||
			!glob.coopSessionActive || 
			a_playerID < 0 ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT ||
			!!glob.coopPlayers[a_playerID] ||
			!glob.coopPlayers[a_playerID]->isActive ||
			!glob.coopPlayers[a_playerID]->coopActor)
		{
			return RE::ActorHandle();
		}

		return glob.coopPlayers[a_playerID]->coopActor->GetHandle();
	}

	int32_t ALYSLCInterface::GetALYSLCPlayerDID(RE::ActorHandle a_actorHandle) const noexcept
	{
		// Return the input device ID corresponding to the given actor handle,
		// -1 if no co-op session is active, if the player is currently inactive,
		// if the given handle is invalid, or if the given actor is not a player.

		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		if (!glob.globalDataInit || 
			!glob.allPlayersInit || 
			!glob.coopSessionActive || 
			!ALYSLC::Util::HandleIsValid(a_actorHandle))
		{
			return -1;
		}

		const auto pIndex = ALYSLC::GlobalCoopData::GetCoopPlayerIndex(a_actorHandle);
		if (pIndex == -1)
		{
			return -1;
		}

		return glob.coopPlayers[pIndex]->deviceID;
	}

	int32_t ALYSLCInterface::GetALYSLCPlayerPID(RE::ActorHandle a_actorHandle) const noexcept
	{
		// Return the player ID corresponding to the given actor handle,
		// -1 if no co-op session is active, if the player is currently inactive,
		// if the given handle is invalid, or if the given actor is not a player.

		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		if (!glob.globalDataInit || 
			!glob.allPlayersInit || 
			!glob.coopSessionActive || 
			!ALYSLC::Util::HandleIsValid(a_actorHandle))
		{
			return -1;
		}

		return ALYSLC::GlobalCoopData::GetCoopPlayerIndex(a_actorHandle);
	}

	bool ALYSLCInterface::IsALYSLCCharacter(RE::ActorHandle a_actorHandle) const noexcept
	{
		// Return true if the given actor handle corresponds to a controllable co-op character.
		// Return false otherwise.
		// A co-op session does not have to be active.

		return ALYSLC::GlobalCoopData::IsCoopCharacter(a_actorHandle);
	}

	bool ALYSLCInterface::IsALYSLCPlayer(RE::ActorHandle a_actorHandle) const noexcept
	{
		// Return true if the given actor handle corresponds to an active player.
		// Return false otherwise.

		return ALYSLC::GlobalCoopData::IsCoopPlayer(a_actorHandle);
	}

	bool ALYSLCInterface::IsSessionActive() const noexcept
	{
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		return glob.coopSessionActive;
	}

	//=============================================================================================
	// [V2]
	//=============================================================================================

	bool ALYSLCInterface::IsPerformingAction
	(
		RE::ActorHandle a_playerActorHandle, uint32_t a_playerActionIndex
	) const noexcept
	{
		// Return true if the player controlling the character given by the actor handle
		// is performing the action that corresponds to the given index.
		// See the 'ALYSLC::InputAction' enum in the 'Enums.h' file
		// for the supported action indices.
	
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		// Player must be active, have a valid actor handle, 
		// and the player action must fall within the player action index range.
		if (!glob.globalDataInit || 
			!glob.coopSessionActive || 
			!ALYSLC::Util::HandleIsValid(a_playerActorHandle) || 
			a_playerActionIndex < !ALYSLC::InputAction::kFirstAction ||
			a_playerActionIndex > !ALYSLC::InputAction::kLastAction)
		{
			return false;
		}

		// Must be a player.
		auto pIndex = ALYSLC::GlobalCoopData::GetCoopPlayerIndex(a_playerActorHandle);
		if (pIndex == -1)
		{
			return false;
		}

		return 
		(
			glob.coopPlayers[pIndex]->pam->IsPerforming
			(
				static_cast<ALYSLC::InputAction>(a_playerActionIndex)
			)
		);
	}

	bool ALYSLCInterface::IsPressingInput
	(
		RE::ActorHandle a_playerActorHandle, uint32_t a_inputIndex
	) const noexcept
	{
		// Return true if the player controlling the character given by the actor handle
		// is pressing the input that corresponds to the given index.
		// See the 'ALYSLC::InputAction' enum in the 'Enums.h' file
		// for the supported input indices.
	
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		// Player must be active, have a valid actor handle, 
		// and the input index must fall within the correct range.
		if (!glob.globalDataInit || 
			!glob.coopSessionActive || 
			!ALYSLC::Util::HandleIsValid(a_playerActorHandle) || 
			a_inputIndex < !ALYSLC::InputAction::kFirst ||
			a_inputIndex >= !ALYSLC::InputAction::kInputTotal)
		{
			return false;
		}

		// Must be a player and, for now, have a device ID in the controller ID range.
		auto pIndex = ALYSLC::GlobalCoopData::GetCoopPlayerIndex(a_playerActorHandle);
		if (pIndex == -1 ||
			glob.coopPlayers[pIndex]->deviceID < 0 ||
			glob.coopPlayers[pIndex]->deviceID >= ALYSLC_MAX_CONTROLLER_COUNT)
		{
			return false;
		}

		return 
		(
			glob.cdh->GetInputState
			(
				glob.coopPlayers[pIndex]->deviceID,
				static_cast<ALYSLC::InputAction>(a_inputIndex)
			).isPressed
		);
	}

	//=============================================================================================
	// [V3]
	//=============================================================================================

	void ALYSLCInterface::AddSkillXP
	(
		RE::ActorHandle a_playerActorHandle, RE::ActorValue a_skillAV, float a_baseXP
	) const noexcept
	{
		// Increment the given player's serialized XP total for the given skill.
		// Factors in the player's specific XP modifier.
		// Shared skills are leveled up directly through P1 
		// and nothing is saved to the serialized data.

		// Enderal has no usage-based skill levelling.
		if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
		{
			return;
		}

		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		// Player must be active, have a valid actor handle, 
		// and the actor value must fall within the correct range.
		if (!glob.globalDataInit || 
			!glob.coopSessionActive || 
			!ALYSLC::Util::HandleIsValid(a_playerActorHandle) || 
			a_skillAV <= RE::ActorValue::kNone ||
			a_skillAV >= RE::ActorValue::kTotal)
		{
			return;
		}
		
		const auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (!p1)
		{
			return;
		}
		
		// Add skill XP directly to P1 for shared skills.
		bool isShared = glob.SHARED_SKILL_AVS_SET.contains(a_skillAV);
		if (isShared)
		{
			p1->UseSkill(a_skillAV, a_baseXP, nullptr);
			return;
		}
		
		ALYSLC::GlobalCoopData::AddSkillXP
		(
			ALYSLC::GlobalCoopData::GetCoopPlayerIndex(a_playerActorHandle), a_skillAV, a_baseXP
		);
	}

	int32_t ALYSLCInterface::GetMenuControlPID() const noexcept
	{
		// Return the player ID for the player currently controlling menus.
		// NOTE:
		// Works even before a co-op session starts.
		
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		if (!glob.globalDataInit || ALYSLC::Util::MenusOnlyAlwaysOpen())
		{
			return -1;
		}

		return glob.menuPID >= 0 && glob.menuPID < ALYSLC_MAX_PLAYER_COUNT ? glob.menuPID : -1;
	}

	RE::ActorHandle ALYSLCInterface::GetMenuControlPlayer() const noexcept
	{
		// Return the actor handle for the player currently controlling menus.
		// NOTE:
		// If the player currently controlling menus does not have an active character,
		// such as before the co-op session starts, this call will return an empty handle.
		
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		// No active co-op session, no controllable menus open, or invalid menu PID.
		if (!glob.globalDataInit || 
			!glob.allPlayersInit ||
			!glob.coopSessionActive ||
			ALYSLC::Util::MenusOnlyAlwaysOpen() ||
			glob.menuPID < 0 ||
			glob.menuPID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return RE::ActorHandle();
		}

		const auto& p = glob.coopPlayers[glob.menuPID];
		if (!p->isActive || !p->coopActor)
		{
			return RE::ActorHandle();
		}

		return p->coopActor->GetHandle();
	}

	void ALYSLCInterface::RequestMenuControl
	(
		int32_t a_playerID, 
		RE::BSFixedString a_menuName, 
		RE::ObjectRefHandle a_assocObjRefHandle
	) const noexcept
	{
		// Insert a request from the given player to control the given menu.
		// NOTE: 
		// Call before the desired menu opens and while a co-op session is active.
		
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		// No global data or invalid player ID.
		if (!glob.globalDataInit ||
			!glob.allPlayersInit ||
			!glob.coopSessionActive ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return;
		}

		bool shouldReset = a_playerID == -1 || glob.coopPlayers[a_playerID]->deviceID == -1;
		if (shouldReset)
		{
			// Reset directly if PID or linked DID is -1.
			ALYSLC::GlobalCoopData::ResetMenuPlayerIDs();
			glob.mim->ToggleCoopPlayerMenuMode(-1, -1);
		}
		else
		{
			// Send a request to resolve later.
			glob.moarm->InsertRequest
			(
				a_playerID,
				ALYSLC::InputAction::kNone, 
				ALYSLC::SteadyClock::now(), 
				a_menuName, 
				a_assocObjRefHandle,
				true
			);
		}
	}
}

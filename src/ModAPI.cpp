#include "ModAPI.h"
#include <GlobalCoopData.h>

namespace ALYSLC_API
{
	RE::ActorHandle ALYSLCInterface::GetALYSLCPlayerByCID(int32_t a_controllerID) const noexcept
	{
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		if (!glob.globalDataInit ||
			!glob.allPlayersInit ||
			!glob.coopSessionActive || 
			a_controllerID < 0 ||
			a_controllerID >= ALYSLC_MAX_PLAYER_COUNT ||
			!glob.coopPlayers[a_controllerID]->isActive)
		{
			return RE::ActorHandle();
		}

		return glob.coopPlayers[a_controllerID]->coopActor->GetHandle();
	}

	RE::ActorHandle ALYSLCInterface::GetALYSLCPlayerByPID(int32_t a_playerID) const noexcept
	{
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		if (!glob.globalDataInit ||
			!glob.allPlayersInit ||
			!glob.coopSessionActive || 
			a_playerID < 0 ||
			a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			return RE::ActorHandle();
		}

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			if (p->playerID == a_playerID)
			{
				return p->coopActor->GetHandle();
			}
		}

		return RE::ActorHandle();
	}

	int32_t ALYSLCInterface::GetALYSLCPlayerCID(RE::ActorHandle a_actorHandle) const noexcept
	{
		// Return the controller ID corresponding to the given actor handle,
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

		const auto pIndex = ALYSLC::GlobalCoopData::GetCoopPlayerIndex(a_actorHandle);
		if (pIndex == -1)
		{
			return -1;
		}

		return glob.coopPlayers[pIndex]->playerID;
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
}

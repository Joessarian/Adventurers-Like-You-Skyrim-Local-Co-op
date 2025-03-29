#pragma once
#include "ALYSLC_API.h"

namespace ALYSLC_API
{
	class ALYSLCInterface : public IVALYSLC1
	{
	public:

		static ALYSLCInterface* GetSingleton() noexcept
		{
			static ALYSLCInterface singleton;
			return std::addressof(singleton);
		}

		/// <summary>
		/// Get the actor for the player with the given controller ID.
		/// </summary>
		/// <returns>
		/// If the controller given by the ID is controlling a player, 
		/// return that player's actor handle.
		/// Otherwise, return an empty handle.
		/// </returns>
		virtual RE::ActorHandle GetALYSLCPlayerByCID(int32_t a_controllerID) const noexcept;

		/// <summary>
		/// Get the actor for the player with the given player ID.
		/// Player 1 always has a player ID of 0, and all active companion players' IDs 
		/// are assigned  sequentially in the order of their XInput controller IDs (CIDs).
		/// Player IDs keep track of player-specific settings and ignore gaps in assigned CIDs.
		/// 0 -> Player 1
		/// 1 -> Player 2
		/// 2 -> Player 3
		/// 3 -> Player 4
		/// </summary>
		/// <returns>
		/// If the player ID is in the range [0, 3] and the corresponding player is active,
		/// return that player's actor handle.
		/// Otherwise, return an empty handle.
		/// </returns>
		virtual RE::ActorHandle GetALYSLCPlayerByPID(int32_t a_playerID) const noexcept;

		/// <summary>
		/// Get the ID for the controller controlling the given player actor.
		/// The controller ID is used to index active players and retrieve information 
		/// on a specific player.
		/// </summary>
		/// <returns>
		/// If the given actor handle corresponds to an active (co-op session started) player, 
		/// return the ID [0, 3] of the controller controlling the actor.
		/// Otherwise, return -1.
		/// </returns>
		virtual int32_t GetALYSLCPlayerCID(RE::ActorHandle a_actorHandle) const noexcept;

		/// <summary>
		/// Check if the given actor handle corresponds to a co-op player.
		/// </summary>
		/// <returns>
		/// True if a co-op player (P1 or companion player NPC), false otherwise.
		/// </returns>
		virtual bool IsALYSLCPlayer(RE::ActorHandle a_actorHandle) const noexcept;

		/// <summary>
		/// Check if there is an active local co-op session.
		/// </summary>
		/// <returns>
		/// True if companion players have been summoned.
		/// False if no players have been summoned yet or all players were dismissed.
		/// </returns>
		virtual bool IsSessionActive() const noexcept;
		
	private:

		ALYSLCInterface() noexcept = default;
		virtual ~ALYSLCInterface() noexcept = default;

		ALYSLCInterface(const ALYSLCInterface& _other) = delete;
		ALYSLCInterface(ALYSLCInterface&& _other) = delete;
		ALYSLCInterface& operator=(const ALYSLCInterface& _other) = delete;
		ALYSLCInterface& operator=(ALYSLCInterface&& _other) = delete;
	};
}

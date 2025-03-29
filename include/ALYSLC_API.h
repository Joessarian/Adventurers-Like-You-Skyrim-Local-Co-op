#pragma once

// For modders: Copy this file into your own project if you wish to use this API.
namespace ALYSLC_API
{
	constexpr const auto ALYSLCPluginName = "ALYSLC.esl";
	constexpr const auto ALYSLCEnderalPluginName = "ALYSLC Enderal.esl";

	// Available ALYSLC interface versions.
	enum class InterfaceVersion : uint8_t
	{
		V1
	};

	// ALYSLC's modder interface.
	// NOTE:
	// Only basic data for now. Will expose more player data in the future. 
	class IVALYSLC1
	{
	public:

		/// <summary>
		/// Get the actor for the player with the given controller ID.
		/// </summary>
		/// <returns>
		/// If the controller given by the ID is controlling a player, 
		/// return that player's actor handle.
		/// Otherwise, return an empty handle.
		/// </returns>
		[[nodiscard]] virtual RE::ActorHandle GetALYSLCPlayerByCID
		(
			int32_t a_controllerID
		) const noexcept = 0;

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
		[[nodiscard]] virtual RE::ActorHandle GetALYSLCPlayerByPID
		(
			int32_t a_playerID
		) const noexcept = 0;

		/// <summary>
		/// Get the ID for the controller controlling the given player actor.
		/// </summary>
		/// <returns>
		/// If the given actor handle corresponds to an active (co-op session started) player, 
		/// return the ID [0, 3] of the controller controlling the actor.
		/// Otherwise, return -1.
		/// </returns>
		[[nodiscard]] virtual int32_t GetALYSLCPlayerCID
		(
			RE::ActorHandle a_actorHandle
		) const noexcept = 0;

		/// <summary>
		/// Check if the given actor handle corresponds to a co-op player.
		/// </summary>
		/// <returns>
		/// True if a co-op player (P1 or companion player NPC), false otherwise.
		/// </returns>
		[[nodiscard]] virtual bool IsALYSLCPlayer
		(
			RE::ActorHandle a_actorHandle
		) const noexcept = 0;

		/// <summary>
		/// Check if there is an active local co-op session.
		/// </summary>
		/// <returns>
		/// True if companion players have been summoned.
		/// False if no players have been summoned yet or all players were dismissed.
		/// </returns>
		[[nodiscard]] virtual bool IsSessionActive() const noexcept = 0;
	};

	typedef void* (*_RequestPluginAPI)(const InterfaceVersion interfaceVersion);

	/// <summary>
	/// Request the ALYSLC interface.
	/// Recommended: Send your request during or after SKSEMessagingInterface::kMessage_PostLoad 
	/// to make sure the dll has already been loaded
	/// </summary>
	/// <param name="a_interfaceVersion">The interface version to request</param>
	/// <returns>The pointer to the API singleton, or nullptr if request failed</returns>
	[[nodiscard]] inline void* RequestPluginAPI
	(
		const InterfaceVersion a_interfaceVersion = InterfaceVersion::V1
	)
	{
		REX::W32::HMODULE pluginHandle = REX::W32::GetModuleHandleA("ALYSLC.dll");
		_RequestPluginAPI requestAPIFunction = 
		(
			(_RequestPluginAPI)GetProcAddress(pluginHandle, "RequestPluginAPI")
		);
		if (requestAPIFunction)
		{
			return requestAPIFunction(a_interfaceVersion);
		}

		return nullptr;
	}
}

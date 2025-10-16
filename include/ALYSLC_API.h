#pragma once

// For modders: Copy this file into your own project if you wish to use this API.
namespace ALYSLC_API
{
	constexpr const auto ALYSLCPluginName = "ALYSLC.esp";
	constexpr const auto ALYSLCEnderalPluginName = "ALYSLC Enderal.esp";

	// Available ALYSLC interface versions.
	enum class InterfaceVersion : uint8_t
	{
		V1,
		V2
	};

	// ALYSLC's modder interface.
	// NOTE:
	// Only basic data for now. Will expose more player data in the future. 

	// Session info and active player/character checks.
	class IVALYSLC1
	{
	public:

		/// <summary>
		/// Get the actor for the player with the given device ID.
		/// Controller IDs fall in the range [0, 3] and keyboard + mouse IDs are >= 4.
		/// </summary>
		/// <returns>
		/// If the device given by the ID is controlling a player, 
		/// return that player's actor handle.
		/// Otherwise, return an empty handle.
		/// </returns>
		[[nodiscard]] virtual RE::ActorHandle GetALYSLCPlayerByDID
		(
			int32_t a_deviceID
		) const noexcept = 0;

		/// <summary>
		/// Get the actor for the player with the given player ID.
		/// Player 1 always has a player ID of 0, and all active companion players' IDs 
		/// are assigned sequentially in the order of their device IDs (DIDs).
		/// The player ID is used to index active players,
		/// keep track of player-specific settings,
		/// and retrieve information on a specific player.
		/// It ignores gaps in assigned DIDs.
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
		/// Get the ID for the device controlling the given player actor.
		/// The device ID is used to retrieve input device state for players.
		/// Controller IDs fall in the range [0, 3] and keyboard + mouse IDs are >= 4.
		/// </summary>
		/// <returns>
		/// If the given actor handle corresponds to an active (co-op session started) player, 
		/// return the ID of the device controlling the actor.
		/// Otherwise, return -1.
		/// </returns>
		[[nodiscard]] virtual int32_t GetALYSLCPlayerDID
		(
			RE::ActorHandle a_actorHandle
		) const noexcept = 0;

		/// <summary>
		/// Get the player ID for the player controlling the given player actor.
		/// </summary>
		/// <returns>
		/// If the given actor handle corresponds to an active (co-op session started) player, 
		/// return the ID [0, 3] of the player controlling the actor.
		/// Otherwise, return -1.
		/// </returns>
		[[nodiscard]] virtual int32_t GetALYSLCPlayerPID
		(
			RE::ActorHandle a_actorHandle
		) const noexcept = 0;

		/// <summary>
		/// Check if the given actor handle corresponds to a character 
		/// that is controllable by a co-op player.
		/// A co-op session does not have to be active.
		/// </summary>
		/// <returns>
		/// True if a co-op character (P1 or companion player NPC), false otherwise.
		/// </returns>
		[[nodiscard]] virtual bool IsALYSLCCharacter
		(
			RE::ActorHandle a_actorHandle
		) const noexcept = 0;

		/// <summary>
		/// Check if the given actor handle corresponds to an active co-op player.
		/// </summary>
		/// <returns>
		/// True if an active co-op player character (P1 or companion player NPC), false otherwise.
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

	// Player equip, movement, player action, and targeting state. 
	class IVALYSLC2 : IVALYSLC1
	{
	public:
		/// <summary>
		/// Check if the player controlling the character with the given actor handle
		/// is performing the action that corresponds to the given index.
		/// </summary>
		/// <returns>
		/// True if the player is performing the action.
		/// False if the player is not performing the action.
		/// </returns>
		[[nodiscard]] virtual bool IsPerformingAction
		(
			RE::ActorHandle a_playerActorHandle,
			uint32_t a_playerActionIndex
		) const noexcept = 0;

		/// <summary>
		/// Check if the player controlling the character with the given actor handle
		/// is pressing the button/key or moving the analog stick/mouse 
		/// that corresponds to the given input action index.
		/// </summary>
		/// <returns>
		/// True if the player is pressing the input.
		/// False if the player is not pressing the input.
		/// </returns>
		[[nodiscard]] virtual bool IsPressingInput
		(
			RE::ActorHandle a_playerActorHandle,
			uint32_t a_inputIndex
		) const noexcept = 0;
	};
	
	// Skill progression and menu control data.
	class IVALYSLC3 : IVALYSLC2
	{
	public:
		/// <summary>
		/// Add experience points to the given skill for the player given by the actor handle.
		/// Shared skills (Alchemy, Enchanting, Lockpicking, Pickpocket, Speech, Smithing)
		/// progress directly through player 1.
		/// The new skill XP amount for the skill is saved afterward 
		/// to the the player's serializable data.
		/// </summary>
		/// <returns> Nothing. </returns>
		[[nodiscard]] virtual void AddSkillXP
		(
			RE::ActorHandle a_playerActorHandle,
			RE::ActorValue a_skillAV,
			float a_baseXP
		) const noexcept = 0;
		
		/// <summary>
		/// Gets the player ID for the player currently controlling any open menus.
		/// </summary>
		/// <returns> 
		/// The player ID of the player currently controlling menus,
		/// or -1 if no controllable menu is open.
		/// </returns>
		[[nodiscard]] virtual int32_t GetMenuControlPID() const noexcept = 0;

		/// <summary>
		/// Gets the actor handle for the player currently controlling any open menus.
		/// </summary>
		/// <returns> 
		/// The actor handle for the player currently controlling menus,
		/// or an empty handle if no controllable menu is open.
		/// </returns>
		[[nodiscard]] virtual RE::ActorHandle GetMenuControlPlayer() const noexcept = 0;

		/// <summary>
		/// IMPORTANT: Call before the desired menu opens and while a co-op session is active.
		/// While a co-op session is active, request menu control for the given menu
		/// be granted to the player corresponding to the given player ID.
		/// Specify '-1' as the player ID to relinquish control.
		/// Can optionally specify an object that triggered the menu 
		/// to better link the request to the player.
		/// (ex. a 'pull chain' as the object that triggers a message box)
		/// </summary>
		/// <returns> Nothing. </returns>
		[[nodiscard]] virtual void RequestMenuControl
		(
			int32_t a_playerID,
			RE::BSFixedString a_menuName,
			RE::ObjectRefHandle a_assocObjRefHandle
		) const noexcept = 0;
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

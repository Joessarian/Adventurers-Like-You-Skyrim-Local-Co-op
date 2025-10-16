#pragma once
#include "ALYSLC_API.h"

namespace ALYSLC_API
{
	class ALYSLCInterface : public IVALYSLC3
	{
	public:
		static ALYSLCInterface* GetSingleton() noexcept
		{
			static ALYSLCInterface singleton;
			return std::addressof(singleton);
		}

		// 
		// [V1]
		//

		/// <summary>
		/// Get the actor for the player with the given device ID.
		/// Controller IDs fall in the range [0, 3] and keyboard + mouse IDs are >= 4.
		/// </summary>
		/// <returns>
		/// If the device given by the ID is controlling a player, 
		/// return that player's actor handle.
		/// Otherwise, return an empty handle.
		/// </returns>
		virtual RE::ActorHandle GetALYSLCPlayerByDID(int32_t a_deviceID) const noexcept;

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
		virtual RE::ActorHandle GetALYSLCPlayerByPID(int32_t a_playerID) const noexcept;

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
		virtual int32_t GetALYSLCPlayerDID(RE::ActorHandle a_actorHandle) const noexcept;

		/// <summary>
		/// Get the player ID for the player controlling the given player actor.
		/// </summary>
		/// <returns>
		/// If the given actor handle corresponds to an active (co-op session started) player, 
		/// return the ID [0, 3] of the player controlling the actor.
		/// Otherwise, return -1.
		/// </returns>
		virtual int32_t GetALYSLCPlayerPID(RE::ActorHandle a_actorHandle) const noexcept;
		
		/// <summary>
		/// Check if the given actor handle corresponds to a character 
		/// that is controllable by a co-op player.
		/// A co-op session does not have to be active.
		/// </summary>
		/// <returns>
		/// True if a co-op character (P1 or companion player NPC), false otherwise.
		/// </returns>
		virtual bool IsALYSLCCharacter(RE::ActorHandle a_actorHandle) const noexcept;

		/// <summary>
		/// Check if the given actor handle corresponds to an active co-op player.
		/// </summary>
		/// <returns>
		/// True if an active co-op player character (P1 or companion player NPC), false otherwise.
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

		//
		// [V2]
		//

		/// <summary>
		/// Check if the player controlling the character with the given actor handle
		/// is performing the action that corresponds to the given index.
		/// See the 'ALYSLC::InputAction' enum in the 'Enums.h' file
		/// for the supported action indices after and including 'kFirstAction'.
		/// </summary>
		/// <returns>
		/// True if the player is performing the action.
		/// False if the player is not performing the action.
		/// </returns>
		[[nodiscard]] virtual bool IsPerformingAction
		(
			RE::ActorHandle a_playerActorHandle,
			uint32_t a_playerActionIndex
		) const noexcept;

		/// <summary>
		/// Check if the player controlling the character with the given actor handle
		/// is pressing the button/key or moving the analog stick/mouse 
		/// that corresponds to the given input action index.
		/// See the 'ALYSLC::InputAction' enum in the 'Enums.h' file
		/// for the supported action indices before 'kInputTotal'.
		/// </summary>
		/// <returns>
		/// True if the player is pressing the input.
		/// False if the player is not pressing the input.
		/// </returns>
		[[nodiscard]] virtual bool IsPressingInput
		(
			RE::ActorHandle a_playerActorHandle,
			uint32_t a_inputIndex
		) const noexcept;

		//
		// [V3]
		//

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
		) const noexcept;
		
		/// <summary>
		/// Gets the player ID for the player currently controlling any open menus.
		/// </summary>
		/// <returns> 
		/// The player ID of the player currently controlling menus,
		/// or -1 if no controllable menu is open.
		/// </returns>
		[[nodiscard]] virtual int32_t GetMenuControlPID() const noexcept;

		/// <summary>
		/// Gets the actor handle for the player currently controlling any open menus.
		/// </summary>
		/// <returns> 
		/// The actor handle for the player currently controlling menus,
		/// or an empty handle if no controllable menu is open.
		/// </returns>
		[[nodiscard]] virtual RE::ActorHandle GetMenuControlPlayer() const noexcept;

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
		) const noexcept;
		
	private:
		ALYSLCInterface() noexcept = default;
		virtual ~ALYSLCInterface() noexcept = default;

		ALYSLCInterface(const ALYSLCInterface& _other) = delete;
		ALYSLCInterface(ALYSLCInterface&& _other) = delete;
		ALYSLCInterface& operator=(const ALYSLCInterface& _other) = delete;
		ALYSLCInterface& operator=(ALYSLCInterface&& _other) = delete;
	};
}

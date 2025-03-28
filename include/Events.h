#pragma once
#include <queue>
#include <Player.h>

namespace ALYSLC
{
	namespace Events
	{
		// Register all event handlers to receive events.
		void RegisterEvents();
		// Close MIM, reset menu CIDs, and reset open supported menus flag and time point.
		void ResetMenuState();
	}

	class CoopActorKillEventHandler : public RE::BSTEventSink<RE::ActorKill::Event>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		
		static CoopActorKillEventHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::ActorKill::Event* a_actorKillEvent,
			RE::BSTEventSource<RE::ActorKill::Event>*
		) override;
	
	private:
		CoopActorKillEventHandler() = default;
		CoopActorKillEventHandler(const CoopActorKillEventHandler& a_cakeh) = delete;
		CoopActorKillEventHandler(CoopActorKillEventHandler&& a_cakeh) = delete;
		~CoopActorKillEventHandler() = default;
		CoopActorKillEventHandler& operator=(const CoopActorKillEventHandler& a_cakeh) = delete;
		CoopActorKillEventHandler& operator=(CoopActorKillEventHandler&& a_cakeh) = delete;
	};

	class CoopBleedoutEventHandler : public RE::BSTEventSink<RE::TESEnterBleedoutEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		
		static CoopBleedoutEventHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::TESEnterBleedoutEvent* a_bleedoutEvent, 
			RE::BSTEventSource<RE::TESEnterBleedoutEvent>*
		) override;

	private:
		CoopBleedoutEventHandler() = default;
		CoopBleedoutEventHandler(const CoopBleedoutEventHandler& a_cbeh) = delete;
		CoopBleedoutEventHandler(CoopBleedoutEventHandler&& a_cbeh) = delete;
		~CoopBleedoutEventHandler() = default;
		CoopBleedoutEventHandler& operator=(const CoopBleedoutEventHandler& a_cbeh) = delete;
		CoopBleedoutEventHandler& operator=(CoopBleedoutEventHandler&& a_cbeh) = delete;
	};

	class CoopCellChangeHandler : public RE::BSTEventSink<RE::TESMoveAttachDetachEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		
		static CoopCellChangeHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::TESMoveAttachDetachEvent* a_cellChangeEvent, 
			RE::BSTEventSource<RE::TESMoveAttachDetachEvent>*
		) override;
	
	private:
		CoopCellChangeHandler() = default;
		CoopCellChangeHandler(const CoopCellChangeHandler& a_ccch) = delete;
		CoopCellChangeHandler(CoopCellChangeHandler&& a_ccch) = delete;
		~CoopCellChangeHandler() = default;
		CoopCellChangeHandler& operator=(const CoopCellChangeHandler& a_ccch) = delete;
		CoopCellChangeHandler& operator=(CoopCellChangeHandler&& a_ccch) = delete;
	};

	class CoopCellFullyLoadedHandler : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		
		static CoopCellFullyLoadedHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::TESCellFullyLoadedEvent* a_cellFullyLoadedEvent,
			RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*
		) override;
		
		static inline uint32_t lastLoadP1CellNameHash = Hash(""sv);
	
	private:
		CoopCellFullyLoadedHandler() = default;
		CoopCellFullyLoadedHandler(const CoopCellFullyLoadedHandler& a_cflh) = delete;
		CoopCellFullyLoadedHandler(CoopCellFullyLoadedHandler&& a_cflh) = delete;
		~CoopCellFullyLoadedHandler() = default;
		CoopCellFullyLoadedHandler& operator=(const CoopCellFullyLoadedHandler& a_cflh) = delete;
		CoopCellFullyLoadedHandler& operator=(CoopCellFullyLoadedHandler&& a_cflh) = delete;
	};

	class CoopCombatEventHandler : public RE::BSTEventSink<RE::TESCombatEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		
		static CoopCombatEventHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::TESCombatEvent* a_combatEvent, 
			RE::BSTEventSource<RE::TESCombatEvent>*
		) override;
	
	private:
		CoopCombatEventHandler() = default;
		CoopCombatEventHandler(const CoopCombatEventHandler& a_cceh) = delete;
		CoopCombatEventHandler(CoopCombatEventHandler&& a_cceh) = delete;
		~CoopCombatEventHandler() = default;
		CoopCombatEventHandler& operator=(const CoopCombatEventHandler& a_cceh) = delete;
		CoopCombatEventHandler& operator=(CoopCombatEventHandler&& a_cceh) = delete;
	};

	class CoopContainerChangedHandler : public RE::BSTEventSink<RE::TESContainerChangedEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		
		static CoopContainerChangedHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::TESContainerChangedEvent* a_containerChangedEvent, 
			RE::BSTEventSource<RE::TESContainerChangedEvent>*
		) override;
	
	private:
		CoopContainerChangedHandler() = default;
		CoopContainerChangedHandler(const CoopContainerChangedHandler& a_ccch) = delete;
		CoopContainerChangedHandler(CoopContainerChangedHandler&& a_ccch) = delete;
		~CoopContainerChangedHandler() = default;
		CoopContainerChangedHandler& operator=(const CoopContainerChangedHandler& a_ccch) = delete;
		CoopContainerChangedHandler& operator=(CoopContainerChangedHandler&& a_ccch) = delete;
	};

	class CoopCrosshairEventHandler : public RE::BSTEventSink<SKSE::CrosshairRefEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		
		static CoopCrosshairEventHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const SKSE::CrosshairRefEvent* a_event, 
			RE::BSTEventSource<SKSE::CrosshairRefEvent>*
		) override;

	private:
		CoopCrosshairEventHandler() = default;
		CoopCrosshairEventHandler(const CoopCrosshairEventHandler& a_cceh) = delete;
		CoopCrosshairEventHandler(CoopCrosshairEventHandler&& a_cceh) = delete;
		~CoopCrosshairEventHandler() = default;
		CoopCrosshairEventHandler& operator=(const CoopCrosshairEventHandler& a_cceh) = delete;
		CoopCrosshairEventHandler& operator=(CoopCrosshairEventHandler&& a_cceh) = delete;
	};

	class CoopDeathEventHandler : public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
		
		static CoopDeathEventHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::TESDeathEvent* a_deathEvent,
			RE::BSTEventSource<RE::TESDeathEvent>* a_source
		) override;
	
	private:
		CoopDeathEventHandler() = default;
		CoopDeathEventHandler(const CoopDeathEventHandler& a_cdeh) = delete;
		CoopDeathEventHandler(CoopDeathEventHandler&& a_cdeh) = delete;
		~CoopDeathEventHandler() = default;
		CoopDeathEventHandler& operator=(const CoopDeathEventHandler& a_cdeh) = delete;
		CoopDeathEventHandler& operator=(CoopDeathEventHandler&& a_cdeh) = delete;
	};

	class CoopEquipEventHandler : public RE::BSTEventSink<RE::TESEquipEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
	
		static CoopEquipEventHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::TESEquipEvent* a_equipEvent, 
			RE::BSTEventSource<RE::TESEquipEvent>*
		) override;
	
	private:
		CoopEquipEventHandler() = default;
		CoopEquipEventHandler(const CoopEquipEventHandler& a_ceeh) = delete;
		CoopEquipEventHandler(CoopEquipEventHandler&& a_ceeh) = delete;
		~CoopEquipEventHandler() = default;
		CoopEquipEventHandler& operator=(const CoopEquipEventHandler& a_ceeh) = delete;
		CoopEquipEventHandler& operator=(CoopEquipEventHandler&& a_ceeh) = delete;
	};

	class CoopLoadGameEventHandler : public RE::BSTEventSink<RE::TESLoadGameEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
	
		static CoopLoadGameEventHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::TESLoadGameEvent* a_loadGameEvent, 
			RE::BSTEventSource<RE::TESLoadGameEvent>*
		) override;
	
	private:
		CoopLoadGameEventHandler() = default;
		CoopLoadGameEventHandler(const CoopLoadGameEventHandler& a_clgeh) = delete;
		CoopLoadGameEventHandler(CoopLoadGameEventHandler&& a_clgeh) = delete;
		~CoopLoadGameEventHandler() = default;
		CoopLoadGameEventHandler& operator=(const CoopLoadGameEventHandler& a_clgeh) = delete;
		CoopLoadGameEventHandler& operator=(CoopLoadGameEventHandler&& a_clgeh) = delete;
	};

	class CoopHitEventHandler : public RE::BSTEventSink<RE::TESHitEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
	
		static CoopHitEventHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::TESHitEvent* a_hitEvent, 
			RE::BSTEventSource<RE::TESHitEvent>*
		) override;
	
	private:
		CoopHitEventHandler() = default;
		CoopHitEventHandler(const CoopHitEventHandler& a_cheh) = delete;
		CoopHitEventHandler(CoopHitEventHandler&& a_cheh) = delete;
		~CoopHitEventHandler() = default;
		CoopHitEventHandler& operator=(const CoopHitEventHandler& a_cheh) = delete;
		CoopHitEventHandler& operator=(CoopHitEventHandler&& a_cheh) = delete;
	};

	class CoopMenuOpenCloseHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
	
		static CoopMenuOpenCloseHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::MenuOpenCloseEvent* a_menuEvent, 
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*
		) override;
	
	private:
		CoopMenuOpenCloseHandler() = default;
		CoopMenuOpenCloseHandler(const CoopMenuOpenCloseHandler& a_cmoch) = delete;
		CoopMenuOpenCloseHandler(CoopMenuOpenCloseHandler&& a_cmoch) = delete;
		~CoopMenuOpenCloseHandler() = default;
		CoopMenuOpenCloseHandler& operator=(const CoopMenuOpenCloseHandler& a_cmoch) = delete;
		CoopMenuOpenCloseHandler& operator=(CoopMenuOpenCloseHandler&& a_cmoch) = delete;
	};

	class CoopPositionPlayerEventHandler : public RE::BSTEventSink<RE::PositionPlayerEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;
	
		static CoopPositionPlayerEventHandler* GetSingleton();
		
		static void Register();
		
		EventResult ProcessEvent
		(
			const RE::PositionPlayerEvent* a_positionPlayerEvent, 
			RE::BSTEventSource<RE::PositionPlayerEvent>* a_eventSource
		) override;
	
	private:
		CoopPositionPlayerEventHandler() = default;
		CoopPositionPlayerEventHandler(const CoopPositionPlayerEventHandler& a_cppeh) = delete;
		CoopPositionPlayerEventHandler(CoopPositionPlayerEventHandler&& a_cppeh) = delete;
		~CoopPositionPlayerEventHandler() = default;
		CoopPositionPlayerEventHandler& operator=
		(
			const CoopPositionPlayerEventHandler& a_cppeh
		) = delete;
		CoopPositionPlayerEventHandler& operator=
		(
			CoopPositionPlayerEventHandler&& a_cppeh
		) = delete;
	};
}

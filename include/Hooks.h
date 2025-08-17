#pragma once
#include <Compatibility.h>
#include <Player.h>
#include <Util.h>

namespace ALYSLC
{
	using SteadyClock = std::chrono::steady_clock;
	namespace Hooks
	{
		using EventResult = RE::BSEventNotifyControl;
		void Install();

		// [Main Hook]
		// Credits to Shrimperator and dTry for the first hook location:
		// https://gitlab.com/Shrimperator/skyrim-mod-betterthirdpersonselection/-/blob/main/src/RevE/Hooks.cpp#L42
		// https://github.com/D7ry/valhallaCombat/blob/Master/src/include/Hooks.h#L124
		class MainHook
		{
		public:
			static void InstallHook()
			{
				auto& trampoline = SKSE::GetTrampoline();
				REL::Relocation<uintptr_t> hook{ RELOCATION_ID(35551, 36544) };
				_Update = trampoline.write_call<5>(hook.address() + OFFSET(0x11F, 0x160), Update);
				SPDLOG_INFO("[MainHook] Installed Update() hook");
			}

		private:
			static void Update(RE::Main* a_this, float a_a2);
			static inline REL::Relocation<decltype(Update)> _Update;
		};

		// [ActorEquipManager Hooks]
		// Credits to po3 for the equip and unequip hook locations:
		// https://github.com/powerof3/Spell-Perk-Item-Distributor/blob/master/SPID/src/Outfits/OutfitManager%2BHooks.cpp#L225
		// https://github.com/powerof3/Spell-Perk-Item-Distributor/blob/master/SPID/src/Outfits/OutfitManager%2BHooks.cpp#L246
		class ActorEquipManagerHooks
		{
		public:
			static void InstallHooks()
			{
				auto& trampoline = SKSE::GetTrampoline();
				REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37938, 38894) };
				REL::Relocation<uintptr_t> hook2{ RELOCATION_ID(37945, 38901) };
				_EquipObject = trampoline.write_call<5>
				(
					hook.address() + 
					OFFSET(0xE5, 0x170), 
					EquipObject
				);
				SPDLOG_INFO("[ActorEquipManagerHooks] Installed EquipObject() hook.");
				_UnequipObject = trampoline.write_call<5>
				(
					hook2.address() + 
					OFFSET(0x138, 0x1B9), 
					UnequipObject
				);
				SPDLOG_INFO("[ActorEquipManagerHooks] Installed UnequipObject() hook.");
			}

		private:
			static void EquipObject
			(
				RE::ActorEquipManager* a_this,
				RE::Actor* a_actor, 
				RE::TESBoundObject* a_object, 
				const RE::ObjectEquipParams& a_objectEquipParams
			);
			static void UnequipObject
			(
				RE::ActorEquipManager* a_this, 
				RE::Actor* a_actor, 
				RE::TESBoundObject* a_object, 
				const RE::ObjectEquipParams& a_objectEquipParams
			);
			static inline REL::Relocation<decltype(EquipObject)> _EquipObject;
			static inline REL::Relocation<decltype(UnequipObject)> _UnequipObject;
		};

		// [ActorMagicCaster Hooks]
		class ActorMagicCasterHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_ActorMagicCaster[0] };
				/*_DeselectSpellImpl = vtbl.write_vfunc(0x12, DeselectSpellImpl);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed DeselectSpellImpl() hook.");
				_FinishCastImpl = vtbl.write_vfunc(0x07, FinishCastImpl);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed FinishCastImpl() hook.");*/
				_InterruptCastImpl = vtbl.write_vfunc(0x08, InterruptCastImpl);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed InterruptCastImpl() hook.");
				_RequestCastImpl = vtbl.write_vfunc(0x03, RequestCastImpl);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed RequestCastImpl() hook.");
				/*_SelectSpellImpl = vtbl.write_vfunc(0x11, SelectSpellImpl);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed SelectSpellImpl() hook.");
				_SetCurrentSpellImpl = vtbl.write_vfunc(0x10, SetCurrentSpellImpl);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed SetCurrentSpellImpl() hook.");
				_SpellCast = vtbl.write_vfunc(0x09, SpellCast);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed SpellCast() hook.");
				_StartCastImpl = vtbl.write_vfunc(0x06, StartCastImpl);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed StartCastImpl() hook.");*/
				_StartChargeImpl = vtbl.write_vfunc(0x04, StartChargeImpl);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed StartChargeImpl() hook.");
				/*_StartReadyImpl = vtbl.write_vfunc(0x05, StartReadyImpl);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed StartReadyImpl() hook.");*/
				_Update = vtbl.write_vfunc(0x1D, Update);
				SPDLOG_INFO("[ActorMagicCasterHooks Hook] Installed Update() hook.");
			}

		private:
			static void DeselectSpellImpl(RE::ActorMagicCaster* a_this);
			static void FinishCastImpl(RE::ActorMagicCaster* a_this);
			static void InterruptCastImpl(RE::ActorMagicCaster* a_this, bool a_depleteEnergy);
			static void RequestCastImpl(RE::ActorMagicCaster* a_this);
			static void SelectSpellImpl(RE::ActorMagicCaster* a_this);
			static void SetCurrentSpellImpl(RE::ActorMagicCaster* a_this, RE::MagicItem* a_spell);
			static void SpellCast
			(
				RE::ActorMagicCaster* a_this,
				bool a_doCast, 
				uint32_t a_arg2, 
				RE::MagicItem* a_spell
			);
			static void StartCastImpl(RE::ActorMagicCaster* a_this);
			static bool StartChargeImpl(RE::ActorMagicCaster* a_this);
			static void StartReadyImpl(RE::ActorMagicCaster* a_this);
			static void Update(RE::ActorMagicCaster* a_this, float a_delta);
			
			static inline REL::Relocation<decltype(DeselectSpellImpl)> _DeselectSpellImpl;
			static inline REL::Relocation<decltype(FinishCastImpl)> _FinishCastImpl;
			static inline REL::Relocation<decltype(InterruptCastImpl)> _InterruptCastImpl;
			static inline REL::Relocation<decltype(RequestCastImpl)> _RequestCastImpl;
			static inline REL::Relocation<decltype(SelectSpellImpl)> _SelectSpellImpl;
			static inline REL::Relocation<decltype(SetCurrentSpellImpl)> _SetCurrentSpellImpl;
			static inline REL::Relocation<decltype(SpellCast)> _SpellCast;
			static inline REL::Relocation<decltype(StartCastImpl)> _StartCastImpl;
			static inline REL::Relocation<decltype(StartChargeImpl)> _StartChargeImpl;
			static inline REL::Relocation<decltype(StartReadyImpl)> _StartReadyImpl;
			static inline REL::Relocation<decltype(Update)> _Update;
		};

		// [AIProcess Hooks]
		// Credits to ersh1:
		// https://github.com/ersh1/TrueDirectionalMovement/blob/master/src/Hooks.h#L318
		class AIProcessHooks
		{
		public:
			static void InstallHooks()
			{
				auto& trampoline = SKSE::GetTrampoline();
				REL::Relocation<uintptr_t> hook1{ RELOCATION_ID(36365, 37356) };
				REL::Relocation<uintptr_t> hook2{ RELOCATION_ID(41293, 42373) };

				_AIProcess_SetRotationSpeedZ1 = trampoline.write_call<5>
				(
					hook1.address() + OFFSET(0x356, 0x3EF), AIProcess_SetRotationSpeedZ1
				);
				_AIProcess_SetRotationSpeedZ2 = trampoline.write_call<5>
				(
					hook1.address() + OFFSET(0x5E4, 0x632), AIProcess_SetRotationSpeedZ2
				);
				_AIProcess_SetRotationSpeedZ3 = trampoline.write_branch<5>
				(
					hook2.address() + OFFSET(0x49, 0x49), AIProcess_SetRotationSpeedZ3
				);
			}

		private:
			static void AIProcess_SetRotationSpeedZ1(RE::AIProcess* a_this, float a_rotationSpeed);
			static void AIProcess_SetRotationSpeedZ2(RE::AIProcess* a_this, float a_rotationSpeed);
			static void AIProcess_SetRotationSpeedZ3(RE::AIProcess* a_this, float a_rotationSpeed);
			static inline REL::Relocation<decltype(AIProcess_SetRotationSpeedZ1)> 
			_AIProcess_SetRotationSpeedZ1;
			static inline REL::Relocation<decltype(AIProcess_SetRotationSpeedZ2)> 
			_AIProcess_SetRotationSpeedZ2;
			static inline REL::Relocation<decltype(AIProcess_SetRotationSpeedZ3)> 
			_AIProcess_SetRotationSpeedZ3;
		};

		// [AnimationGraphManager Hooks]
		class AnimationGraphManagerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_BSAnimationGraphManager[0] };
				_ProcessEvent = vtbl.write_vfunc(0x01, ProcessEvent);
				SPDLOG_INFO("[AnimationGraphManager Hook] Installed ProcessEvent() hook.");
			}

		private:
			static EventResult ProcessEvent
			(
				RE::BSAnimationGraphManager* a_this, 
				const RE::BSAnimationGraphEvent* a_event, 
				RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource
			);
			static inline REL::Relocation<decltype(ProcessEvent)> _ProcessEvent;
		};

		// [BSMultiBound Hooks]
		class BSMultiBoundHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_BSMultiBound[0] };
				_QWithinPoint = vtbl.write_vfunc(0x25, QWithinPoint);
				SPDLOG_INFO("[BSMultiBound Hook] Installed QWithinPoint() hook.");
			}

		private:
			static bool QWithinPoint(RE::BSMultiBound* a_this, const RE::NiPoint3& a_pos);
			static inline REL::Relocation<decltype(QWithinPoint)> _QWithinPoint;
		};

		// [Character Hooks]
		class CharacterHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_Character[0] };
				REL::Relocation<uintptr_t> vtbl3{ RE::VTABLE_Character[3] };

				_CheckClampDamageModifier = vtbl.write_vfunc(0x127, CheckClampDamageModifier);
				SPDLOG_INFO("[Character Hook] Installed CheckClampDamageModifier() hook.");
				_DrawWeaponMagicHands = vtbl.write_vfunc(0xA6, DrawWeaponMagicHands);
				SPDLOG_INFO("[Character Hook] Installed DrawWeaponMagicHands() hook.");
				/*_GetCombatGroup = vtbl.write_vfunc(0xD4, GetCombatGroup);
				SPDLOG_INFO("[Character Hook] Installed GetCombatGroup() hook.");*/
				_HandleHealthDamage = vtbl.write_vfunc(0x104, HandleHealthDamage);
				SPDLOG_INFO("[Character Hook] Installed HandleHealthDamage() hook.");
				_ModifyAnimationUpdateData = vtbl.write_vfunc(0x79, ModifyAnimationUpdateData);
				SPDLOG_INFO("[Character Hook] Installed ModifyAnimationUpdateData() hook.");
				_NotifyAnimationGraph = vtbl3.write_vfunc(0x01, NotifyAnimationGraph);
				SPDLOG_INFO("[Character Hook] Installed NotifyAnimationGraph() hook.");
				_PutCreatedPackage = vtbl.write_vfunc(0xDF, PutCreatedPackage);
				SPDLOG_INFO("[Character Hook] Installed PutCreatedPackage() hook.");
				_RemoveWeapon = vtbl.write_vfunc(0x82, RemoveWeapon);
				SPDLOG_INFO("[Character Hook] Installed RemoveWeapon() hook.");
				_ResetInventory = vtbl.write_vfunc(0x8A, ResetInventory);
				SPDLOG_INFO("[Character Hook] Installed ResetInventory() hook.");
				_SetCurrentScene = vtbl.write_vfunc(0x4B, SetCurrentScene);
				SPDLOG_INFO("[Character Hook] Installed SetCurrentScene() hook.");
				_Update = vtbl.write_vfunc(0xAD, Update);
				SPDLOG_INFO("[Character Hook] Installed Update() hook.");
			}

		private:
			static float CheckClampDamageModifier
			(
				RE::Character* a_this, RE::ActorValue a_av, float a_delta
			);
			static void DrawWeaponMagicHands(RE::Character* a_this, bool a_draw);
			static RE::CombatGroup* GetCombatGroup(RE::Character* a_this);
			static void HandleHealthDamage
			(
				RE::Character* a_this, RE::Actor* a_attacker, float a_damage
			);
			static void ModifyAnimationUpdateData
			(
				RE::Character* a_this, RE::BSAnimationUpdateData& a_data
			);
			static bool NotifyAnimationGraph
			(
				RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName
			);
			static void PutCreatedPackage
			(
				RE::Character* a_this, 
				RE::TESPackage* a_package, 
				bool a_tempPackage,
				bool a_createdPackage
			);
			static void RemoveWeapon(RE::Character* a_this, RE::BIPED_OBJECT a_equipIndex);
			static void ResetInventory(RE::Character* a_this, bool a_leveledOnly);
			static void SetCurrentScene(RE::Character* a_this, RE::BGSScene* a_scene);
			static void Update(RE::Character* a_this, float a_delta);

			static inline REL::Relocation<decltype(CheckClampDamageModifier)> 
			_CheckClampDamageModifier;
			static inline REL::Relocation<decltype(DrawWeaponMagicHands)> _DrawWeaponMagicHands;
			static inline REL::Relocation<decltype(GetCombatGroup)> _GetCombatGroup;
			static inline REL::Relocation<decltype(HandleHealthDamage)> _HandleHealthDamage;
			static inline REL::Relocation<decltype(ModifyAnimationUpdateData)> 
			_ModifyAnimationUpdateData;
			static inline REL::Relocation<decltype(NotifyAnimationGraph)> _NotifyAnimationGraph;
			static inline REL::Relocation<decltype(PutCreatedPackage)> _PutCreatedPackage;
			static inline REL::Relocation<decltype(RemoveWeapon)> _RemoveWeapon;
			static inline REL::Relocation<decltype(ResetInventory)> _ResetInventory;
			static inline REL::Relocation<decltype(SetCurrentScene)> _SetCurrentScene;
			static inline REL::Relocation<decltype(Update)> _Update;
		};

		// [Melee Hooks]
		// Credits to dTry:
		// https://github.com/D7ry/valhallaCombat/blob/Master/src/include/Hooks.h#L61
		class MeleeHitHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37673, 38627) };	 //628C20, 64E760
				auto& trampoline = SKSE::GetTrampoline();
				_ProcessHit = trampoline.write_call<5>
				(
					hook.address() + 
					OFFSET(0x3C0, 0x4A8), 
					ProcessHit
				);
				SPDLOG_INFO("[MeleeHit Hook] Installed ProcessHit() hook.");
			}

		private:
			static void ProcessHit(RE::Actor* a_victim, RE::HitData& a_hitData);
			static inline REL::Relocation<decltype(ProcessHit)> _ProcessHit;  //140626400
		};

		// [MenuControls Hooks]
		class MenuControlsHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_MenuControls[0] };
				_ProcessEvent = vtbl.write_vfunc(0x01, ProcessEvent);
				SPDLOG_INFO("[MenuControls Hook] Installed ProcessEvent() hook.");
				debugMenuTriggered = 
				ignoringPauseWaitEvent =
				pauseBindPressedFirst = 
				summoningMenuTriggered = false;
			}

		private:
			static EventResult ProcessEvent
			(
				RE::MenuControls* a_this, 
				RE::InputEvent* const* a_event,
				RE::BSTEventSource<RE::InputEvent*>* a_eventSource
			);
			static inline REL::Relocation<decltype(ProcessEvent)> _ProcessEvent;

			// NOTE:
			// The following three checks only process gamepad events.
			
			// Check if the correct binds were pressed to open the summoning or debug menus.
			// Return true if the input was handled and should be invalidated.
			static bool CheckForMenuTriggeringInput(RE::InputEvent* a_firstGamepadEvent);
			// 1. Check if P1 is in the Favorites Menu and is trying to hotkey an entry
			// and update its hotkey state accordingly.
			// 2. Check if P1 is in the Favorites Menu and is trying to equip 
			// a quick slot spell/item and (un)equip this item as needed.
			// 3. Check if P1 is in the Favorites Menu and toggle SMORF state if needed.
			// Return true if the input was handled and should be invalidated.
			static bool CheckForP1FavoritesMenuInput(RE::InputEvent* a_firstGamepadEvent);
			// Check if P1 is in the Favorites Menu and is trying to hotkey an entry.
			// Return true if the input was handled and should be invalidated.
			static bool CheckForP1HotkeyReq(RE::InputEvent* a_firstGamepadEvent);
			// Check if P1 is in the Favorites Menu and is trying to equip a quick slot spell/item.
			// Return true if the input was handled and should be invalidated.
			static bool CheckForP1QSEquipReq(RE::InputEvent* a_firstGamepadEvent);
			// Check if P1 is trying to revive another player while the co-op camera is inactive
			// and revive the other player if so.
			// Can revive with the 'Activate' input event from either keyboard or controller.
			// Return true if the event should be processed by the MenuControls hook.
			static bool CheckForP1ReviveReq(RE::InputEvent* const* a_eventHead);
			// Filter out and discard P1 input events that should be ignored while in co-op,
			// and allow other player's emulated P1 input events to pass through if they
			// are in control of menus.
			// IMPORTANT: 
			// InputEvent's 'pad24' member is used to store processing info:
			// 0xC0DAXXXX: event was already filtered and handled here.
			// 0xXXXXC0DA: proxied P1 input allowed through by this function.
			// 0xXXXXCA11: emulated P1 input sent by another player from the MIM.
			// 0xXXXXDEAD: ignore this input event.
			// 
			// Return true if the event should be processed by the MenuControls hook.
			static bool FilterInputEvents
			(
				RE::InputEvent* const* a_eventHead,
				RE::InputEvent* a_firstGamepadEvent
			);
			// Return a pointer to the first gamepad device input event in the given chain.
			static RE::InputEvent* GetFirstGamepadInputEvent(RE::InputEvent* const* a_eventHead);

			// Was an attempt made to open the co-op debug menu?
			static inline bool debugMenuTriggered;
			// Should the Pause/Wait menu-triggering input event be ignored 
			// while another menu is opened?
			static inline bool ignoringPauseWaitEvent;
			// 'Pause'/'Jornal' bind was pressed before 'Wait' bind 
			// when attempting to open the co-op summoning menu.
			static inline bool pauseBindPressedFirst;
			// Was an attempt made to open the co-op summoning menu?
			static inline bool summoningMenuTriggered;
		};

		// [NiNode Hooks]
		class NiNodeHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_NiNode[0] };
				_UpdateDownwardPass = vtbl.write_vfunc(0x2C, UpdateDownwardPass);
				SPDLOG_INFO("[NiNode Hook] Installed UpdateDownwardPass() hook.");
			}

		private:
			static void UpdateDownwardPass
			(
				RE::NiNode* a_this, RE::NiUpdateData& a_data, std::uint32_t a_arg2
			);
			static inline REL::Relocation<decltype(UpdateDownwardPass)> _UpdateDownwardPass;
		};

		// [PlayerCameraTransitionState Hooks]
		class PlayerCameraTransitionStateHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_PlayerCameraTransitionState[0] };
				_Begin = vtbl.write_vfunc(0x01, Begin);
				SPDLOG_INFO("[PlayerCameraTransitionState Hooks] Installed Begin() hook.");
			}

		private:
			static void Begin(RE::PlayerCameraTransitionState* a_this);
			static inline REL::Relocation<decltype(Begin)> _Begin;
		};

		// [PlayerCharacter Hooks]
		class PlayerCharacterHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
				REL::Relocation<uintptr_t> vtbl3{ RE::VTABLE_PlayerCharacter[3] };
				_DrawWeaponMagicHands = vtbl.write_vfunc(0xA6, DrawWeaponMagicHands);
				SPDLOG_INFO("[PlayerCharacter Hook] Installed DrawWeaponMagicHands() hook.");
				_CheckClampDamageModifier = vtbl.write_vfunc(0x127, CheckClampDamageModifier);
				SPDLOG_INFO("[PlayerCharacter Hook] Installed CheckClampDamageModifier() hook.");
				_HandleHealthDamage = vtbl.write_vfunc(0x104, HandleHealthDamage);
				SPDLOG_INFO("[PlayerCharacter Hook] Installed HandleHealthDamage() hook.");
				_ModifyAnimationUpdateData = vtbl.write_vfunc(0x79, ModifyAnimationUpdateData);
				SPDLOG_INFO("[PlayerCharacter Hook] Installed ModifyAnimationUpdateData() hook.");
				_NotifyAnimationGraph = vtbl3.write_vfunc(0x01, NotifyAnimationGraph);
				SPDLOG_INFO("[PlayerCharacter Hook] Installed NotifyAnimationGraph() hook.");
				_Update = vtbl.write_vfunc(0xAD, Update);
				SPDLOG_INFO("[PlayerCharacter Hook] Installed Update() hook.");
				_UseSkill = vtbl.write_vfunc(0xF7, UseSkill);
				SPDLOG_INFO("[PlayerCharacter Hook] Installed UseSkill() hook.");
			}

		private:
			static float CheckClampDamageModifier
			(
				RE::PlayerCharacter* a_this, RE::ActorValue a_av, float a_delta
			);
			static void DrawWeaponMagicHands(RE::PlayerCharacter* a_this, bool a_draw);
			static void HandleHealthDamage
			(
				RE::PlayerCharacter* a_this, RE::Actor* a_attacker, float a_damage
			);
			static void ModifyAnimationUpdateData
			(
				RE::PlayerCharacter* a_this, RE::BSAnimationUpdateData& a_data
			);
			static bool NotifyAnimationGraph
			(
				RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName
			);
			static void Update(RE::PlayerCharacter* a_this, float a_delta);													
			static void UseSkill
			(
				RE::PlayerCharacter* a_this, 
				RE::ActorValue a_av, 
				float a_points, 
				RE::TESForm* a_arg3
			);

			static inline REL::Relocation<decltype(CheckClampDamageModifier)> 
			_CheckClampDamageModifier;
			static inline REL::Relocation<decltype(DrawWeaponMagicHands)> _DrawWeaponMagicHands;
			static inline REL::Relocation<decltype(HandleHealthDamage)> _HandleHealthDamage;
			static inline REL::Relocation<decltype(ModifyAnimationUpdateData)> 
			_ModifyAnimationUpdateData;
			static inline REL::Relocation<decltype(NotifyAnimationGraph)> _NotifyAnimationGraph;
			static inline REL::Relocation<decltype(Update)> _Update;
			static inline REL::Relocation<decltype(UseSkill)> _UseSkill;
		};

		// [Projectile Hooks]
		class ProjectileHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> projectileVtbl{ RE::VTABLE_Projectile[0] };
				REL::Relocation<uintptr_t> arrowProjectileVtbl{ RE::VTABLE_ArrowProjectile[0] };
				REL::Relocation<uintptr_t> barrierProjectileVtbl
				{
					RE::VTABLE_BarrierProjectile[0] 
				};
				REL::Relocation<uintptr_t> beamProjectileVtbl{ RE::VTABLE_BeamProjectile[0] };
				REL::Relocation<uintptr_t> coneProjectileVtbl{ RE::VTABLE_ConeProjectile[0] };
				REL::Relocation<uintptr_t> flameProjectileVtbl{ RE::VTABLE_FlameProjectile[0] };
				REL::Relocation<uintptr_t> grenadeProjectileVtbl
				{
					RE::VTABLE_GrenadeProjectile[0] 
				};
				REL::Relocation<uintptr_t> missileProjectileVtbl
				{
					RE::VTABLE_MissileProjectile[0] 
				};

				// Arrow.
				_ArrowProjectile_GetLinearVelocity = arrowProjectileVtbl.write_vfunc
				(
					0x86, GetLinearVelocity
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] " 
					"Installed ArrowProjectile GetLinearVelocity() hook."
				);
				_ArrowProjectile_OnArrowCollision = arrowProjectileVtbl.write_vfunc
				(
					0xBE, OnProjectileCollision
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] Installed ArrowProjectile OnArrowCollision() hook."
				);
				_ArrowProjectile_UpdateImpl = arrowProjectileVtbl.write_vfunc(0xAB, UpdateImpl);
				SPDLOG_INFO("[Projectile Hook] Installed ArrowProjectile UpdateImpl() hook.");

				// Barrier.
				_BarrierProjectile_GetLinearVelocity = barrierProjectileVtbl.write_vfunc
				(
					0x86, GetLinearVelocity
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] " 
					"Installed BarrierProjectile GetLinearVelocity() hook."
				);
				_BarrierProjectile_ShouldUseDesiredTarget = barrierProjectileVtbl.write_vfunc
				(
					0xC1, ShouldUseDesiredTarget
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] Installed BarrierProjectile ShouldUseDesiredTarget() hook."
				);
				_BarrierProjectile_UpdateImpl = barrierProjectileVtbl.write_vfunc
				(
					0xAB, UpdateImpl
				);
				SPDLOG_INFO("[Projectile Hook] Installed BarrierProjectile UpdateImpl() hook.");

				// Beam.
				_BeamProjectile_GetLinearVelocity = beamProjectileVtbl.write_vfunc
				(
					0x86, GetLinearVelocity
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] "
					"Installed BeamProjectile GetLinearVelocity() hook."
				);
				_BeamProjectile_ShouldUseDesiredTarget = beamProjectileVtbl.write_vfunc
				(
					0xC1, ShouldUseDesiredTarget
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] Installed BeamProjectile ShouldUseDesiredTarget() hook."
				);
				_BeamProjectile_UpdateImpl = beamProjectileVtbl.write_vfunc(0xAB, UpdateImpl);
				SPDLOG_INFO("[Projectile Hook] Installed BeamProjectile UpdateImpl() hook.");

				// Cone.
				_ConeProjectile_GetLinearVelocity = coneProjectileVtbl.write_vfunc
				(
					0x86, GetLinearVelocity
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] " 
					"Installed ConeProjectile GetLinearVelocity() hook."
				);
				_ConeProjectile_OnConeCollision = coneProjectileVtbl.write_vfunc
				(
					0xBE, OnProjectileCollision
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] Installed ConeProjectile OnConeCollision() hook."
				);
				_ConeProjectile_UpdateImpl = coneProjectileVtbl.write_vfunc(0xAB, UpdateImpl);
				SPDLOG_INFO("[Projectile Hook] Installed ConeProjectile UpdateImpl() hook.");

				// Flame.
				_FlameProjectile_GetLinearVelocity = flameProjectileVtbl.write_vfunc
				(
					0x86, GetLinearVelocity
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] " 
					"Installed FlameProjectile GetLinearVelocity() hook."
				);
				_FlameProjectile_ShouldUseDesiredTarget = flameProjectileVtbl.write_vfunc
				(
					0xC1, ShouldUseDesiredTarget
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] Installed FlameProjectile ShouldUseDesiredTarget() hook."
				);
				_FlameProjectile_UpdateImpl = flameProjectileVtbl.write_vfunc(0xAB, UpdateImpl);
				SPDLOG_INFO("[Projectile Hook] Installed FlameProjectile UpdateImpl() hook.");

				// Grenade.
				_GrenadeProjectile_GetLinearVelocity = grenadeProjectileVtbl.write_vfunc
				(
					0x86, GetLinearVelocity
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] " 
					"Installed GrenadeProjectile GetLinearVelocity() hook."
				);
				_GrenadeProjectile_ShouldUseDesiredTarget = grenadeProjectileVtbl.write_vfunc
				(
					0xC1, ShouldUseDesiredTarget
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] Installed GrenadeProjectile ShouldUseDesiredTarget() hook."
				);
				_GrenadeProjectile_UpdateImpl = grenadeProjectileVtbl.write_vfunc
				(
					0xAB, UpdateImpl
				);
				SPDLOG_INFO("[Projectile Hook] Installed GrenadeProjectile UpdateImpl() hook.");

				// Missile.
				_MissileProjectile_GetLinearVelocity = missileProjectileVtbl.write_vfunc
				(
					0x86, GetLinearVelocity
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] " 
					"Installed MissileProjectile GetLinearVelocity() hook."
				);
				_MissileProjectile_OnMissileCollision = missileProjectileVtbl.write_vfunc
				(
					0xBE, OnProjectileCollision
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] Installed MissileProjectile OnMissileCollision() hook."
				);
				_MissileProjectile_UpdateImpl = missileProjectileVtbl.write_vfunc
				(
					0xAB, UpdateImpl
				);
				SPDLOG_INFO("[Projectile Hook] Installed MissileProjectile UpdateImpl() hook.");

				// Generic.
				_Projectile_GetLinearVelocity = projectileVtbl.write_vfunc
				(
					0x86, GetLinearVelocity
				);
				SPDLOG_INFO("[Projectile Hook] Installed Projectile GetLinearVelocity() hook.");
				_Projectile_OnProjectileCollision = projectileVtbl.write_vfunc
				(
					0xBE, OnProjectileCollision
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] Installed Projectile OnMissileCollision() hook."
				);
				_Projectile_ShouldUseDesiredTarget = projectileVtbl.write_vfunc
				(
					0xC1, ShouldUseDesiredTarget
				);
				SPDLOG_INFO
				(
					"[Projectile Hook] Installed Projectile ShouldUseDesiredTarget() hook."
				);
				_Projectile_UpdateImpl = projectileVtbl.write_vfunc(0xAB, UpdateImpl);
				SPDLOG_INFO("[Projectile Hook] Installed Projectile UpdateImpl() hook.");
				
				auto& trampoline = SKSE::GetTrampoline();
				REL::Relocation<uintptr_t> hook{ RELOCATION_ID(43013, 44204) };
				_Projectile_ProcessHit = trampoline.write_call<5>
				(
					hook.address() + OFFSET(0x251, 0x21F), ProcessHit
				);
				SPDLOG_INFO("[MainHook] Installed Projectile ProcessHit() hook");
			}

		private:
			// TODO: Template functions to reduce copied code.
			static void GetLinearVelocity(RE::Projectile* a_this, RE::NiPoint3& a_velocity);
			// Credits to dTry for both OnCollision hooks:
			// https://github.com/D7ry/valhallaCombat/blob/Master/src/include/Hooks.h#L181
			static void OnProjectileCollision
			(
				RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector
			);
			// All credits to digital-apple for the hook location:
			// https://github.com/digital-apple/ExplosionCollisionFix/blob/main/source/Hooks.cpp#L7
			static inline bool ProcessHit
			(
				RE::Projectile* a_this,
				RE::TESObjectREFR* a_hitRefr,
				RE::NiPoint3* a_location,
				RE::hkVector4* a_unknown,
				RE::COL_LAYER a_collisionLayer,
				RE::MATERIAL_ID a_materialID,
				bool* a_handled
			);
			static bool ShouldUseDesiredTarget(RE::Projectile* a_this); 
			static void UpdateImpl(RE::Projectile* a_this, float a_delta);

			// Arrow
			static inline REL::Relocation<decltype(GetLinearVelocity)> 
			_ArrowProjectile_GetLinearVelocity;
			static inline REL::Relocation<decltype(OnProjectileCollision)> 
			_ArrowProjectile_OnArrowCollision;
			static inline REL::Relocation<decltype(UpdateImpl)> _ArrowProjectile_UpdateImpl;
			
			// Barrier
			static inline REL::Relocation<decltype(GetLinearVelocity)> 
			_BarrierProjectile_GetLinearVelocity;
			static inline REL::Relocation<decltype(ShouldUseDesiredTarget)> 
			_BarrierProjectile_ShouldUseDesiredTarget;
			static inline REL::Relocation<decltype(UpdateImpl)> _BarrierProjectile_UpdateImpl;

			// Beam
			static inline REL::Relocation<decltype(GetLinearVelocity)> 
			_BeamProjectile_GetLinearVelocity;
			static inline REL::Relocation<decltype(ShouldUseDesiredTarget)> 
			_BeamProjectile_ShouldUseDesiredTarget;
			static inline REL::Relocation<decltype(UpdateImpl)> _BeamProjectile_UpdateImpl;

			// Cone
			static inline REL::Relocation<decltype(GetLinearVelocity)> 
			_ConeProjectile_GetLinearVelocity;
			static inline REL::Relocation<decltype(OnProjectileCollision)> 
			_ConeProjectile_OnConeCollision;
			static inline REL::Relocation<decltype(UpdateImpl)> _ConeProjectile_UpdateImpl;
			
			// Flame
			static inline REL::Relocation<decltype(GetLinearVelocity)> 
			_FlameProjectile_GetLinearVelocity;
			static inline REL::Relocation<decltype(ShouldUseDesiredTarget)> 
			_FlameProjectile_ShouldUseDesiredTarget;
			static inline REL::Relocation<decltype(UpdateImpl)> _FlameProjectile_UpdateImpl;

			// Grenade
			static inline REL::Relocation<decltype(GetLinearVelocity)> 
			_GrenadeProjectile_GetLinearVelocity;
			static inline REL::Relocation<decltype(ShouldUseDesiredTarget)>
			_GrenadeProjectile_ShouldUseDesiredTarget;
			static inline REL::Relocation<decltype(UpdateImpl)> _GrenadeProjectile_UpdateImpl;

			// Missile
			static inline REL::Relocation<decltype(GetLinearVelocity)> 
			_MissileProjectile_GetLinearVelocity;
			static inline REL::Relocation<decltype(OnProjectileCollision)> 
			_MissileProjectile_OnMissileCollision;
			static inline REL::Relocation<decltype(UpdateImpl)> _MissileProjectile_UpdateImpl;
			
			// Generic
			static inline REL::Relocation<decltype(GetLinearVelocity)> 
			_Projectile_GetLinearVelocity;
			static inline REL::Relocation<decltype(OnProjectileCollision)> 
			_Projectile_OnProjectileCollision;
			static inline REL::Relocation<decltype(ProcessHit)> _Projectile_ProcessHit;
			static inline REL::Relocation<decltype(ShouldUseDesiredTarget)> 
			_Projectile_ShouldUseDesiredTarget;
			static inline REL::Relocation<decltype(UpdateImpl)> _Projectile_UpdateImpl;

			// Adjust projectile trajectory towards the computed intercept position 
			// or the player's current target.
			// Return true if the projectile was directed at the target position.
			static bool DirectProjectileAtTarget
			(
				const std::shared_ptr<CoopPlayer>& a_p, 
				const RE::ObjectRefHandle& a_projectileHandle,
				RE::NiPoint3& a_resultingVelocityOut, 
				const bool& a_justReleased
			);
			// Store the firing player's CID in one outparam (-1 if not by a player), 
			// and true in the other outparam if the projectile was fired at a player.
			static void GetFiredAtOrByPlayer
			(
				const RE::ObjectRefHandle& a_projectileHandle, 
				int32_t& a_firingPlayerCIDOut,
				bool& a_firedAtPlayerOut
			);
			// Store the player CID for the player grabbing/releasing the given projectile 
			// in the outparams (-1 if not by a player).
			static void GetManipulatingPlayer
			(
				const RE::ObjectRefHandle& a_projHandle,
				int32_t& a_grabbedByPlayerCID, 
				int32_t& a_releasedByPlayerCID
			);
			// Position a grabbed hostile projectile or guide a released projectile
			// along the trajectory set by the grabbing/releasing player's 
			// reference manipulation manager.
			// Update the velocity through the outparam.
			// Return true if the projectile was manipulated.
			static bool HandleManipulatedProjectile
			(
				const std::shared_ptr<CoopPlayer>& a_p,  
				const RE::ObjectRefHandle& a_projectileHandle, 
				bool a_isGrabbed,
				RE::NiPoint3& a_resultingVelocityOut
			);
			// Adjust the projectile's trajectory to home in at the player's current target.
			// Update the velocity through the outparam.
			static void SetHomingTrajectory
			(
				const std::shared_ptr<CoopPlayer>& a_p, 
				const RE::ObjectRefHandle& a_projectileHandle,
				RE::NiPoint3& a_resultingVelocityOut
			);
			// Guide the projectile along a pre-determined trajectory 
			// towards the computed target intercept position.
			// Update the velocity through the outparam.
			static void SetFixedTrajectory
			(
				const std::shared_ptr<CoopPlayer>& a_p,  
				const RE::ObjectRefHandle& a_projectileHandle, 
				RE::NiPoint3& a_resultingVelocityOut
			);
			// Direct flame and beam projectiles in a straight line 
			// directly at the target position.
			static void SetStraightTrajectory
			(
				const std::shared_ptr<CoopPlayer>& a_p,
				const RE::ObjectRefHandle& a_projectileHandle,
				RE::NiPoint3& a_resultingVelocityOut
			);
		};

		// [SpellItem Hooks]
		// NOTE:
		// Unused for now, since this hook only seems to fire in vanilla Enderal,
		// not in vanilla Skyrim.
		class SpellItemHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_SpellItem[0] };
				_AdjustCost = vtbl.write_vfunc(0x63, AdjustCost);
				SPDLOG_INFO("[SpellItem Hooks] Installed AdjustCost() hook.");
			}

		private:
			static void AdjustCost(RE::SpellItem* a_this, float& a_cost, RE::Actor* a_actor);
			static inline REL::Relocation<decltype(AdjustCost)> _AdjustCost;
		};

		// [TESCamera Hooks]
		// Credits to ersh1 and doodlum:
		// https://github.com/ersh1/Precision/blob/main/src/Hooks.h#L163
		// https://github.com/doodlum/skyrim-camera-noise/blob/main/src/CameraNoiseManager.h#L65
		class TESCameraHooks
		{
		public:
			static void InstallHooks()
			{
				// 84AB90, 876700
				REL::Relocation<std::uintptr_t> hook1{ RELOCATION_ID(49852, 50784) };  
				auto& trampoline = SKSE::GetTrampoline();
				_Update = trampoline.write_call<5>(hook1.address() + OFFSET(0x1A6, 0x1A6), Update);
				SPDLOG_INFO("[TESCamera Hooks] Installed Update() hook.");
			}

		private:
			static void Update(RE::TESCamera* a_this);
			static inline REL::Relocation<decltype(Update)> _Update;
		};

		// [TESObjectBOOK Hooks]
		class TESObjectBOOKHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_TESObjectBOOK[0] };
				_Activate = vtbl.write_vfunc(0x37, Activate);
				SPDLOG_INFO("[TESObjectBOOK Hooks] Installed Activate() hook.");
			}

		private:
			static bool Activate
			(
				RE::TESObjectBOOK* a_this, 
				RE::TESObjectREFR* a_targetRef,
				RE::TESObjectREFR* a_activatorRef,
				std::uint8_t a_arg3, 
				RE::TESBoundObject* a_object, 
				std::int32_t a_targetCount
			);
			static inline REL::Relocation<decltype(Activate)> _Activate;
		};

		// [TESObjectREFR Hooks]
		class TESObjectREFRHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_TESObjectREFR[0] };
				_SetParentCell = vtbl.write_vfunc(0x98, SetParentCell);
				SPDLOG_INFO("[TESObjectREFR Hooks] Installed SetParentCell() hook.");
			}

		private:
			static void SetParentCell(RE::TESObjectREFR* a_this, RE::TESObjectCELL* a_cell);
			static inline REL::Relocation<decltype(SetParentCell)> _SetParentCell;
		};

		// [Third Person States Camera Hooks]
		class ThirdPersonCameraStatesHooks
		{
			// TODO: Template this eventually.
		public:
			static void InstallHooks()
			{
				// TPCS: Third Person Camera State
				// BCS: Bleedout Camera State
				// HCS: Horse Camera State
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_ThirdPersonState[0] };
				_GetRotationTPCS = vtbl.write_vfunc(0x04, GetRotation);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] "
					"Installed ThirdPersonState::GetRotation() hook."
				);
				_HandleLookInputTPCS = vtbl.write_vfunc(0x0F, HandleLookInput);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed ThirdPersonState::HandleLookInput() hook."
				);
				_SetFreeRotationModeTPCS = vtbl.write_vfunc(0x0D, SetFreeRotationMode);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed ThirdPersonState::SetFreeRotationMode() hook."
				);
				_UpdateRotationTPCS = vtbl.write_vfunc(0x0E, UpdateRotation);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed ThirdPersonState::UpdateRotation() hook."
				);

				REL::Relocation<uintptr_t> vtbl1{ RE::VTABLE_BleedoutCameraState[0] };
				_BeginBCS = vtbl1.write_vfunc(0x01, Begin);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed BleedOutCameraState::Begin() hook."
				);
				_GetRotationBCS = vtbl1.write_vfunc(0x04, GetRotation);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed BleedOutCameraState::GetRotation() hook."
				);
				_HandleLookInputBCS = vtbl1.write_vfunc(0x0F, HandleLookInput);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed BleedOutCameraState::HandleLookInput() hook."
				);
				_SetFreeRotationModeBCS = vtbl1.write_vfunc(0x0D, SetFreeRotationMode);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed BleedOutCameraState::SetFreeRotationMode() hook."
				);
				_UpdateRotationBCS = vtbl1.write_vfunc(0x0E, UpdateRotation);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed BleedOutCameraState::UpdateRotation() hook."
				);

				REL::Relocation<uintptr_t> vtbl2{ RE::VTABLE_HorseCameraState[0] };
				_BeginHCS = vtbl2.write_vfunc(0x01, Begin);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] "
					"Installed HorseCameraState::Begin() hook."
				);
				_GetRotationHCS = vtbl2.write_vfunc(0x04, GetRotation);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] "
					"Installed HorseCameraState::GetRotation() hook."
				);
				_HandleLookInputHCS = vtbl2.write_vfunc(0x0F, HandleLookInput);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed HorseCameraState::HandleLookInput() hook."
				);
				_SetFreeRotationModeHCS = vtbl2.write_vfunc(0x0D, SetFreeRotationMode);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed HorseCameraState::SetFreeRotationMode() hook."
				);
				_UpdateRotationHCS = vtbl2.write_vfunc(0x0E, UpdateRotation);
				SPDLOG_INFO
				(
					"[ThirdPersonCameraStates Hooks] " 
					"Installed HorseCameraState::UpdateRotation() hook."
				);
			}

		private:
			static void Begin(RE::ThirdPersonState* a_this);									
			static void GetRotation(RE::ThirdPersonState* a_this, RE::NiQuaternion& a_rotation);
			static void HandleLookInput(RE::ThirdPersonState* a_this, const RE::NiPoint2& a_input);
			static void SetFreeRotationMode(RE::ThirdPersonState* a_this, bool a_weaponSheathed);
			static void UpdateRotation(RE::ThirdPersonState* a_this);							


			static inline REL::Relocation<decltype(Begin)> _BeginTPCS;
			static inline REL::Relocation<decltype(Begin)> _BeginBCS;
			static inline REL::Relocation<decltype(Begin)> _BeginHCS;
			static inline REL::Relocation<decltype(GetRotation)> _GetRotationTPCS;
			static inline REL::Relocation<decltype(GetRotation)> _GetRotationBCS;
			static inline REL::Relocation<decltype(GetRotation)> _GetRotationHCS;
			static inline REL::Relocation<decltype(HandleLookInput)> _HandleLookInputTPCS;
			static inline REL::Relocation<decltype(HandleLookInput)> _HandleLookInputBCS;
			static inline REL::Relocation<decltype(HandleLookInput)> _HandleLookInputHCS;
			static inline REL::Relocation<decltype(SetFreeRotationMode)> _SetFreeRotationModeTPCS;
			static inline REL::Relocation<decltype(SetFreeRotationMode)> _SetFreeRotationModeBCS;
			static inline REL::Relocation<decltype(SetFreeRotationMode)> _SetFreeRotationModeHCS;
			static inline REL::Relocation<decltype(UpdateRotation)> _UpdateRotationTPCS;
			static inline REL::Relocation<decltype(UpdateRotation)> _UpdateRotationBCS;
			static inline REL::Relocation<decltype(UpdateRotation)> _UpdateRotationHCS;
		};
		
		// [ValueModifierEffect Hooks]
		class ValueModifierEffectHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_ValueModifierEffect[0] };
				_Start = vtbl.write_vfunc(0x14, Start);
				SPDLOG_INFO("[ValueModifierEffect Hooks] Installed Start() hook.");
			}

		private:
			static void Start(RE::ValueModifierEffect* a_this);
			static inline REL::Relocation<decltype(Start)> _Start;
		};

		// [VampireLordEffect Hooks]
		class VampireLordEffectHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_VampireLordEffect[0] };
				_Start = vtbl.write_vfunc(0x14, Start);
				SPDLOG_INFO("[VampireLordEffect Hooks] Installed Start() hook.");
				_Finish = vtbl.write_vfunc(0x15, Finish);
				SPDLOG_INFO("[VampireLordEffect Hooks] Installed Finish() hook.");
			}

		private:
			static void Start(RE::VampireLordEffect* a_this);
			static void Finish(RE::VampireLordEffect* a_this);
			static inline REL::Relocation<decltype(Start)> _Start;
			static inline REL::Relocation<decltype(Finish)> _Finish;
		};

		// [WerewolfEffect Hooks]
		class WerewolfEffectHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_WerewolfEffect[0] };
				_Start = vtbl.write_vfunc(0x14, Start);
				SPDLOG_INFO("[WerewolfEffect Hooks] Installed Start() hook.");
				_Finish = vtbl.write_vfunc(0x15, Finish);
				SPDLOG_INFO("[WerewolfEffect Hooks] Installed Finish() hook.");
			}

		private:
			static void Start(RE::WerewolfEffect* a_this);
			static void Finish(RE::WerewolfEffect* a_this);
			static inline REL::Relocation<decltype(Start)> _Start;
			static inline REL::Relocation<decltype(Finish)> _Finish;
		};

		//=================================
		// [Menu Message Processing Hooks]:
		//=================================

		// [Barter Menu Hooks]
		class BarterMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_BarterMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[BarterMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::BarterMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Book Menu Hooks]
		class BookMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_BookMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[BookMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::BookMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Container Menu Hooks]
		class ContainerMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_ContainerMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[ContainerMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::ContainerMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Crafting Menu Hooks]
		class CraftingMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_CraftingMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[CraftingMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::CraftingMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Dialogue Menu Hooks]
		class DialogueMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_DialogueMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[DialogueMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::DialogueMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Favorites Menu Hooks]
		class FavoritesMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_FavoritesMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[FavoritesMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:                                     
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::FavoritesMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Inventory Menu Hooks]
		class InventoryMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_InventoryMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[InventoryMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::InventoryMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Loading Menu Hooks]
		class LoadingMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_LoadingMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[LoadingMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::LoadingMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Magic Menu Hooks]
		class MagicMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_MagicMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[MagicMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::MagicMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Stats Menu Hooks]
		class StatsMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_StatsMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[StatsMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::StatsMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		// [Training Menu Hooks]
		class TrainingMenuHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_TrainingMenu[0] };
				_ProcessMessage = vtbl.write_vfunc(0x04, ProcessMessage);
				SPDLOG_INFO("[TrainingMenu Hooks] Installed ProcessMessage() hook.");
			}

		private:
			static RE::UI_MESSAGE_RESULTS ProcessMessage
			(
				RE::TrainingMenu* a_this, RE::UIMessage& a_message
			);
			static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
		};

		//====================
		// [P1 Handler Hooks]:
		//====================
		
		// [ActivateHandler Hooks]
		class ActivateHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_ActivateHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[ActivateHandler Hook] Installed CanProcess() hook.");
			}

		private:
			static bool CanProcess
			(
				RE::ActivateHandler* a_this, RE::InputEvent* a_event
			);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};

		// [AttackBlockHandler Hooks]
		class AttackBlockHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_AttackBlockHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[AttackBlockHandler Hook] Installed CanProcess() hook.");
			}

		private:
			static bool CanProcess
			(
				RE::AttackBlockHandler* a_this, RE::InputEvent* a_event
			);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};

		// [JumpHandler Hooks]
		class JumpHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_JumpHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[JumpHandler Hook] Installed CanProcess() hook.");
			}
		private:
			static bool CanProcess(RE::JumpHandler* a_this, RE::InputEvent* a_event);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};

		// [LookHandler Hooks]
		class LookHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_LookHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[LookHandler Hook] Installed CanProcess() hook.");
			}

		private:
			static bool CanProcess(RE::LookHandler* a_this, RE::InputEvent* a_event);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};

		// [MovementHandler Hooks]
		class MovementHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_MovementHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[MovementHandler Hook] Installed CanProcess() hook.");
			}

		private:
			static bool CanProcess(RE::MovementHandler* a_this, RE::InputEvent* a_event);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};

		// [ReadyWeaponHandler Hooks]
		class ReadyWeaponHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_ReadyWeaponHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[ReadyWeaponHandler Hook] Installed CanProcess() hook.");
			}

		private:
			static bool CanProcess(RE::ReadyWeaponHandler* a_this, RE::InputEvent* a_event);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};

		// [ShoutHandler Hooks]
		class ShoutHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_ShoutHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[ShoutHandler Hook] Installed CanProcess() hook.");
			}

		private:
			static bool CanProcess(RE::ShoutHandler* a_this, RE::InputEvent* a_event);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};

		// [SneakHandler Hooks]
		class SneakHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_SneakHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[SneakHandler Hook] Installed CanProcess() hook.");
			}

		private:
			static bool CanProcess(RE::SneakHandler* a_this, RE::InputEvent* a_event);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};

		// [SprintHandler Hooks]
		class SprintHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_SprintHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[SprintHandler Hook] Installed CanProcess() hook.");
			}

		private:
			static bool CanProcess(RE::SprintHandler* a_this, RE::InputEvent* a_event);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};

		// [TogglePOVHandler Hooks]
		class TogglePOVHandlerHooks
		{
		public:
			static void InstallHooks()
			{
				REL::Relocation<uintptr_t> vtbl{ RE::VTABLE_TogglePOVHandler[0] };
				_CanProcess = vtbl.write_vfunc(0x01, CanProcess);
				SPDLOG_INFO("[TogglePOVHandler Hook] Installed CanProcess() hook.");
			}

		private:
			static bool CanProcess(RE::TogglePOVHandler* a_this, RE::InputEvent* a_event);
			static inline REL::Relocation<decltype(CanProcess)> _CanProcess;
		};
	}
}


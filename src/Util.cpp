#pragma once
#include "Util.h"
#include <Compatibility.h>
#include <GlobalCoopData.h>
#include <Raycast.h>
#include <chrono>
#include <complex>
#include <valarray>

// Conversions
#define GAME_TO_HAVOK (0.0142875f)
#define HAVOK_TO_GAME (69.99125f)
#define PI (3.14159265358979323846f)
#define TO_DEGREES (180.0f / PI)
#define TO_RADIANS (PI / 180.0f)

namespace ALYSLC
{
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();
	
	//================
	//[Interpolation]:
	//================

	float TwoWayInterpData::UpdateInterpolatedValue(const bool& a_directionChangeFlag)
	{
		// Update the interpolated value after updating the cached
		// direction change flag with the given one.
		
		// A direction flag of true means interpolate to the maximum endpoint,
		// and a direction flag of false means interpolate to the minimum endpoint.
		// If the two are not equivalent, switch the direction of interpolation.
		if (directionChangeFlag != a_directionChangeFlag)
		{
			directionChangeTP = SteadyClock::now();
			valueAtDirectionChange = value;
			directionChangeFlag = a_directionChangeFlag;
		}
		else if ((value == minEndpoint && valueAtDirectionChange != minEndpoint) || 
				 (value == maxEndpoint && valueAtDirectionChange != maxEndpoint))
		{
			// Reached an interp endpoint, so save the value
			// for the next direction change.
			valueAtDirectionChange = value;
		}

		float secsSinceChange = Util::GetElapsedSeconds(directionChangeTP);
		interpToMax = directionChangeFlag && value != maxEndpoint;
		interpToMin = !directionChangeFlag && value != minEndpoint;
		// Continue interpolating to the currently targeted endpoint.
		if (interpToMin || interpToMax)
		{
			// The true interpolation interval scales with 
			// the remaining distance to the new endpoint on direction change.
			if (interpToMax)
			{
				float secsInterpInterval = std::clamp
				(
					secsInterpToMaxInterval * (maxEndpoint - valueAtDirectionChange),
					0.0f, 
					secsInterpToMaxInterval
				);
				if (secsInterpInterval == 0.0f)
				{
					value = maxEndpoint;
				}
				else
				{
					value = Util::InterpolateSmootherStep
					(
						valueAtDirectionChange, 
						maxEndpoint, 
						std::clamp(secsSinceChange / secsInterpInterval, 0.0f, 1.0f)
					);
				}
			}
			else if (interpToMin)
			{
				float secsInterpInterval = std::clamp
				(
					secsInterpToMinInterval * (valueAtDirectionChange - minEndpoint),
					0.0f,
					secsInterpToMinInterval
				);
				if (secsInterpInterval == 0.0f)
				{
					value = minEndpoint;
				}
				else
				{
					value = Util::InterpolateSmootherStep
					(
						valueAtDirectionChange,
						minEndpoint,
						std::clamp(secsSinceChange / secsInterpInterval, 0.0f, 1.0f)
					);
				}
			}
		}

		return value;
	}

	namespace Util
	{
		// All credits go to the Wikipedia contributors for this page:
		// https://en.wikipedia.org/wiki/Lambert_W_function
		namespace LambertWFunc
		{
			// Real value solutions for the W0 and W-1 branches.
			// If there is no real solution for a branch, nullopt is returned.
			using FuncRealSolnPair = std::pair<std::optional<double>, std::optional<double>>;
			FuncRealSolnPair ApproxRealSolutionBothBranches
			(
				const double& a_z, const double& a_precision
			)
			{
				// Returns approximate REAL values of both of the Lambert W function's branches 
				// at the given z value, approximated with the given number of steps.

				if (a_z <= -exp(-1) || a_z >= 0)
				{
					// Not a z value at which either branch is defined (< -1 / e),
					// or at the singularity (-1 / e), or the W-1 branch is undefined (>= 0).
					return FuncRealSolnPair({ std::nullopt, std::nullopt });
				}

				// Guaranteed z is now in (-1 / e, 0)
				const double ez = exp(1) * a_z;
				// Set starting values for each branch.
				double init0 = ((ez) / (1 + ez + sqrt(1 + ez))) * (log(1 + sqrt(1 + ez)));
				double initMin1{ -0.25 };
				if (a_z <= -0.25)
				{
					initMin1 = -1 - sqrt(2 * (1 + ez));
				}
				else
				{
					initMin1 = log(-a_z) - log(-log(-a_z));
				}

				// Run the approximation.
				return RunApprox(ApproxMethod::kNewton, init0, initMin1, a_z, a_precision);
			}

			// All approximation methods adapted from here:
			// https://en.wikipedia.org/wiki/Lambert_W_function#Numerical_evaluation
			double HalleyApprox(const double& a_wn, const double& a_z)
			{
				const double expWn = exp(a_wn);
				const double dExpWn = a_wn * exp(a_wn);
				return 
				{
					a_wn - 
					(
						(dExpWn - a_z) / 
						(expWn * (a_wn + 1) - ((a_wn + 2) * (dExpWn - a_z) / (2 * a_wn + 2)))
					)
				};
			}

			double IaconoBoydApprox(const double& a_wn, const double& a_z)
			{
				return 
				{
					(a_wn / (1 + a_wn)) * (1 + log(a_z / a_wn))
				};
			}

			double NewtonApprox(const double& a_wn, const double& a_z)
			{
				const double expWn = exp(a_wn);
				const double dExpWn = a_wn * exp(a_wn);
				return 
				{
					a_wn - ((dExpWn - a_z) / (expWn + dExpWn)) 
				};
			}

			FuncRealSolnPair RunApprox
			(
				ApproxMethod&& a_method,
				const double& a_init0, 
				const double& a_initMin1,
				const double& a_z, 
				const double& a_precision
			)
			{
				// Helper function to get real solutions for both branches 
				// of the Lambert W function using the given data.
				
				// Max steps to run for each branch.
				uint32_t maxStepsB0 = floor(log2(-log10(a_precision)) + 0.5);
				uint32_t maxStepsBMin1 = floor(log2(-log2(a_precision)) + 0.5);
				// Real value for the W0 branch.
				double wn0 = a_init0;
				// Previously computed value.
				double wPrev = wn0;
				// Difference between the prev and current computed values.
				double stepDiff = INFINITY;
				// Current step.
				uint32_t step = 0;
				// Iterate until max steps or target precision reached.
				while (step < maxStepsB0 && stepDiff > a_precision)
				{
					// Save prev.
					wPrev = wn0;
					// Get approximated value.
					switch (a_method)
					{
					case ApproxMethod::kNewton:
					{
						wn0 = NewtonApprox(wn0, a_z);
						break;
					}
					case ApproxMethod::kIaconoBoyd:
					{
						wn0 = IaconoBoydApprox(wn0, a_z);
						break;
					}
					case ApproxMethod::kHalley:
					{
						wn0 = HalleyApprox(wn0, a_z);
						break;
					}
					default:
						break;
					}

					// Set diff.
					stepDiff = fabsf(wn0 - wPrev);
					// On to the next step.
					++step;
				}

				// Real value for the W-1 branch.
				double wnMin1 = a_initMin1;
				wPrev = wnMin1;
				stepDiff = INFINITY;
				step = 0;
				while (step < maxStepsBMin1 && stepDiff > a_precision)
				{
					// Save prev.
					wPrev = wnMin1;
					// Get approximated value.
					switch (a_method)
					{
					case ApproxMethod::kHalley:
					{
						wnMin1 = HalleyApprox(wnMin1, a_z);
						break;
					}
					case ApproxMethod::kNewton:
					{
						wnMin1 = NewtonApprox(wnMin1, a_z);
						break;
					}
					case ApproxMethod::kIaconoBoyd:
					{
						wnMin1 = IaconoBoydApprox(wnMin1, a_z);
						break;
					}
					default:
						break;
					}

					// Set diff.
					stepDiff = fabsf(wnMin1 - wPrev);
					// On to the next step.
					++step;
				}

				return { wn0, wnMin1 };
			}
		}

		void AddAsCombatTarget(RE::Actor* a_sourceActor, RE::Actor* a_targetActor)
		{
			// Add the given target actor as a combat target for the source actor.
			// If called before a ranged/melee hit connects and damage is dealt,
			// this will allow the source actor to deal damage to the target actor,
			// which is not always the case in combat 
			// when attacking a member of a different combat group,
			// especially for companion players.
			// Example:
			// Shooting Heimskyr in the face, and then shooting Nazeem (neutral or otherwise)
			// as he approaches with his snarky comments, will do 0 damage to Nazeem,
			// (he's gotten too powerful),
			// since the player's combat group does not seem to always add in targets 
			// from another combat group before attacks connect.

			auto combatGroup = a_sourceActor->GetCombatGroup(); 
			if (!combatGroup)
			{
				return;
			}

			combatGroup->lock.LockForWrite();

			bool isATarget = false;
			for (const auto& combatTarget : combatGroup->targets)
			{
				// Already a target, so we can exit.
				if (combatTarget.targetHandle == a_targetActor->GetHandle())
				{
					isATarget = true;
					break;
				}
			}

			// Adding as a target to allow for the projectile/weapon collisions 
			// to deal their associated damage.
			if (!isATarget)
			{
				// Create a new target.
				std::unique_ptr<RE::CombatTarget> newTarget = std::make_unique<RE::CombatTarget>();
				newTarget->targetHandle = a_targetActor->GetHandle();
				combatGroup->targets.emplace_back(*newTarget.get());
			}

			combatGroup->lock.UnlockForWrite();
		}

		void AddSyncedTask(std::function<void()> a_func, bool a_isUITask)
		{
			// Run a task using one of the game's task threads.
			// Return only after the task finishes.
			// NOTE: 
			// Do not add a synced task from one of the game's own threads,
			// as this will lock up the thread and freeze the game.

			const auto taskInterface = SKSE::GetTaskInterface(); 
			if (!taskInterface)
			{
				return;
			}

			std::atomic_bool taskDone{ false };
			// Queue a task through a regular or UI task pool thread.
			if (a_isUITask)
			{
				taskInterface->AddUITask
				(
					[&]() 
					{
						a_func();
						taskDone.store(true);
						taskDone.notify_all();
					}
				);
				taskDone.wait(false);
			}
			else
			{
				taskInterface->AddTask
				(
					[&]()
					{
						a_func();
						taskDone.store(true);
						taskDone.notify_all();
					}
				);
				taskDone.wait(false);
			}
		}

		bool CanManipulateActor(RE::Actor* a_actor, RE::hkpRigidBody* a_rigidBody)
		{
			// Returns true if the given actor and rigid body
			// supports manipulation of its velocity.
			// Can use the supplied rigid body, 
			// or retrieve the rigid body from the actor's current 3D.

			if (!a_actor)
			{
				return false;
			}
			
			auto hkpRigidBodyPtr = 
			(
				!a_rigidBody ? 
				GethkpRigidBody(a_actor) : 
				RE::hkRefPtr<RE::hkpRigidBody>(a_rigidBody)
			);
			if (!hkpRigidBodyPtr)
			{
				return false;
			}

			// Ragdolling certain actors can cause all sorts of weird issues,
			// including stopping translational movement but looping the last played animation,
			// warping/stretching the ragdoll to reach a target position,
			// or becoming immune to all physical damage once released.
			// Have to figure out how to differentiate between actors 
			// that ragdoll and get up just fine even with either of the below race data flags, 
			// eg. Horses and Skeletons, 
			// and the problematic ragdolling actors, 
			// eg. Dragons, Atronachs, etc.
			// So to avoid this headache until I find a fix or workaround, 
			// we'll just prevent manipulation of actors with the 'NoKnockdowns' 
			// or 'AllowRagdollCollisions' race flags.
			return 
			(
				(
					hkpRigidBodyPtr->motion.type != RE::hkpMotion::MotionType::kFixed &&
					hkpRigidBodyPtr->motion.type != RE::hkpMotion::MotionType::kInvalid
				) &&
				(
					(a_actor->IsDead()) || 
					(
						a_actor->race &&
						a_actor->race->data.flags.none
						(
							RE::RACE_DATA::Flag::kNoKnockdowns,
							RE::RACE_DATA::Flag::kAllowRagdollCollision
						)
					)
				)
			);	
		}

		bool CanStopCombatWithActor(RE::Actor * a_aggroedActor)
		{
			// Can stop combat (surrender or just stop) between a player and the given actor.
			if (!a_aggroedActor)
			{
				return false;
			}

			// Pacify mounts, guards (surrender), actors with no bounty on the player,
			// fleeing actors, or friendly actors.
			return
			(
				a_aggroedActor->IsAMount() ||
				IsGuard(a_aggroedActor) || 
				HasNoBountyButInCrimeFaction(a_aggroedActor) ||
				IsFleeing(a_aggroedActor) ||
				IsPartyFriendlyActor(a_aggroedActor)
			);
		}

		void ChangeEssentialStatus(RE::Actor* a_actor, bool a_shouldSet, bool a_adjustBleedout)
		{
			// Set or unset the essential flags for the given actor.
			// Can also adjust bleedout override.

			if (!a_actor)
			{
				return;
			}

			auto actorBase = a_actor->GetActorBase();
			if (actorBase)
			{
				SetActorBaseDataFlag
				(
					actorBase, RE::ACTOR_BASE_DATA::Flag::kEssential, a_shouldSet
				);
				if (a_adjustBleedout)
				{
					SetActorBaseDataFlag
					(
						actorBase, RE::ACTOR_BASE_DATA::Flag::kBleedoutOverride, a_shouldSet
					);
					actorBase->actorData.bleedoutOverride = a_shouldSet ? -INT16_MAX : 0.0f;
				}
			}

			if (a_shouldSet)
			{
				a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kEssential);
			}
			else
			{
				a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
			}

			if (auto p1 = RE::PlayerCharacter::GetSingleton(); a_actor == p1)
			{
				glob.player1RefAlias->SetEssential(a_shouldSet);
			}
		}

		void ChangeFormFavoritesStatus
		(
			RE::Actor* a_actor, RE::TESForm* a_form, const bool& a_shouldFavorite
		)
		{
			// Change the form in the actor's inventory to favorited/unfavorited.

			if (!a_actor || !a_form)
			{
				return;
			}

			if (a_form->Is(RE::FormType::Spell, RE::FormType::Shout)) 
			{
				auto magicFavorites = RE::MagicFavorites::GetSingleton();
				if (!magicFavorites)
				{
					return;
				}

				if (a_shouldFavorite) 
				{
					magicFavorites->SetFavorite(a_form);
				}
				else
				{
					magicFavorites->RemoveFavorite(a_form);
				}
			}
			else
			{
				auto inventoryChanges = a_actor->GetInventoryChanges();
				if (!inventoryChanges)
				{
					return;
				}

				// Look for the form in the actor's inventory.
				auto inventory = a_actor->GetInventory();
				for (auto& [boundObj, entryDataPair] : inventory)
				{
					const auto& [count, ied] = entryDataPair;
					if (!boundObj || boundObj != a_form || count <= 0 || !ied)
					{
						continue;
					}

					// Exists and has a non-zero count.
					if (ied->extraLists)
					{
						for (auto exDataList : *ied->extraLists)
						{
							if (!exDataList || !exDataList->HasType(RE::ExtraDataType::kHotkey))
							{
								continue;
							}

							// Unfavorite only if extra hotkey data is present.
							if (!a_shouldFavorite)
							{
								NativeFunctions::Unfavorite
								(
									inventoryChanges, ied.get(), exDataList
								);
							}

							// Return once extra hotkey data found,
							// since this is the condition indicating
							// that the item is already favorited or is now unfavorited.
							return;
						}

						// Favorite only if not previously favorited.
						if (a_shouldFavorite)
						{
							NativeFunctions::Favorite
							(
								inventoryChanges,
								ied.get(),
								!ied->extraLists->empty() ?
								ied->extraLists->front() :
								nullptr
							);
						}
					}
					else if (a_shouldFavorite)
					{
						// Favorite the form right away because
						// there is no extra data at all for this item.
						NativeFunctions::Favorite(inventoryChanges, ied.get(), nullptr);
					}

					// Item found.
					// At this point, there is no reason to continue
					// checking the actor's inventory.
					return;
				}
			}
		}

		void ChangeFormHotkeyStatus
		(
			RE::Actor* a_actor, RE::TESForm* a_form, const int8_t& a_hotkeySlotToSet
		)
		{
			// Add/remove hotkey to the given form for the given actor.
			// Set -1 as the hotkey index to remove the hotkey.

			if (!a_actor || !a_form || a_hotkeySlotToSet < -1 || a_hotkeySlotToSet > 7)
			{
				return;
			}

			auto magicFavorites = RE::MagicFavorites::GetSingleton();
			if (!magicFavorites)
			{
				return;
			}

			// Have to check both inventory objects and magic favorites.
			// We'll do magic favorites first.
			bool formIsMagical = a_form->Is(RE::FormType::Spell, RE::FormType::Shout);
			for (auto i = 0; i < magicFavorites->hotkeys.size(); ++i)
			{
				// Request to clear and the requested form was found, so clear out the hotkey.
				if (a_hotkeySlotToSet == -1 && magicFavorites->hotkeys[i] == a_form)
				{
					SPDLOG_DEBUG
					(
						"[Util] ChangeFormHotkeyStatus: {}: Removed MAG {} from hotkey slot {}.",
						a_actor->GetName(), a_form->GetName(), i + 1
					);
					magicFavorites->hotkeys[i] = nullptr;
				}
				else if (a_hotkeySlotToSet != -1 && i == a_hotkeySlotToSet)
				{
					// Request to set and the requested form is magical,
					// so set this slot to the form.
					if (formIsMagical)
					{
						SPDLOG_DEBUG
						(
							"[Util] ChangeFormHotkeyStatus: {}: Added MAG {} to hotkey slot {}.",
							a_actor->GetName(), a_form->GetName(), i + 1
						);
						magicFavorites->hotkeys[i] = a_form;
					}
					else if (magicFavorites->hotkeys[i])
					{
						// Request to set but the requested form is not magical, so clear the slot.
						// Still have to look for the form among the actor's physical favorites.
						SPDLOG_DEBUG
						(
							"[Util] ChangeFormHotkeyStatus: {}: "
							"Removed MAG {} from hotkey slot {}, "
							"since we want to set PHYS {} as the new hotkeyed form.",
							a_actor->GetName(),
							magicFavorites->hotkeys[i]->GetName(), 
							i + 1,
							a_form->GetName()
						);
						magicFavorites->hotkeys[i] = nullptr;
					}
				}
			}

			auto inventoryChanges = a_actor->GetInventoryChanges();
			if (!inventoryChanges)
			{
				return;
			}

			// Look for the form in the actor's inventory.
			auto inventory = a_actor->GetInventory();
			for (auto& [boundObj, entryDataPair] : inventory)
			{
				const auto& [count, ied] = entryDataPair;
				if (!boundObj || count <= 0 || !ied)
				{
					continue;
				}

				// Exists and has a non-zero count, but no exralists, 
				// so no possibility of being favorited, continue.
				if (!ied->extraLists)
				{
					continue;
				}

				for (auto exDataList : *ied->extraLists)
				{
					// No extra data list, not favorited.
					if (!exDataList)
					{
						continue;
					}

					auto exHotkeyData = exDataList->GetByType<RE::ExtraHotkey>();
					// No ExtraHotkey data, can't be hotkeyed.
					if (!exHotkeyData)
					{
						continue;
					}

					const auto hotkeySlot = (int32_t)(*exHotkeyData->hotkey);
					// Request to remove hotkey and the form was found with an assigned hotkey,
					// so unbind it.
					if (a_hotkeySlotToSet == -1 && boundObj == a_form && hotkeySlot != -1) 
					{
						exHotkeyData->hotkey = RE::ExtraHotkey::Hotkey::kUnbound;
						SPDLOG_DEBUG
						(
							"[Util] ChangeFormHotkeyStatus: {}: "
							"Removed PHYS {} from hotkey slot {}.",
							a_actor->GetName(), a_form->GetName(), hotkeySlot + 1
						);

						// Already removed the hotkey from the requested form,
						// which can only have 1 entry in the actor's inventory, 
						// so there's nothing more to do.
						return;
					}
					else if (a_hotkeySlotToSet != -1)
					{
						// NOTE: 
						// We don't return early when adding hotkeys to physical forms
						// because multiple physical forms can be bound to the same hotkey slot
						// and we have to ensure that we remove the hotkey slot bindings 
						// for all other forms that share the same requested slot.
						// Unfortunately, this means we have to traverse the entire inventory.
						if (hotkeySlot == a_hotkeySlotToSet) 
						{
							// Form is already hotkeyed with the requested slot.
							if (boundObj != a_form)
							{
								if (formIsMagical) 
								{
									SPDLOG_DEBUG
									(
										"[Util] ChangeFormHotkeyStatus: {}: "
										"Removed PHYS {} from hotkey slot {}, "
										"since we want to set MAG {} as the new hotkeyed form.",
										a_actor->GetName(), 
										boundObj->GetName(), 
										hotkeySlot + 1,
										a_form->GetName()
									);
								}
								else
								{
									SPDLOG_DEBUG
									(
										"[Util] ChangeFormHotkeyStatus: {}: "
										"Removed PHYS {} from hotkey slot {}, "
										"since we want to set PHYS {} as the new hotkeyed form.",
										a_actor->GetName(),
										boundObj->GetName(),
										hotkeySlot + 1, 
										a_form->GetName()
									);
								}

								exHotkeyData->hotkey = RE::ExtraHotkey::Hotkey::kUnbound;
							}
						}
						else if (boundObj == a_form)
						{
							// This requested form is not hotkeyed in the same slot as requested,
							// so we can directly set its hotkey slot to the requested one.
							SPDLOG_DEBUG
							(
								"[Util] ChangeFormHotkeyStatus: {}: "
								"Added PHYS {} to hotkey slot {}.",
								a_actor->GetName(), a_form->GetName(), a_hotkeySlotToSet + 1
							);
							exHotkeyData->hotkey = static_cast<RE::ExtraHotkey::Hotkey>
							(
								a_hotkeySlotToSet
							);
						}
					}
				}
			}
		}

		void ChangeNodeColliderState
		(
			RE::Actor* a_actor,
			RE::NiAVObject* a_node,
			PrecisionAnnotationReqType&& a_reqType,
			float&& a_damageMult,
			float&& a_lengthMult
		)
		{
			// Using Precision, construct a collider around the given node
			// via sending an animation event with annotations derived from the given type.
			// Can also modify the damage applied on hit and the length of the collider.

			if (!a_actor || !PrecisionCompat::g_precisionInstalled)
			{
				return;
			}

			RE::BSAnimationGraphManagerPtr manager{ };
			a_actor->GetAnimationGraphManager(manager);
			if (!manager)
			{
				return;
			}

			int32_t activeGraphIdx = manager->activeGraph;
			if (activeGraphIdx < 0 || 
				activeGraphIdx >= manager->graphs.size() || 
				!manager->graphs[activeGraphIdx])
			{
				return;
			}

			RE::BSTEventSource<RE::BSAnimationGraphEvent>* eventSource = 	
			(
				manager->graphs[activeGraphIdx].get()
			);
			switch (a_reqType)
			{
			case (PrecisionAnnotationReqType::kStart):
			{
				// Signal that colliders will be added.
				auto event = std::make_unique<RE::BSAnimationGraphEvent>
				(
					"Collision_AttackStart", 
					a_actor, 
					""
				);
				eventSource->SendEvent(event.get());
				event.release();

				break;
			}
			case (PrecisionAnnotationReqType::kAdd):
			{
				// Add collider to the given node. Can specify damage and collider length.
				if (!a_node) 
				{
					return;
				}

				auto event = std::make_unique<RE::BSAnimationGraphEvent>
				(
					"Collision_Add", 
					a_actor, 
					fmt::format
					(
						"Node({})|ID({})||DamageMult({})|LengthMult({})",
						a_node->name, 1, a_damageMult, a_lengthMult
					)
				);
				eventSource->SendEvent(event.get());
				event.release();

				break;
			}
			case (PrecisionAnnotationReqType::kRemove):
			{
				// Remove a collider from the given node.
				if (!a_node) 
				{
					return;
				}

				auto event = std::make_unique<RE::BSAnimationGraphEvent>
				(
					"Collision_Remove", 
					a_actor, 
					fmt::format("Node({})", a_node->name)
				);
				eventSource->SendEvent(event.get());
				event.release();

				break;
			}
			case (PrecisionAnnotationReqType::kRemoveAll):
			{
				// Remove all added colliders.
				auto event = std::make_unique<RE::BSAnimationGraphEvent>
				(
					"Collision_AttackEnd", 
					a_actor, 
					""
				);
				eventSource->SendEvent(event.get());
				event.release();

				break;
			}
			case (PrecisionAnnotationReqType::kMultiHit):
			{
				// Continue listening for collisions even after the collider(s) hit something.
				if (!a_node) 
				{
					return;
				}

				auto event = std::make_unique<RE::BSAnimationGraphEvent>
				(
					"Collision_ClearTargets", 
					a_actor, 
					fmt::format("Node({})", a_node->name)
				);
				eventSource->SendEvent(event.get());
				event.release();

				break;
			}
			default:
			{
				break;
			}
			}
		}

		bool ChangePerk(RE::Actor* a_actor, RE::BGSPerk* a_perk, bool&& a_add, int32_t a_rank)
		{
			// Add or remove the perk from the actor.

			bool succ = false;
			if (!a_actor || !a_perk)
			{
				return false;
			}

			if (auto actorBase = a_actor->GetActorBase(); actorBase)
			{
				// Credits to po3 for perk application/removal methods.
				//https://github.com/powerof3/PapyrusExtenderSSE/blob/master/include/Serialization/Services.h#L54
				if (a_add)
				{
					// Add perk and apply perk entry first.
					if (succ = actorBase->AddPerk(a_perk, a_rank); succ)
					{
						for (auto& perkEntry : a_perk->perkEntries)
						{
							if (perkEntry)
							{
								perkEntry->ApplyPerkEntry(a_actor);
							}
						}
					}
				}
				else
				{
					// Remove perk and perk entry first.
					if (succ = actorBase->RemovePerk(a_perk); succ)
					{
						for (auto& perkEntry : a_perk->perkEntries)
						{
							if (perkEntry)
							{
								perkEntry->RemovePerkEntry(a_actor);
							}
						}
					}
				}

				// Armor AV changed + reset weights.
				if (succ)
				{
					a_actor->OnArmorActorValueChanged();
					if (auto invChanges = a_actor->GetInventoryChanges(); invChanges)
					{
						invChanges->armorWeight = invChanges->totalWeight;
						invChanges->totalWeight = -1.0f;
						a_actor->equippedWeight = -1.0f;
					}
				}
			}

			if (a_actor->IsPlayerRef()) 
			{
				// Call actor add/remove perk function for P1.
				// Does not function for NPCs.
				auto p1 = RE::PlayerCharacter::GetSingleton();
				if (p1)
				{
					if (a_add) 
					{
						p1->AddPerk(a_perk, a_rank);
					}
					else
					{
						p1->RemovePerk(a_perk);
					}
				}
			}

			// Then check if the perk was applied/removed as requested.
			return
			(
				a_add ?
				a_actor->HasPerk(a_perk) :
				!a_actor->HasPerk(a_perk)
			);
		}

		RE::ThumbstickEvent* CreateThumbstickEvent
		(
			const RE::BSFixedString& a_userEvent, float a_xValue, float a_yValue, bool a_isLS
		)
		{
			// Create and return a thumbstick event using the provided data and return it.
			// NOTE: 
			// Must be free'd by the caller. Ideally, wrap in a smart ptr first.

			auto thumbstickEvent = RE::malloc<RE::ThumbstickEvent>(sizeof(RE::ThumbstickEvent));
			std::memset(thumbstickEvent, 0, sizeof(RE::ThumbstickEvent));
			if (thumbstickEvent)
			{
				reinterpret_cast<std::uintptr_t*>(thumbstickEvent)[0] = 
				(
					RE::VTABLE_ThumbstickEvent[0].address()
				);
				thumbstickEvent->device = RE::INPUT_DEVICE::kGamepad;
				thumbstickEvent->eventType = RE::INPUT_EVENT_TYPE::kThumbstick;
				thumbstickEvent->next = nullptr;
				thumbstickEvent->idCode = 
				(
					a_isLS ? 
					RE::ThumbstickEvent::InputType::kLeftThumbstick : 
					RE::ThumbstickEvent::InputType::kRightThumbstick
				);
				thumbstickEvent->userEvent = a_userEvent;
				thumbstickEvent->xValue = a_xValue;
				thumbstickEvent->yValue = a_yValue;
				// Signal as P1 proxied thumbstick event.
				thumbstickEvent->pad24 = 0xC0DA;
			}

			return thumbstickEvent;
		}

		void EnableCollisionForActor(RE::Actor* a_actor)
		{
			// Enable collisions for the given actor by setting their collision layer to 'Biped'.
			if (!a_actor)
			{
				return;
			}

			a_actor->SetCollision(true);
			auto actor3DPtr = GetRefr3D(a_actor);
			if (!actor3DPtr)
			{
				return;
			}

			// Change collision layer back to 'Biped'.
			actor3DPtr->SetCollisionLayer(RE::COL_LAYER::kBiped);
		}
	
		void ForEachReferenceInCellWithinRange
		(
			RE::TESObjectCELL* a_cell,
			RE::NiPoint3 a_originPos, 
			float a_radius,
			const bool& a_use3DDist, 
			std::function<RE::BSContainer::ForEachResult(RE::TESObjectREFR* a_refr)> a_callback
		)
		{
			// Modified version of TESObjectCELL's ForEachReferenceInRange() function
			// to use cell refr's 3D center positions instead of the refr data world positions.

			const float squaredRadius = a_radius * a_radius;
			a_cell->ForEachReference
			(
				[&](RE::TESObjectREFR* a_refr) 
				{
					if (!a_refr) 
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}
					
					// We pick the smaller of distances to the two positions below 
					// since for certain refrs, especially activators,
					// the center position might be very far from the refr's reported location,
					// and we only need one or the other to be in range.
					const auto& refrPos = a_refr->data.location;
					const auto refrPosCenter = Get3DCenterPos(a_refr);
					const auto distance = 
					(
						a_use3DDist ? 
						min
						(
							a_originPos.GetSquaredDistance(refrPos), 
							a_originPos.GetSquaredDistance(refrPosCenter)
						) : 
						powf
						(
							min
							(
								GetXYDistance(a_originPos, refrPos), 
								GetXYDistance(a_originPos, refrPosCenter)
							), 
							2.0f
						)
					);

					return 
					(
						distance <= squaredRadius ? 
						a_callback(a_refr) : 
						RE::BSContainer::ForEachResult::kContinue
					);
				}
			);
		}

		void ForEachReferenceInRange
		(
			RE::NiPoint3 a_originPos, 
			float a_radius,
			const bool& a_use3DDist, 
			std::function<RE::BSContainer::ForEachResult(RE::TESObjectREFR* a_refr)> a_callback
		)
		{
			// Credits to Ryan for the CommonLibSSE TES::ForEachReferenceInRange() function,
			// and Shrimperator for their adaptation of the function 
			// to use an origin position instead of an origin reference:
			// https://gitlab.com/Shrimperator/skyrim-mod-betterthirdpersonselection/-/blob/main/src/lib/Util.cpp
			// NOTE: 
			// Do not queue tasks in the passed filter function. Game will freeze.

			auto tes = RE::TES::GetSingleton();
			if (tes->interiorCell)
			{
				// Directly run callback on each refr in the interior cell.
				ForEachReferenceInCellWithinRange
				(
					tes->interiorCell, a_originPos, a_radius, a_use3DDist, 
					[&](RE::TESObjectREFR* a_refr) 
					{
						return a_callback(a_refr);
					}
				);
			}
			else
			{
				// Check attached grid cells' refrs within radius of the origin point.
				const auto gridLength = tes->gridCells ? tes->gridCells->length : 0; 
				if (gridLength > 0)
				{
					const float yPlus = a_originPos.y + a_radius;
					const float yMinus = a_originPos.y - a_radius;
					const float xPlus = a_originPos.x + a_radius;
					const float xMinus = a_originPos.x - a_radius;
					std::uint32_t x = 0;
					do
					{
						std::uint32_t y = 0;
						do
						{
							auto cell = tes->gridCells->GetCell(x, y);
							if (!cell || !cell->IsAttached())
							{
								++y;
								continue;
							}

							const auto cellCoords = cell->GetCoordinates();
							if (!cellCoords)
							{
								++y;
								continue;
							}

							const RE::NiPoint2 worldPos{ cellCoords->worldX, cellCoords->worldY };
							// Cell is within range of the origin position.
							if (worldPos.x < xPlus && 
								(worldPos.x + 4096.0f) > xMinus && 
								worldPos.y < yPlus && 
								(worldPos.y + 4096.0f) > yMinus)
							{
								ForEachReferenceInCellWithinRange
								(
									cell, a_originPos, a_radius, a_use3DDist, 
									[&](RE::TESObjectREFR* a_refr) 
									{
										return a_callback(a_refr);
									}
								);
							}

							++y;
						} while (y < gridLength);

						++x;
					} while (x < gridLength);
				}
			}

			if (const auto skyCell = tes->worldSpace ? tes->worldSpace->skyCell : nullptr; skyCell)
			{
				// Run callback on all refrs in the current sky cell.
				ForEachReferenceInCellWithinRange
				(
					skyCell, a_originPos, a_radius, a_use3DDist, 
					[&](RE::TESObjectREFR* a_refr) 
					{
						return a_callback(a_refr);
					}
				);
			}
		}

		SkillList GetActorSkillLevels(RE::Actor* a_actor)
		{
			// Get a list of all this player's skill levels.

			auto skillsArr = SkillList();
			skillsArr.fill(15.0f);
			// Default to 15's across the board.
			if (!a_actor)
			{
				return skillsArr;
			}

			constexpr auto totalSkills = Skill::kTotal;
			Skill currentSkill = totalSkills;
			RE::ActorValue currentAV = RE::ActorValue::kNone;
			for (auto i = 0; i < totalSkills; ++i)
			{
				// Skill AVs
				currentSkill = static_cast<Skill>(i);
				const auto iter = glob.SKILL_TO_AV_MAP.find(currentSkill);
				if (iter == glob.SKILL_TO_AV_MAP.end())
				{
					continue;
				}

				currentAV = iter->second;
				// Enderal companion player skills are hardcoded to start at level 5,
				// and can't be changed directly since they are auto-calculated
				// when scaled with P1.
				// Make sure that the base values are at the minimum 15, 
				// just like P1's skills when starting the game.
				if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
				{
					skillsArr[i] = 15.0f;  
				}
				else
				{
					skillsArr[i] = a_actor->GetBaseActorValue(currentAV);
				}
			}

			return skillsArr;
		}

		RE::BSFixedString GetActivationText
		(
			RE::TESBoundObject* a_baseObj, RE::TESObjectREFR* a_refr, bool& a_hasActivationText
		)
		{
			// Get modified activation text for the given refr.
			// Return whether or not the refr has valid activation text (is a selectable object)
			// through the outparam.
			
			a_hasActivationText = false;
			if (!a_baseObj || !a_refr)
			{
				return ""sv;
			}

			// NOTE:
			// Saving option to bold the name of the refr.
			//RE::BSFixedString activationText = fmt::format
			//(
			//	"<font face=\"$EverywhereBoldFont\">{}</font>", a_refr->GetName()
			//);
			//RE::BSString baseActivationText{ "" };
			//a_baseObj->GetActivateText(a_refr, baseActivationText);
			//// Get modified activation text for the given refr.
			//std::string activateStr{ baseActivationText.c_str() };
			//auto firstNewlinePos = activateStr.find_first_of("\n") + 1;
			//if (firstNewlinePos != std::string::npos && firstNewlinePos < activateStr.size())
			//{
			//	activationText = fmt::format
			//	(
			//		"{} <font face=\"$EverywhereBoldFont\">{}</font>",
			//		activateStr.substr(0, firstNewlinePos - 1),
			//		activateStr.substr(firstNewlinePos)
			//	);
			//	a_hasActivationText = true;
			//}

			RE::BSFixedString activationText = a_refr->GetName();
			RE::BSString baseActivationText{ "" };
			a_baseObj->GetActivateText(a_refr, baseActivationText);
			// Get modified activation text for the given refr.
			std::string activateStr{ baseActivationText.c_str() };
			auto firstNewlinePos = activateStr.find_first_of("\n") + 1;
			if (firstNewlinePos != std::string::npos && firstNewlinePos < activateStr.size())
			{
				activationText = activateStr;
				a_hasActivationText = true;
			}

			return activationText;
		}

		float GetBoundMaxOrMinEdgePixelDist(RE::TESObjectREFR* a_refr, bool&& a_max)
		{
			// Get the maximum/minimum bound edge length in pixels for the given refr.
			// Retrieves the base edge length via the refr's bounds, 
			// orients the bounds at the refr's center position in worldspace
			// and so that the axis of interest is perpendicular to the camera's facing direction,
			// and then calculates the pixel distance from one end of the edge to the other.
			// Then returns the max/min of the three distances.
			
			if (!a_refr)
			{
				return 1.0f;
			}
			
			RE::NiPoint3 boundMax{ };
			RE::NiPoint3 boundMin{ };
			RE::NiPoint3 boundCenter{ };
			auto asActor = a_refr->As<RE::Actor>();
			boundMax = a_refr->GetBoundMax();
			boundMin = a_refr->GetBoundMin();
			boundCenter = a_refr->data.location;
			bool isDead = a_refr->IsDead();
			bool isKnocked = asActor && asActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal;
			bool isRagdolled = asActor && asActor->IsInRagdollState();
			bool isUprightActor = asActor && !isDead && !isKnocked && !isRagdolled;
			if (isUprightActor)
			{
				// Offset halfway up the actor if upright.
				boundCenter = 
				(
					asActor->data.location + 
					RE::NiPoint3(0.0f, 0.0f, 0.5f * asActor->GetHeight())
				);
			}
			else if (auto refrHkpRigidBodyPtr = GethkpRigidBody(a_refr); refrHkpRigidBodyPtr)
			{
				if ((asActor) && (isDead || isKnocked || isRagdolled))
				{
					// Centered at the rigid body's position when ragdolled.
					// The 3D center position is still upright, so we can't use it.
					boundCenter = ToNiPoint3
					(
						refrHkpRigidBodyPtr->motion.motionState.transform.translation *
						HAVOK_TO_GAME
					);
				}
				else
				{
					// 3D center pos otherwise.
					boundCenter = Get3DCenterPos(a_refr);
				}

				// Grab bounds from collidable shape and the refr bounds are unspecified.
				if ((refrHkpRigidBodyPtr->collidable.GetShape() &&
					 refrHkpRigidBodyPtr->collidable.GetShape()->type == RE::hkpShapeType::kBox) &&
					(boundMax == RE::NiPoint3() || boundMin == RE::NiPoint3()))
				{
					auto shape = refrHkpRigidBodyPtr->collidable.GetShape();
					RE::hkTransform hkTrans{ };
					hkTrans.rotation.col0 = { 1.0f, 0.0f, 0.0f, 0.0f };
					hkTrans.rotation.col1 = { 0.0f, 1.0f, 0.0f, 0.0f };
					hkTrans.rotation.col2 = { 0.0f, 0.0f, 1.0f, 0.0f };
					RE::hkAabb aabb{ };
					shape->GetAabbImpl(hkTrans, 0.0f, aabb);
					boundMax = ToNiPoint3(aabb.max) * HAVOK_TO_GAME;
					boundMin = ToNiPoint3(aabb.min) * HAVOK_TO_GAME;
				}
			}
			
			auto hit3DPtr = GetRefr3D(a_refr); 
			if (hit3DPtr)
			{
				if (boundMin == boundMax && boundMax.Length() == 0.0f)
				{
					// Fall back to the radius for the bounds.
					boundMax = 
					(
						RE::NiPoint3(0.0f, 1.0f, 0.0f) * hit3DPtr->worldBound.radius
					);
					boundMin = -boundMax;
				}
			}

			// Next fallback: halfway up the refr as the center position.
			if (boundCenter.Length() == 0.0f)
			{
				boundCenter = 
				(
					a_refr->data.location + 
					RE::NiPoint3(0.0f, 0.0f, 0.5f * a_refr->GetHeight())
				);
			}

			// Last fallback: bounds determined by half the refr's height.
			if (boundMin == boundMax && boundMax.Length() == 0.0f)
			{
				boundMax = 
				(
					RE::NiPoint3(0.0f, 1.0f, 0.0f) * 0.5f * a_refr->GetHeight()
				);
				boundMin = -boundMax;
			}
		
			// Offset from the bounding box's center to one of the corners 
			// along the positive X and Y axes.
			auto halfExtent = (boundMax - boundMin) / 2.0f;

			enum
			{
				kX,
				kY,
				kZ
			};
			auto axis = kX;
			// Max/min edge pixel distance.
			float chosenDist = a_max ? -FLT_MAX : FLT_MAX;
			// Current edge's pixel distance.
			float edgeDist = 0.0f;
			// Current half extents coordinate.
			float halfCoord = 0.0f;
			// Camera 'up' axis.
			RE::NiPoint3 camUp = 
			(
				glob.cam->playerCam && glob.cam->playerCam->cameraRoot ? 
				glob.cam->playerCam->cameraRoot->local.rotate * RE::NiPoint3(0.0f, 0.0f, 1.0f) :
				Util::RotationToDirectionVect
				(
					-Util::NormalizeAngToPi(glob.cam->GetCurrentPitch() - PI / 2.0f),
					Util::ConvertAngle(glob.cam->GetCurrentYaw())
				)
			);
			// Camera 'right' axis.
			RE::NiPoint3 camRight = 
			(
				glob.cam->playerCam && glob.cam->playerCam->cameraRoot ? 
				glob.cam->playerCam->cameraRoot->local.rotate * RE::NiPoint3(1.0f, 0.0f, 0.0f) :
				Util::RotationToDirectionVect
				(
					0.0f,
					Util::ConvertAngle
					(
						Util::NormalizeAng0To2Pi(glob.cam->GetCurrentYaw() + PI / 2.0f)
					)
				)
			);

			// Edge along the X axis.
			halfCoord = halfExtent.x;
			edgeDist = std::clamp
			(
				WorldToScreenPoint3(boundCenter + camRight * halfCoord, false).GetDistance
				(
					WorldToScreenPoint3(boundCenter - camRight * halfCoord, false)
				),
				1.0f, 
				DebugAPI::screenResX
			);
			if ((a_max && edgeDist > chosenDist) || (!a_max && edgeDist < chosenDist))
			{
				chosenDist = edgeDist;
				axis = kX;
			}
			
			// Edge along the Y axis.
			halfCoord = halfExtent.y;
			edgeDist = std::clamp
			(
				WorldToScreenPoint3(boundCenter + camRight * halfCoord, false).GetDistance
				(
					WorldToScreenPoint3(boundCenter - camRight * halfCoord, false)
				),
				1.0f, 
				DebugAPI::screenResX
			);
			if ((a_max && edgeDist > chosenDist) || (!a_max && edgeDist < chosenDist))
			{
				chosenDist = edgeDist;
				axis = kY;
			}
			
			// Edge along the Z axis.
			halfCoord = halfExtent.z;
			edgeDist = std::clamp
			(
				WorldToScreenPoint3(boundCenter + camUp * halfCoord, false).GetDistance
				(
					WorldToScreenPoint3(boundCenter - camUp * halfCoord, false)
				),
				1.0f, 
				DebugAPI::screenResY
			);
			if ((a_max && edgeDist > chosenDist) || (!a_max && edgeDist < chosenDist))
			{
				chosenDist = edgeDist;
				axis = kZ;
			}
			
			return chosenDist;
		}

		float GetBoundPixelDist(RE::TESObjectREFR* a_refr, bool&& a_vertAxis)
		{
			// Return the vertical or horizontal axis's screenspace length for the given refr.
			// Computed as the X/Y coordinate difference between the min and max X/Y coordinates
			// retrieved from the refr's bounding box endpoints.

			if (!a_refr)
			{
				return 1.0f;
			}
			
			RE::NiPoint3 boundMax{ };
			RE::NiPoint3 boundMin{ };
			RE::NiPoint3 boundCenter{ };
			RE::NiMatrix3 rotMat{ }; 
			auto asActor = a_refr->As<RE::Actor>();
			boundMax = a_refr->GetBoundMax();
			boundMin = a_refr->GetBoundMin();
			boundCenter = a_refr->data.location;
			bool isDead = a_refr->IsDead();
			bool isKnocked = asActor && asActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal;
			bool isRagdolled = asActor && asActor->IsInRagdollState();
			bool isUprightActor = asActor && !isDead && !isKnocked && !isRagdolled;
			if (isUprightActor)
			{
				// Offset halfway up the actor if upright.
				boundCenter = 
				(
					asActor->data.location + 
					RE::NiPoint3(0.0f, 0.0f, 0.5f * asActor->GetHeight())
				);
			}
			else if (auto refrHkpRigidBodyPtr = GethkpRigidBody(a_refr); refrHkpRigidBodyPtr)
			{
				if ((asActor) && (isDead || isKnocked || isRagdolled))
				{
					// Centered at the rigid body's position when ragdolled.
					// The 3D center position is still upright, so we can't use it.
					boundCenter = ToNiPoint3
					(
						refrHkpRigidBodyPtr->motion.motionState.transform.translation *
						HAVOK_TO_GAME
					);
				}
				else
				{
					// 3D center pos otherwise.
					boundCenter = Get3DCenterPos(a_refr);
				}

				// Grab bounds from collidable shape.
				if (refrHkpRigidBodyPtr->collidable.GetShape() &&
					refrHkpRigidBodyPtr->collidable.GetShape()->type == 
					RE::hkpShapeType::kBox)
				{
					auto shape = refrHkpRigidBodyPtr->collidable.GetShape();
					RE::hkTransform hkTrans{ };
					hkTrans.rotation.col0 = { 1.0f, 0.0f, 0.0f, 0.0f };
					hkTrans.rotation.col1 = { 0.0f, 1.0f, 0.0f, 0.0f };
					hkTrans.rotation.col2 = { 0.0f, 0.0f, 1.0f, 0.0f };
					RE::hkAabb aabb{ };
					shape->GetAabbImpl(hkTrans, 0.0f, aabb);
					boundMax = ToNiPoint3(aabb.max) * HAVOK_TO_GAME;
					boundMin = ToNiPoint3(aabb.min) * HAVOK_TO_GAME;
				}
			}
			
			auto hit3DPtr = GetRefr3D(a_refr); 
			if (hit3DPtr)
			{
				// Rotation from the refr's 3D.
				rotMat = hit3DPtr->world.rotate;
				if (boundMin == boundMax && boundMax.Length() == 0.0f)
				{
					// Fall back to the radius for the bounds.
					boundMax = 
					(
						RE::NiPoint3(0.0f, 1.0f, 0.0f) * hit3DPtr->worldBound.radius
					);
					boundMin = -boundMax;
				}
			}
			else
			{
				// Set rotation using the refr data angles as a fallback.
				SetRotationMatrixPYR
				(
					rotMat,
					a_refr->data.angle.x,
					a_refr->data.angle.z,
					a_refr->data.angle.y
				);
			}

			// Next fallback: halfway up the refr as the center position.
			if (boundCenter.Length() == 0.0f)
			{
				boundCenter = 
				(
					a_refr->data.location + 
					RE::NiPoint3(0.0f, 0.0f, 0.5f * a_refr->GetHeight())
				);
			}

			// Last fallback: bounds determined by half the refr's height.
			if (boundMin == boundMax && boundMax.Length() == 0.0f)
			{
				boundMax = 
				(
					RE::NiPoint3(0.0f, 1.0f, 0.0f) * 0.5f * a_refr->GetHeight()
				);
				boundMin = -boundMax;
			}
		
			// Offset from the bounding box's center to one of the corners 
			// along the positive X and Y axes.
			auto halfExtent = (boundMax - boundMin) / 2.0f;

			//
			// Compute the minimum and maximum X or Y screen coordinates from all the edges.
			//
			
			float maxCoord = -FLT_MAX;
			float minCoord = FLT_MAX;
			auto setMinMaxCoords = 
			[a_vertAxis, &maxCoord, &minCoord]
			(const std::pair<RE::NiPoint3, RE::NiPoint3>& a_endpoints)
			{
				if (a_vertAxis)
				{
					if (a_endpoints.first.y > maxCoord)
					{
						maxCoord = a_endpoints.first.y;
					}
					else if (a_endpoints.first.y < minCoord)
					{
						minCoord = a_endpoints.first.y;
					}

					if (a_endpoints.second.y > maxCoord)
					{
						maxCoord = a_endpoints.second.y;
					}
					else if (a_endpoints.second.y < minCoord)
					{
						minCoord = a_endpoints.second.y;
					}
				}
				else
				{
					if (a_endpoints.first.x > maxCoord)
					{
						maxCoord = a_endpoints.first.x;
					}
					else if (a_endpoints.first.x < minCoord)
					{
						minCoord = a_endpoints.first.x;
					}

					if (a_endpoints.second.x > maxCoord)
					{
						maxCoord = a_endpoints.second.x;
					}
					else if (a_endpoints.second.x < minCoord)
					{
						minCoord = a_endpoints.second.x;
					}
				}	
			};
			
			//
			// Get the endpoints of the bounding box.
			//
		
			// Top face.
			RE::NiPoint3 start = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
			);
			RE::NiPoint3 end = 
			(
				boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
			);
			end = 
			(
				boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
			);
			end = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
			);
			setMinMaxCoords			
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
			);
			end = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			// Bottom face.
			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
			);
			end = 
			(
				boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
			);
			end = 
			(
				boundCenter +
				rotMat * 
				RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			start = 
			(
				boundCenter + 
				rotMat * 
				RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
			);
			end = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
			);
			end = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			// Connecting the faces.
			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
			);
			end = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
			);
			end = 
			(
				boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
			);
			end = 
			(
				boundCenter + 
				rotMat * 
				RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			start = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
			);
			end = 
			(
				boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
			);
			setMinMaxCoords
			(
				{ WorldToScreenPoint3(start, false), WorldToScreenPoint3(end, false) }
			);

			// Return the diff clamped to [1, screen dimension] pixels.
			return 
			(
				std::clamp
				(
					maxCoord - minCoord, 
					1.0f, 
					a_vertAxis ? DebugAPI::screenResY : DebugAPI::screenResX
				)
			);
		}

		RE::COL_LAYER GetCollisionLayer(RE::NiAVObject* a_refr3D)
		{
			// Get the given 3D object's collision layer.

			auto collisionLayer = a_refr3D->GetCollisionLayer();
			if (collisionLayer == RE::COL_LAYER::kUnidentified)
			{
				// Check if the rigid body's collidable has a collision layer next.
				// Must do this for certain refrs, such as actors.
				auto hkpRigidBodyPtr = GethkpRigidBody(a_refr3D);
				if (hkpRigidBodyPtr)
				{
					auto collidable = hkpRigidBodyPtr->GetCollidable();
					if (collidable)
					{
						collisionLayer = collidable->GetCollisionLayer();
					}
				}
			}

			return collisionLayer;
		}

		float GetDetectionPercent(RE::Actor* a_reqActor, RE::Actor* a_detectingActor)
		{
			// Get the detection percent of the requesting actor by the detecting actor 
			// [0.0, 100.0].
			// If not in combat, use the detection level of the detecting actor directly.
			// Otherwise, use the number of stealth points lost.
			
			if (glob.isInCoopCombat)
			{
				return GetStealthPointsLost(a_reqActor, a_detectingActor);
			}
			else
			{
				// Sick formatting, bro.
				return 
				(
					5.0f * 
					(
						20.0f + 
						std::clamp
						(
							static_cast<float>
							(
								a_detectingActor->RequestDetectionLevel(a_reqActor)
							), 
							-20.0f, 
							0.0f
						)
					)
				);
			}
		}

		RE::NiPoint3 GetEulerAnglesFromRotMatrix(const RE::NiMatrix3& a_matrix)
		{
			// Get the rotation matrix's Euler angles 
			// and then return (x, y, z) = (pitch, roll, yaw) in the game's angular coordinates.

			auto eulerAngles = RE::NiPoint3();
			// Transpose first.
			glm::mat4 mat = glm::identity<glm::mat4>();
			mat[0][0] = a_matrix.entry[0][0];
			mat[1][0] = a_matrix.entry[0][1];
			mat[2][0] = a_matrix.entry[0][2];
			mat[0][1] = a_matrix.entry[1][0];
			mat[1][1] = a_matrix.entry[1][1];
			mat[2][1] = a_matrix.entry[1][2];
			mat[0][2] = a_matrix.entry[2][0];
			mat[1][2] = a_matrix.entry[2][1];
			mat[2][2] = a_matrix.entry[2][2];
			glm::extractEulerAngleXZY(mat, eulerAngles.x, eulerAngles.y, eulerAngles.z);
			// Cruddy manual corrections to match the game's coordinate system, 
			// based on the results.
			eulerAngles.x = PI / 2.0f - eulerAngles.x;
			eulerAngles.z = ConvertAngle(eulerAngles.z);

			return eulerAngles;
		}

		RE::NiPoint3 GetEyePosition(RE::Actor* a_actor)
		{
			// Get the position of the actor's eyes.

			if (!a_actor || !a_actor->IsHandleValid() || !a_actor->Is3DLoaded())
			{
				return RE::NiPoint3();
			}

			// Eye body part.
			auto targetEyeBP =
			(
				a_actor->race && a_actor->race->bodyPartData ?
				a_actor->race->bodyPartData->parts[RE::BGSBodyPartDefs::LIMB_ENUM::kEye] :  
				nullptr
			);
			// Get eye world position from the body part first, if available.
			const auto actor3DPtr = GetRefr3D(a_actor); 
			if (actor3DPtr && targetEyeBP)
			{
				auto targetEyeBPPtr = RE::NiPointer<RE::NiAVObject>
				(
					actor3DPtr->GetObjectByName(targetEyeBP->targetName)
				);
				if (targetEyeBPPtr) 
				{
					return targetEyeBPPtr->world.translate;
				}
			}

			// Fall back to the actor's 'looking at' location.
			return a_actor->GetLookingAtLocation();
		}

		double GetGravitationalConstant()
		{
			// Get gravitational constant for P1's current cell.
			// In game units, not havok units.

			double g = Settings::fG * HAVOK_TO_GAME;
			if (glob.player1Actor && glob.player1Actor->parentCell)
			{
				auto bhkWorld = glob.player1Actor->parentCell->GetbhkWorld();
				if (bhkWorld && bhkWorld->GetWorld1())
				{
					g = -bhkWorld->GetWorld1()->gravity.quad.m128_f32[2] * HAVOK_TO_GAME;
				}
			}

			return g;
		}

		RE::hkRefPtr<RE::hkpRigidBody> GethkpRigidBody(RE::NiAVObject* a_node3D)
		{
			// Get a smart pointer to the 3D object's rigid body, if it has one.

			if (!a_node3D)
			{
				return nullptr;
			}

			// Wrap in smart pointer first to hopefully
			// extend the duration of its lifetime until we're done.
			RE::NiPointer<RE::NiAVObject> node3DPtr{ a_node3D }; 
			if (!node3DPtr)
			{
				return nullptr;
			}

			auto collisionObject = node3DPtr->GetCollisionObject(); 
			if (!collisionObject)
			{
				return nullptr;
			}

			auto rigidBodyPtr = RE::NiPointer<RE::bhkRigidBody>(collisionObject->GetRigidBody());
			if (!rigidBodyPtr || !rigidBodyPtr->referencedObject)
			{
				return nullptr;
			}

			auto hkpRigidBodyPtr = RE::hkRefPtr<RE::hkpRigidBody>
			(
				static_cast<RE::hkpRigidBody*>(rigidBodyPtr->referencedObject.get())
			); 
			if (!hkpRigidBodyPtr)
			{
				return nullptr;
			}

			// Finally, a valid rigid body pointer.
			return hkpRigidBodyPtr;
		}

		RE::hkRefPtr<RE::hkpRigidBody> GethkpRigidBody(RE::TESObjectREFR* a_refr)
		{
			// Get a smart pointer to the refr's rigid body, if it has one.

			if (!a_refr)
			{
				return nullptr;
			}
			
			return GethkpRigidBody(a_refr->GetCurrent3D());
		}

		RE::NiPoint3 GetHeadPosition(RE::Actor* a_actor)
		{
			// Get the actor's head position.

			if (!a_actor || !a_actor->IsHandleValid() || !a_actor->Is3DLoaded())
			{
				return RE::NiPoint3();
			}

			// Get the actor's head body part,
			// falling back to their eye and look-at body part's positions, if needed.
			RE::BGSBodyPart* headBP = nullptr;
			if (a_actor->race && a_actor->race->bodyPartData && a_actor->race->bodyPartData->parts)
			{
				auto bpDataList = a_actor->race->bodyPartData->parts;
				if (auto bodyPart = bpDataList[RE::BGSBodyPartDefs::LIMB_ENUM::kHead]; bodyPart)
				{
					headBP = bodyPart;
				}
				else if (bodyPart = bpDataList[RE::BGSBodyPartDefs::LIMB_ENUM::kEye]; bodyPart)
				{
					headBP = bodyPart;
				}
				else if (bodyPart = bpDataList[RE::BGSBodyPartDefs::LIMB_ENUM::kLookAt]; bodyPart)
				{
					headBP = bodyPart;
				}
			}

			const auto actor3DPtr = GetRefr3D(a_actor);
			if (actor3DPtr && headBP)
			{
				auto headBPPtr = RE::NiPointer<RE::NiAVObject>
				(
					actor3DPtr->GetObjectByName(headBP->targetName)
				);
				if (headBPPtr) 
				{
					// Return head body part position.
					return headBPPtr->world.translate;
				}
			}

			// Last fallback is the actor's location offset by their height.
			return a_actor->data.location + RE::NiPoint3(0.0f, 0.0f, a_actor->GetHeight());
		}

		float GetHeadRadius(RE::Actor* a_actor)
		{
			// Get the actor's head node radius.

			if (!a_actor || !a_actor->IsHandleValid() || !a_actor->Is3DLoaded())
			{
				return 0.0f;
			}

			// Get the actor's head body part,
			// falling back on their eye and look-at body part's positions, if needed.
			RE::BGSBodyPart* headBP = nullptr;
			if (a_actor->race && a_actor->race->bodyPartData && a_actor->race->bodyPartData->parts)
			{
				auto bpDataList = a_actor->race->bodyPartData->parts;
				if (auto bodyPart = bpDataList[RE::BGSBodyPartDefs::LIMB_ENUM::kHead]; bodyPart)
				{
					headBP = bodyPart;
				}
				else if (bodyPart = bpDataList[RE::BGSBodyPartDefs::LIMB_ENUM::kEye]; bodyPart)
				{
					headBP = bodyPart;
				}
				else if (bodyPart = bpDataList[RE::BGSBodyPartDefs::LIMB_ENUM::kLookAt]; bodyPart)
				{
					headBP = bodyPart;
				}
			}

			const auto actor3DPtr = GetRefr3D(a_actor);
			if (actor3DPtr && headBP)
			{
				auto headBPPtr = RE::NiPointer<RE::NiAVObject>
				(
					actor3DPtr->GetObjectByName(headBP->targetName)
				);
				if (headBPPtr) 
				{
					return GetRigidBodyCapsuleAxisLength(headBPPtr.get()) / 2.0f;
				}
			}

			// Last fallback is the actor height divided by 8.
			return a_actor->GetHeight() / 8.0f;
		}

		std::pair<RE::TESAmmo*, int32_t> GetHighestCountAmmo
		(
			RE::Actor* a_actor, const bool& a_forBows
		)
		{
			// Search the actor's inventory and return a pair 
			// giving the weapon-matching ammo with the highest count and its count.

			const auto inventoryCounts = a_actor->GetInventoryCounts();
			int32_t highestCount = 0;
			std::pair<RE::TESAmmo*, int32_t> ammoAndCount{ nullptr, 0 };
			for (const auto& [boundObj, count] : inventoryCounts) 
			{
				if (!boundObj || !boundObj->IsAmmo() || count <= 0) 
				{
					continue;
				}

				auto ammo = boundObj->As<RE::TESAmmo>();
				bool forBows = a_forBows && !ammo->IsBolt();
				bool forCrossbows = !a_forBows && ammo->IsBolt();
				if ((forBows || forCrossbows) && (count > highestCount))
				{
					ammoAndCount.first = ammo;
					ammoAndCount.second = count;
					highestCount = count;
				}
			}

			return ammoAndCount;
		}

		std::pair<RE::TESAmmo*, int32_t> GetHighestDamageAmmo
		(
			RE::Actor* a_actor, const bool& a_forBows
		)
		{
			// Search the actor's inventory and return a pair
			// giving the weapon-matching ammo with the highest base damage and its count.

			const auto inventoryCounts = a_actor->GetInventoryCounts();
			float highestDamage = 0.0f;
			std::pair<RE::TESAmmo*, int32_t> ammoAndCount{ nullptr, 0 };
			for (const auto& [boundObj, count] : inventoryCounts)
			{
				if (!boundObj || !boundObj->IsAmmo() || count <= 0) 
				{
					continue;
				}

				auto ammo = boundObj->As<RE::TESAmmo>();
				bool forBows = a_forBows && !ammo->IsBolt();
				bool forCrossbows = !a_forBows && ammo->IsBolt();
				if ((forBows || forCrossbows) && (ammo->data.damage > highestDamage))
				{
					ammoAndCount.first = ammo;
					ammoAndCount.second = count;
					highestDamage = ammo->data.damage;
				}
			}

			return ammoAndCount;
		}

		int32_t GetHotkeyForForm(RE::Actor* a_playerActor, RE::TESForm* a_form)
		{
			// Check if the given form is hotkeyed for the given player and return its slot index.
			// -1 if not hotkeyed, [0, 7] otherwise.
			// NOTE:
			// Should be called after importing any magic hotkeys for companion players.

			if (!a_playerActor || !a_form)
			{
				return -1;
			}

			if (a_form->Is(RE::FormType::Spell, RE::FormType::Shout))
			{
				// If in control of the FavoritesMenu, check P1's current magic favorites,
				// which will account for realtime hotkey changes made in the menu.
				// Since the serialized hotkeyed forms list is only updated when the menu closes,
				// we'll only fall back to it when not in the FavoritesMenu.
				auto ui = RE::UI::GetSingleton();
				bool controllingFavoritesMenu = 
				(
					(ui && ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME)) && 
					(
						(
							(a_playerActor->IsPlayerRef()) && 
							(glob.menuCID == glob.player1CID || glob.menuCID == -1)
						) ||
						(
							glob.menuCID != -1 && 
							GlobalCoopData::GetCoopPlayerIndex(a_playerActor) == glob.menuCID
						)
					)
				);
				if (controllingFavoritesMenu)
				{
					auto magicFavorites = RE::MagicFavorites::GetSingleton();
					if (!magicFavorites)
					{
						return -1;
					}

					for (auto i = 0; i < magicFavorites->hotkeys.size(); ++i)
					{
						if (magicFavorites->hotkeys[i] == a_form)
						{
							return i;
						}
					}
				}
				else if (glob.globalDataInit)
				{
					const auto iter = glob.serializablePlayerData.find(a_playerActor->formID);
					if (iter != glob.serializablePlayerData.end())
					{
						const auto& data = iter->second;
						for (auto i = 0; i < data->hotkeyedForms.size(); ++i)
						{
							auto savedHotkeyedForm = data->hotkeyedForms[i];
							if (savedHotkeyedForm == a_form)
							{
								return i;
							}
						}
					}
				}
			}
			else
			{
				auto inventory = a_playerActor->GetInventory();
				RE::InventoryEntryData* entryData = nullptr;
				// Iterate through the actor's inventory entries.
				for (const auto& inventoryEntry : inventory)
				{
					if (!inventoryEntry.first || inventoryEntry.first != a_form)
					{
						continue;
					}

					// Found the form.
					const auto& entryExtraLists = inventoryEntry.second.second;
					if (!entryExtraLists)
					{
						continue;
					}

					auto extraLists = entryExtraLists->extraLists;
					if (!extraLists)
					{
						continue;
					}

					for (auto exData : *extraLists)
					{
						auto exHotkeyData = exData->GetByType<RE::ExtraHotkey>();
						if (!exHotkeyData)
						{
							continue;
						}

						// Is favorited since the hotkey extra data exists,
						// so we can now grab the hotkey index.
						return (int8_t)(*exHotkeyData->hotkey);
					}
				}
			}

			return -1;
		}

		RE::NiPointer<RE::NiCamera> GetNiCamera()
		{
			// Get the game's NiCamera.

			// Credits to mwilsnd for the method of obtaining the NiCamera.
			// https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/source/camera.cpp#L286
			const auto playerCam = RE::PlayerCamera::GetSingleton(); 
			if (!playerCam) 
			{
				return nullptr;
			}

			if (!playerCam->cameraRoot) 
			{
				return nullptr;
			}

			for (auto childPtr : playerCam->cameraRoot->children) 
			{
				if (!childPtr)
				{
					continue;
				}

				auto asNiCamera = skyrim_cast<RE::NiCamera*>(childPtr.get());
				if (asNiCamera)
				{
					return RE::NiPointer<RE::NiCamera>(asNiCamera);
				}
			}

			return nullptr;
		}

		// Full credits to ersh1: 
		// https://github.com/ersh1/Precision/blob/main/src/Havok/ContactListener.cpp#L8
		RE::hkVector4 GetParentNodeHavokPointVelocity
		(
			RE::NiAVObject* a_node, const RE::hkVector4& a_point
		)
		{
			// Get the given node's parent node's velocity at the given hit position.

			if (!a_node || !a_node->parent) 
			{
				return RE::hkVector4();
			}

			if (a_node->parent->collisionObject) 
			{
				auto hkpRigidBodyPtr = GethkpRigidBody(a_node->parent);
				if (hkpRigidBodyPtr) 
				{
					return hkpRigidBodyPtr->motion.GetPointVelocity(a_point);
				}
			} 
			else 
			{
				return GetParentNodeHavokPointVelocity(a_node->parent, a_point);
			}
			
			return RE::hkVector4();
		}

		RE::TESObjectREFR* GetRefrFrom3D(RE::NiAVObject* a_obj3D)
		{
			// Get a raw pointer to the refr associated with the 3D object, if any. 

			RE::TESObjectREFR* hitRefr{ nullptr };
			if (a_obj3D)
			{
				hitRefr = a_obj3D->userData;
				// Recurse for an associated refr on the 3D object's parent node
				// if the 3D object's user data is nullptr.
				if (!a_obj3D->userData && a_obj3D->parent)
				{
					hitRefr = RecurseForRefr(a_obj3D->parent);
				}
			}

			return hitRefr;
		}

		RE::NiPoint3 GetRefrPosition(RE::TESObjectREFR* a_refr)
		{
			// Get the given refr's position.
			// Default to the refr data's reported position,
			// then if the refr's current 3D is loaded choose the refr's 3D position,
			// then if it's not available, choose the 3D center position,
			// and lastly fall back to the havok rigid body position if necessary.

			RE::NiPoint3 pos{ };
			if (!a_refr)
			{
				return pos;
			}
			
			pos = a_refr->data.location;
			if (auto refr3DPtr = GetRefr3D(a_refr); refr3DPtr)
			{
				pos = refr3DPtr->world.translate;
				if (pos.Length() == 0.0f)
				{
					pos = refr3DPtr->worldBound.center;
					if (pos.Length() == 0.0f)
					{
						auto hkpRigidBodyPtr = GethkpRigidBody(refr3DPtr.get()); 
						if (hkpRigidBodyPtr)
						{
							pos = ToNiPoint3
							(
								hkpRigidBodyPtr->motion.motionState.transform.translation
							) * HAVOK_TO_GAME;
						}
					}
				}
			}

			return pos;
		}

		float GetRigidBodyCapsuleAxisLength(RE::NiAVObject* a_obj3D)
		{
			// Get the tip-to-tip length of the rigid body capsule for the given 3D object.
			// In game units.

			auto hkpRigidBodyPtr = GethkpRigidBody(a_obj3D);
			if (!hkpRigidBodyPtr)
			{
				return 0.0f;
			}

			auto hkpShape = hkpRigidBodyPtr->GetShape(); 
			if (hkpShape->type == RE::hkpShapeType::kCapsule)
			{
				auto hkpCapsuleShape = 
				(
					static_cast<const RE::hkpCapsuleShape*>
					(
						hkpShape
					)
				);
				RE::NiPoint3 vertexA{ };
				RE::NiPoint3 vertexB{ };
				RE::NiPoint3 zAxisDir{ };
				RE::NiPoint3 vertAOffset = 
				(
					ToNiPoint3(hkpCapsuleShape->vertexA) * 
					HAVOK_TO_GAME
				);
				RE::NiPoint3 vertBOffset = 
				(
					ToNiPoint3(hkpCapsuleShape->vertexB) * 
					HAVOK_TO_GAME
				);
				
				const auto& hkTransform = 
				(
					hkpRigidBodyPtr->motion.motionState.transform
				);
				RE::NiTransform niTransform{ };
				niTransform.scale = 1.0f;
				niTransform.translate = 
				(
					ToNiPoint3(hkTransform.translation) * 
					HAVOK_TO_GAME
				);
				niTransform.rotate.entry[0][0] = 
				hkTransform.rotation.col0.quad.m128_f32[0];
				niTransform.rotate.entry[1][0] = 
				hkTransform.rotation.col0.quad.m128_f32[1];
				niTransform.rotate.entry[2][0] = 
				hkTransform.rotation.col0.quad.m128_f32[2];

				niTransform.rotate.entry[0][1] = 
				hkTransform.rotation.col1.quad.m128_f32[0];
				niTransform.rotate.entry[1][1] = 
				hkTransform.rotation.col1.quad.m128_f32[1];
				niTransform.rotate.entry[2][1] = 
				hkTransform.rotation.col1.quad.m128_f32[2];

				niTransform.rotate.entry[0][2] = 
				hkTransform.rotation.col2.quad.m128_f32[0];
				niTransform.rotate.entry[1][2] = 
				hkTransform.rotation.col2.quad.m128_f32[1];
				niTransform.rotate.entry[2][2] = 
				hkTransform.rotation.col2.quad.m128_f32[2];
				vertexA = niTransform * vertAOffset;
				vertexB = niTransform * vertBOffset;
						
				// Vertices are at the endpoints of the Z axis,
				// so we still have to add 2x the radius to get the full axis length.
				return
				(
					(vertexB - vertexA).Length() + 
					2.0f *
					hkpCapsuleShape->radius *
					HAVOK_TO_GAME
				);
			}

			// Fall back to 3D object's world bound diameter.
			return a_obj3D->worldBound.radius * 2.0f;
		}

		void GetSkeletonModelNameForRace(RE::TESRace* a_race, std::string& a_skeletonNameOut)
		{
			// Get a lowercase shortened skeleton model name for the given race.

			// Check the male skeleton model name first, then the female one
			// if it doesn't contain the character assets substring,
			// which I'm using to indicate that the string contains
			// a shortened skeleton model name.

			// Male first.
			a_skeletonNameOut = std::string(a_race->skeletonModels[0].model);
			// Model names seem to precede the '\Character Assets' path substring.
			// Convert to lowercase before returning.
			const auto posM1 = a_skeletonNameOut.find("\\Character Assets"sv); 
			if (posM1 < a_skeletonNameOut.length())
			{
				a_skeletonNameOut = a_skeletonNameOut.substr(0, posM1);
				// Remove everything before the opening backslash to get just the name.
				const auto posM2 = a_skeletonNameOut.rfind("\\"); 
				if (posM2 < a_skeletonNameOut.length())
				{
					a_skeletonNameOut = a_skeletonNameOut.substr(posM2 + 1);
					ToLowercase(a_skeletonNameOut);
				}

				// Substring found, so no need to check the female skeleton model name.
				return;
			}

			// Female next.
			a_skeletonNameOut = std::string(a_race->skeletonModels[1].model);
			const auto posF1 = a_skeletonNameOut.find("\\Character Assets"sv); 
			if (posF1 < a_skeletonNameOut.length())
			{
				a_skeletonNameOut = a_skeletonNameOut.substr(0, posF1);
				// Remove everything before the opening backslash to get just the name.
				const auto posF2 = a_skeletonNameOut.rfind("\\");
				if (posF2 < a_skeletonNameOut.length())
				{
					a_skeletonNameOut = a_skeletonNameOut.substr(posF2 + 1);
					ToLowercase(a_skeletonNameOut);
				}
			}
		}

		float GetStealthPointsLost(RE::Actor* a_playerActor, RE::Actor* a_fromActor)
		{
			// Full credits to max-su-2019 for the method 
			// of retrieving the stealth points decrement
			// and for their Detection Meter mod:
			// https://github.com/max-su-2019/MaxsuDetectionMeter/blob/main/src/DataHandler.cpp#L32
			// Get the number of stealth points that the given actor's combat group removes
			// from the player actor (0 - 100).
			// At 100, the player is fully detected by this group.

			// Ignore if the detecting actor or the player actor are invalid,
			// or if the detecting actor is P1.
			if (!a_fromActor || !a_playerActor || a_fromActor->IsPlayerRef())
			{
				return 0.0f;
			}

			auto isInCombatWithPlayer = 
			[](const RE::CombatTarget& a_targetData, RE::Actor* a_playerActor) -> bool 
			{
				// Targeting the player or a current attacked member 
				// has the player as their combat target.
				if (auto actorPtr = a_targetData.targetHandle.get(); actorPtr)
				{
					return actorPtr.get() == a_playerActor;
				}
				else if (auto attackedMemberPtr = a_targetData.attackedMember.get();
						 attackedMemberPtr)
				{
					auto combatTargetPtr = attackedMemberPtr->currentCombatTarget.get(); 
					if (combatTargetPtr)
					{
						return combatTargetPtr.get() == a_playerActor;
					}
					else
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			};

			// Check the detecting actor's combat group for remaining stealth points.
			auto group = a_fromActor->GetCombatGroup();
			if (group)
			{
				for (const auto& target : group->targets)
				{
					if (isInCombatWithPlayer(target, a_playerActor))
					{
						// When stealth points hit 0, the player is fully detected.
						return std::clamp(100.f - target.stealthPoints, 0.0f, 100.f);
					}
				}
			}

			// No combat group or combat group targets, so not detected.
			return 0.0f;
		}

		RE::NiPoint3 GetTorsoPosition(RE::Actor* a_actor)
		{
			// Get the actor's torso position.
			// Fall back to the actor's world bound center and then halfway up
			// the actor from their feet position if the actor does not have a torso body part.
			// Credits to ersh1 for the method of getting the body part data:
			// https://github.com/ersh1/TrueDirectionalMovement/blob/master/src/Utils.cpp#L78

			if (!a_actor || !a_actor->IsHandleValid() || !a_actor->Is3DLoaded()) 
			{
				return RE::NiPoint3();
			}

			auto targetTorsoBP = 
			(
				a_actor->race && a_actor->race->bodyPartData ?
				a_actor->race->bodyPartData->parts[RE::BGSBodyPartDefs::LIMB_ENUM::kTorso] :
				nullptr
			);

			// Half up the actor first.
			auto targetTorsoPos = 
			(
				a_actor->data.location + RE::NiPoint3(0.0f, 0.0f, a_actor->GetHeight() * 0.5f)
			);
			if (const auto actor3DPtr = GetRefr3D(a_actor); actor3DPtr)
			{
				// 3D bound's center next.
				targetTorsoPos = actor3DPtr->worldBound.center;
				// Then use body part world position if available.
				if (targetTorsoBP) 
				{
					auto targetTorsoBPPtr = RE::NiPointer<RE::NiAVObject>
					(
						actor3DPtr->GetObjectByName(targetTorsoBP->targetName)
					);
					if (targetTorsoBPPtr) 
					{
						targetTorsoPos = targetTorsoBPPtr->world.translate;
					}
				}
			}

			return targetTorsoPos;
		}

		std::pair<float, float> GetVertCollPoints(const RE::NiPoint3& a_point)
		{
			// Get the raycast hit Z coordinates above and below the given points.
			// +- FLT_MAX if no hit.
			// (Above, Below)

			// Unbounded to start.
			std::pair<float, float> vertBoundPoints{ FLT_MAX, -FLT_MAX };
			// Cast reasonably far up and down.
			auto aboveResult = Raycast::hkpCastRay
			(
				ToVec4(a_point), 
				ToVec4(a_point) + glm::vec4(0.0f, 0.0f, 131072.0f, 0.0f), 
				true
			);
			auto belowResult = Raycast::hkpCastRay
			(
				ToVec4(a_point), 
				ToVec4(a_point) - glm::vec4(0.0f, 0.0f, 131072.0f, 0.0f),
				true
			);
			if (aboveResult.hit) 
			{
				vertBoundPoints.first = aboveResult.hitPos.z;
			}
			
			if (belowResult.hit) 
			{
				vertBoundPoints.second = belowResult.hitPos.z;
			}

			return vertBoundPoints;
		}

		std::pair<float, float> GetVertCollPoints
		(
			const RE::NiPoint3& a_point, const float& a_hullSize
		)
		{
			// Get the raycast hit Z coordinates above and below the given points,
			// then offset by the given hull size to place the hit heights
			// above the lower bound and below the upper bound.
			// +- FLT_MAX if no hit.
			// (Above, Below)

			// Unbounded to start.
			std::pair<float, float> vertBoundPoints{ FLT_MAX, -FLT_MAX };
			glm::vec4 start = ToVec4(a_point);
			// Cast reasonably far up and down.
			glm::vec4 abovePoint = ToVec4(a_point) + glm::vec4(0.0f, 0.0f, 131072.0f, 0.0f);
			glm::vec4 belowPoint = ToVec4(a_point) - glm::vec4(0.0f, 0.0f, 131072.0f, 0.0f);
			// Use camera caster to apply hull size.
			// Above.
			Raycast::RayResult result = Raycast::hkpCastRay(start, abovePoint, true);
			if (result.hit)
			{
				vertBoundPoints.first = result.hitPos.z;
			}

			// Below.
			result = Raycast::hkpCastRay(start, belowPoint, true);
			if (result.hit)
			{
				vertBoundPoints.second = result.hitPos.z;
			}

			// Both casts hit the same point if the given point 
			// is within the given hullsize from a surface above and below.
			if (vertBoundPoints.first == vertBoundPoints.second) 
			{
				// Upper bound is above the given point, or is below the given point.
				// Need to keep the lower bound below the upper bound.
				if (vertBoundPoints.first - a_point.z > 0.0f) 
				{
					// Set the lower bound as one hull size below the surface.
					vertBoundPoints.second -= a_hullSize;
				}
				else
				{
					// Set the upper bound as one hull size above the surface.
					vertBoundPoints.first += a_hullSize;
				}
			}

			return vertBoundPoints;
		}

		RE::WEAPON_TYPE GetWeaponTypeFromKeyword(RE::BGSKeyword* a_keyword)
		{
			// Get the weapon type associated with the keyword.

			if (!a_keyword) 
			{
				// No invalid/none weapon enum member defined, so this will have to do.
				return RE::WEAPON_TYPE::kTotal;
			}

			// Need to compare to the keyword's editor ID, since the name isn't loaded.
			const auto& keywordName = a_keyword->formEditorID;
			if (Hash(keywordName) == "WeapTypeSword"_h)
			{
				return RE::WEAPON_TYPE::kOneHandSword;
			}
			else if (Hash(keywordName) == "WeapTypeMace"_h)
			{
				return RE::WEAPON_TYPE::kOneHandMace;
			}
			else if (Hash(keywordName) == "WeapTypeWarAxe"_h)
			{
				return RE::WEAPON_TYPE::kOneHandAxe;
			}
			else if (Hash(keywordName) == "WeapTypeDagger"_h)
			{
				return RE::WEAPON_TYPE::kOneHandDagger;
			}
			else if (Hash(keywordName) == "WeapTypeGreatsword"_h)
			{
				return RE::WEAPON_TYPE::kTwoHandSword;
			}
			else if (Hash(keywordName) == "WeapTypeBattleaxe"_h ||
					 Hash(keywordName) == "WeapTypeWarhammer"_h)
			{
				// No weapon type for warhammers, so set to axe as well.
				return RE::WEAPON_TYPE::kTwoHandAxe;
			}
			else if (Hash(keywordName) == "WeapTypeBow"_h)
			{
				// NOTE: 
				// Crossbows also use this keyword.
				return RE::WEAPON_TYPE::kBow;
			}
			else if (Hash(keywordName) == "WeapTypeStaff"_h)
			{
				return RE::WEAPON_TYPE::kStaff;
			}
			else
			{
				// Default to melee.
				return RE::WEAPON_TYPE::kHandToHandMelee;
			}
		}

		void GetWeightAndValueInRefr
		(
			RE::TESObjectREFR* a_refrContainer,
			float& a_weight,
			int32_t& a_value
		)
		{
			// Get the weight and value of all items in the container/corpse refr.

			// No weight or value by default.
			a_weight = 0.0f;
			a_value = 0;
			if (!a_refrContainer)
			{
				return;
			}

			auto asActor = a_refrContainer->As<RE::Actor>();
			bool isCorpse = asActor && asActor->IsDead();
			if (isCorpse)
			{
				// Check inventory first.
				auto inventory = asActor->GetInventory();
				if (!inventory.empty())
				{
					for (const auto& [boundObj, countInvEntryDataPair] : inventory)
					{
						if (!boundObj || 
							countInvEntryDataPair.first <= 0 || 
							!Util::IsLootableObject(*boundObj))
						{
							continue;
						}
					
						a_weight += boundObj->GetWeight() * countInvEntryDataPair.first;
						if (int32_t value = boundObj->GetGoldValue(); value > -1)
						{
							a_value += value * countInvEntryDataPair.first;
						}
					}
				}

				// Check dropped inventory next.
				auto droppedInventory = asActor->GetDroppedInventory();
				if (!droppedInventory.empty())
				{
					for (const auto& [boundObj, countHandlePair] : droppedInventory)
					{
						if (!boundObj ||
							countHandlePair.first <= 0 || 
							!Util::IsLootableObject(*boundObj))
						{
							continue;
						}
					
							
						a_weight += boundObj->GetWeight() * countHandlePair.first;
						if (int32_t value = boundObj->GetGoldValue(); value > -1)
						{
							a_value += value * countHandlePair.first;
						}
					}
				}
			}
			else
			{
				/*
				auto container = a_refrContainer->GetContainer();
				// Check base container first.
				container->ForEachContainerObject
				(
					[a_refrContainer, &a_weight, &a_value](RE::ContainerObject& a_object) 
					{
						if (a_object.obj && Util::IsLootableObject(*a_object.obj)) 
						{
							if (a_object.count > 0)
							{
								a_weight += a_object.obj->GetWeight() * a_object.count;
								if (int32_t value = a_object.obj->GetGoldValue(); value > -1)
								{
									a_value += value * a_object.count;
								}

								SPDLOG_DEBUG
								(
									"[Util] GetWeightAndValueInRefr: "
									"{}'s base container has {}, weight {}, value {} x{}.",
									a_refrContainer->GetName(),
									a_object.obj->GetName(),
									a_object.obj->GetWeight(),
									a_object.obj->GetGoldValue(),
									a_object.count
								);
							}
						}

						return RE::BSContainer::ForEachResult::kContinue;
					}
				);
				*/

				// Check inventory next.
				auto inventory = a_refrContainer->GetInventory();
				for (const auto& [boundObj, countInvEntryDataPair] : inventory)
				{
					if (!boundObj || 
						countInvEntryDataPair.first <= 0 || 
						!Util::IsLootableObject(*boundObj))
					{
						continue;
					}

					a_weight += boundObj->GetWeight() * countInvEntryDataPair.first;
					if (int32_t value = boundObj->GetGoldValue(); value > -1)
					{
						a_value += value * countInvEntryDataPair.first;
					}
				}
			}
		}

		bool HasLOS
		(
			RE::TESObjectREFR* a_targetRefr,
			RE::Actor* a_observer,
			bool a_forCrosshairSelection, 
			bool a_checkCrosshairPos, 
			const RE::NiPoint3& a_crosshairWorldPos,
			bool a_showDebugInfo
		)
		{
			// Check if the observer has an LOS to the target refr.

			// Invalid target or observer, return false.
			if (!a_observer || 
				!a_targetRefr || 
				!a_targetRefr->loadedData || 
				a_targetRefr->IsDisabled() || 
				a_targetRefr->IsDeleted() || 
				!a_targetRefr->IsHandleValid())
			{
				return false;
			}

			// Skip projectile LOS checks due to instability.
			// Projectiles can get deleted or can become invalid mid-check.
			if (a_targetRefr->GetBaseObject() && 
				*a_targetRefr->GetBaseObject()->formType == RE::FormType::Projectile)
			{
				return true;
			}

			// No player camera, no LOS check, simple as.
			auto playerCam = RE::PlayerCamera::GetSingleton(); 
			if (!playerCam || !playerCam->cameraRoot)
			{
				return false;
			}

			// If the refr is the game's crosshair pick refr, return true right away.
			if (auto pickData = RE::CrosshairPickData::GetSingleton(); pickData)
			{
				if (HandleIsValid(pickData->target) &&
					pickData->target.get().get() == a_targetRefr)
				{
					return true;
				}
			}

			bool hasLOS = false;
			auto observerLOSStartPos = GetActorFocusPoint(a_observer);
			// Ignore the observer in the raycast hit results.
			auto observer3DPtr = GetRefr3D(a_observer);
			// Excluded 3D objects:
			// Camera node, observer, and all players, 
			// as long as they are not the target or observer.
			std::vector<RE::NiAVObject*> excluded3DObjects{ playerCam->cameraRoot.get() };
			if (observer3DPtr)
			{
				excluded3DObjects.emplace_back(observer3DPtr.get());
			}

			std::for_each
			(
				glob.coopPlayers.begin(), glob.coopPlayers.end(),
				[&excluded3DObjects, a_observer, a_targetRefr]
				(const std::shared_ptr<CoopPlayer>& a_p)
				{
					if (!a_p->isActive)
					{
						return;
					}

					auto player3DPtr = GetRefr3D(a_p->coopActor.get());
					if (!player3DPtr || 
						a_p->coopActor.get() == a_targetRefr || 
						a_p->coopActor.get() == a_observer)
					{
						return;
					}
					
					// Valid 3D, not the target, and not the observer.
					excluded3DObjects.emplace_back(player3DPtr.get());
				}
			);

			// NOTE:
			// Same check for both selection and interaction if cam collisions are on, 
			// or if the co-op camera is inactive.
			// This is because the camera is (hopefully) sitting in a valid,
			// reachable position, so we don't have to make sure
			// the targeted refr is reachable for interaction from the observer's current location.
			if (a_forCrosshairSelection || Settings::bCamCollisions || !glob.cam->IsRunning())
			{
				// First, perform the less stringent P1 LOS check before raycasting.
				bool inFrustum = false;
				if (const auto p1 = RE::PlayerCharacter::GetSingleton(); p1) 
				{
					if (a_showDebugInfo)
					{
						SPDLOG_DEBUG("[Util] HasLOS: P1 LOS check.");
					}

					hasLOS = p1->HasLineOfSight(a_targetRefr, inFrustum);
				}
					
				// A handful of different starting points.
				// First, check from the camera node position.
				if (!hasLOS)
				{
					if (a_showDebugInfo)
					{
						SPDLOG_DEBUG("[Util] HasLOS: From camera node pos.");
					}
					
					// Raycast from the camera node's position.
					hasLOS = HasRaycastLOSFromPos
					(
						playerCam->cameraRoot->world.translate,
						a_targetRefr,
						excluded3DObjects, 
						a_checkCrosshairPos, 
						a_crosshairWorldPos,
						a_showDebugInfo
					);
					if (!hasLOS) 
					{
						// Then check from the observer's eye position.
						if (a_showDebugInfo)
						{
							SPDLOG_DEBUG("[Util] HasLOS: From observer focus pos.");
						}

						if (observer3DPtr)
						{
							hasLOS = HasRaycastLOSFromPos
							(
								observerLOSStartPos, 
								a_targetRefr, 
								excluded3DObjects, 
								a_checkCrosshairPos, 
								a_crosshairWorldPos,
								a_showDebugInfo
							);
						}
					}
				}
			}
			else
			{
				// Without camera collisions when the co-op camera is active,
				// the player could be targeting an object that is not reachable 
				// from where they are, so we cannot use the P1's HasLineOfSight() check,
				// which is camera frustum-based.
				// P1's HasLineOfSight() check is also too inconsistent 
				// when targeting objects with an obstacle sitting between P1 and the camera.

				if (a_showDebugInfo)
				{
					SPDLOG_DEBUG("[Util] HasLOS: From camera collision pos.");
				}
				
				// First from the camera's node position.
				/*auto camCollisionNodePos = 
				(
					glob.cam->IsRunning() ? 
					glob.cam->camCollisionTargetPos : 
					playerCam->cameraRoot->world.translate
				);
				hasLOS = HasRaycastLOSFromPos
				(
					camCollisionNodePos, 
					a_targetRefr, 
					excluded3DObjects, 
					a_checkCrosshairPos, 
					a_crosshairWorldPos,
					a_showDebugInfo
				);
				*/

				// First, from the camera's node position or the collision point
				// from the observer's LOS start position to the current camera target position.
				RE::NiPoint3 losCamTargetPos = playerCam->cameraRoot->world.translate;
				auto result = Raycast::hkpCastRay
				(
					ToVec4(observerLOSStartPos),
					ToVec4(playerCam->cameraRoot->world.translate),
					true
				);
				if (result.hit)
				{
					losCamTargetPos = ToNiPoint3
					(
						result.hitPos +
						result.rayNormal *
						min(result.rayLength, glob.cam->camTargetPosHullSize)
					);
				}
				hasLOS = HasRaycastLOSFromPos
				(
					losCamTargetPos, 
					a_targetRefr, 
					excluded3DObjects, 
					a_checkCrosshairPos, 
					a_crosshairWorldPos,
					a_showDebugInfo
				);
				// Then check from the observer's focus point.
				if (!hasLOS && observer3DPtr)
				{
					if (a_showDebugInfo)
					{
						SPDLOG_DEBUG("[Util] HasLOS: From observer focus pos.");
					}

					hasLOS = HasRaycastLOSFromPos
					(
						observerLOSStartPos, 
						a_targetRefr, 
						excluded3DObjects,
						a_checkCrosshairPos, 
						a_crosshairWorldPos,
						a_showDebugInfo
					);
				}
			}
			
			// As a final resort, cast along observer's vertical axis, 
			// bound above and below to remain in traversable space.
			if (!hasLOS && observer3DPtr)
			{
				hasLOS = HasRaycastLOSAlongObserverAxis
				(
					a_observer, 
					a_targetRefr, 
					excluded3DObjects, 
					a_checkCrosshairPos, 
					a_crosshairWorldPos,
					a_showDebugInfo
				);
			}

			if (a_showDebugInfo)
			{
				SPDLOG_DEBUG
				(
					"[Util] HasLOS: {} has LOS on {}: {}.",
					a_observer->GetName(),
					a_targetRefr->GetName(),
					hasLOS
				);
			}

			return hasLOS;
		}
		
		bool HasRaycastLOS
		(
			RE::TESObjectREFR* a_targetRefr,
			RE::Actor* a_observer, 
			std::optional<RE::NiPoint3> a_startPos,
			std::optional<RE::NiPoint3> a_crosshairWorldPos,
			bool a_showDebugInfo
		)
		{
			// WIP:
			// Check for raycast LOS from the observer refr to the target refr 
			// and all its child nodes (expensive, yes).
			// Can cast from the given start point and to the crosshair world position as well.

			bool hasLOS = false;
			const glm::vec4 startPos = 
			(
				a_startPos.has_value() ? 
				ToVec4(a_startPos.value()) : 
				ToVec4(a_observer->GetLookingAtLocation())
			);
			glm::vec4 endPos{ };
			const auto player3DPtr = GetRefr3D(a_observer);
			const auto refrHandle = a_targetRefr->GetHandle();
			const auto excludedObjList = std::vector<RE::NiAVObject*>({ player3DPtr.get() });
			Raycast::RayResult result{ };
			if (a_crosshairWorldPos.has_value())
			{
				endPos = ToVec4(a_crosshairWorldPos.value());
				result = Raycast::hkpCastRay
				(
					startPos, 
					endPos,
					excludedObjList, 
					RE::COL_LAYER::kUnidentified
				);
				hasLOS = 
				(
					(
						!result.hit || 
						result.hitRefrHandle == refrHandle
					) || 
					(
						result.hitObjectPtr &&
						IsRefrAccessibleInside3DObject(a_targetRefr, result.hitObjectPtr.get())
					)
				);
				if (a_showDebugInfo)
				{
					auto hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
					SPDLOG_DEBUG
					(
						"[Util] HasRaycastLOS: "
						"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
						"Raycast to crosshair pos: [{}] ({}, 0x{:X}, type: {:X}).",
						a_targetRefr->GetName(),
						a_targetRefr->formID,
						a_targetRefr->GetBaseObject() ? 
						*a_targetRefr->GetBaseObject()->formType : 
						RE::FormType::None,
						hasLOS,
						result.hit,
						hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
						hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
						hitRefrPtr && hitRefrPtr->GetObjectReference() ?
						*hitRefrPtr->GetObjectReference()->formType :
						RE::FormType::None
					);
					DebugAPI::QueueArrow3D
					(
						startPos,
						endPos,
						0xFFFFFF33,
						10.0f, 2.0f,
						Settings::fSecsBetweenActivationChecks
					);
					if (result.hit)
					{
						DebugAPI::QueuePoint3D
						(
							result.hitPos, 0xFFFFFFFF, 5.0f, Settings::fSecsBetweenActivationChecks
						);
						DebugAPI::QueueArrow3D
						(
							startPos, 
							result.hitPos, 
							0xFFFFFFFF, 
							10.0f, 
							2.0f, 
							Settings::fSecsBetweenActivationChecks
						);
					}
				}
			}

			if (!hasLOS)
			{
				endPos = ToVec4(a_targetRefr->data.location);
				result = Raycast::hkpCastRay
				(
					startPos, 
					endPos,
					excludedObjList, 
					RE::COL_LAYER::kUnidentified
				); 
				hasLOS = 
				(
					(
						!result.hit || 
						result.hitRefrHandle == refrHandle
					) || 
					(
						result.hitObjectPtr &&
						IsRefrAccessibleInside3DObject(a_targetRefr, result.hitObjectPtr.get())
					)
				);
				if (a_showDebugInfo)
				{
					auto hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
					SPDLOG_DEBUG
					(
						"[Util] HasRaycastLOS: "
						"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
						"Raycast to refr pos: [{}] ({}, 0x{:X}, type: {:X}).",
						a_targetRefr->GetName(),
						a_targetRefr->formID,
						a_targetRefr->GetBaseObject() ? 
						*a_targetRefr->GetBaseObject()->formType : 
						RE::FormType::None,
						hasLOS,
						result.hit,
						hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
						hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
						hitRefrPtr && hitRefrPtr->GetObjectReference() ?
						*hitRefrPtr->GetObjectReference()->formType :
						RE::FormType::None
					);
					DebugAPI::QueueArrow3D
					(
						startPos,
						endPos,
						0xFFFF0033,
						10.0f, 2.0f,
						Settings::fSecsBetweenActivationChecks
					);
					if (result.hit)
					{
						DebugAPI::QueuePoint3D
						(
							result.hitPos, 0xFFFFFFFF, 5.0f, Settings::fSecsBetweenActivationChecks
						);
						DebugAPI::QueueArrow3D
						(
							startPos, 
							result.hitPos, 
							0xFFFF00FF, 
							10.0f, 
							2.0f, 
							Settings::fSecsBetweenActivationChecks
						);
					}
				}

				if (!hasLOS)
				{
					auto refr3DPtr = GetRefr3D(a_targetRefr);
					if (refr3DPtr)
					{
						endPos - ToVec4(refr3DPtr->worldBound.center);
						result = Raycast::hkpCastRay
						(
							startPos, 
							endPos,
							excludedObjList, 
							RE::COL_LAYER::kUnidentified
						); 
						hasLOS = 
						(
							(
								!result.hit || 
								result.hitRefrHandle == refrHandle
							) || 
							(
								result.hitObjectPtr && 
								IsRefrAccessibleInside3DObject
								(
									a_targetRefr, result.hitObjectPtr.get()
								)
							)
						);
					}
					if (a_showDebugInfo)
					{
						auto hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
						SPDLOG_DEBUG
						(
							"[Util] HasRaycastLOS: "
							"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
							"Raycast to refr center pos: [{}] ({}, 0x{:X}, type: {:X}).",
							a_targetRefr->GetName(),
							a_targetRefr->formID,
							a_targetRefr->GetBaseObject() ? 
							*a_targetRefr->GetBaseObject()->formType : 
							RE::FormType::None,
							hasLOS,
							result.hit,
							hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
							hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
							hitRefrPtr && hitRefrPtr->GetObjectReference() ?
							*hitRefrPtr->GetObjectReference()->formType :
							RE::FormType::None
						);
						DebugAPI::QueueArrow3D
						(
							startPos,
							endPos,
							0x00FFFF33,
							10.0f, 2.0f,
							Settings::fSecsBetweenActivationChecks
						);
						if (result.hit)
						{
							DebugAPI::QueuePoint3D
							(
								result.hitPos, 
								0xFFFFFFFF,
								5.0f, 
								Settings::fSecsBetweenActivationChecks
							);
							DebugAPI::QueueArrow3D
							(
								startPos, 
								result.hitPos, 
								0x00FFFFFF, 
								10.0f, 
								2.0f, 
								Settings::fSecsBetweenActivationChecks
							);
						}
					}

					if (!hasLOS)
					{
						RE::BSVisit::TraverseScenegraphObjects
						(
							refr3DPtr.get(),
							[
								a_observer,
								a_targetRefr,
								a_showDebugInfo,
								&startPos,
								&endPos,
								&excludedObjList,
								&refrHandle,
								&result,
								&hasLOS
							]
							(RE::NiAVObject* a_node)
							{
								if (!a_node)
								{
									return RE::BSVisit::BSVisitControl::kContinue;
								}

								endPos = ToVec4(a_node->world.translate);
								result = Raycast::hkpCastRay
								(
									startPos, 
									endPos,
									excludedObjList, 
									RE::COL_LAYER::kUnidentified
								); 
								hasLOS = 
								(
									(
										!result.hit || 
										result.hitRefrHandle == refrHandle
									) || 
									(
										result.hitObjectPtr && 
										IsRefrAccessibleInside3DObject
										(
											a_targetRefr, result.hitObjectPtr.get()
										)
									)
								);
								if (hasLOS)
								{
									return RE::BSVisit::BSVisitControl::kStop;
								}

								if (a_showDebugInfo)
								{
									auto hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
									SPDLOG_DEBUG
									(
										"[Util] HasRaycastLOS: "
										"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
										"Raycast to {} node: [{}] ({}, 0x{:X}, type: {:X}).",
										a_targetRefr->GetName(),
										a_targetRefr->formID,
										a_targetRefr->GetBaseObject() ? 
										*a_targetRefr->GetBaseObject()->formType : 
										RE::FormType::None,
										hasLOS,
										a_node->name,
										result.hit,
										hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
										hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
										hitRefrPtr && hitRefrPtr->GetObjectReference() ?
										*hitRefrPtr->GetObjectReference()->formType :
										RE::FormType::None
									);
									DebugAPI::QueueArrow3D
									(
										startPos,
										endPos,
										0xFF00FF33,
										10.0f, 2.0f,
										Settings::fSecsBetweenActivationChecks
									);
									if (result.hit)
									{
										DebugAPI::QueuePoint3D
										(
											result.hitPos,
											0xFFFF00FF, 
											5.0f,
											Settings::fSecsBetweenActivationChecks
										);
										DebugAPI::QueueArrow3D
										(
											startPos, 
											result.hitPos, 
											0xFF00FFFF, 
											10.0f, 
											2.0f, 
											Settings::fSecsBetweenActivationChecks
										);
									}
								}
					
								return RE::BSVisit::BSVisitControl::kContinue;
							}
						);
					}
				}
			}

			return hasLOS;
		}

		bool HasRaycastLOSAlongObserverAxis
		(
			RE::Actor* a_observer,
			RE::TESObjectREFR* a_targetRefr, 
			const std::vector<RE::NiAVObject*>& a_excluded3DObjects,
			bool a_checkCrosshairPos, 
			const RE::NiPoint3& a_crosshairWorldPos,
			bool a_showDebugDraws
		)
		{
			// Perform raycasts along a segment of the observer's vertical axis, 
			// which is bound above and below to keep the casts within traversable space.
			// If any raycast hits the target refr, the target refr is 'visible' to the observer.
			// Can also draw debug lines for the raycasts.

			if (!a_observer || !a_targetRefr)
			{
				return false;
			}

			// Max number of raycasts to perform without hitting the target.
			const uint8_t numCasts = 6;
			// Use the looking at position as the observer's eye position 
			// and the raycasts' divider point.
			RE::NiPoint3 lookingAtLoc = GetActorFocusPoint(a_observer->As<RE::Actor>());
			// Keep slightly offset from the bounds.
			auto bounds = GetVertCollPoints(lookingAtLoc, 10.0f);
			// Cap bounds' maximum offsets from the looking at position if they are unbound.
			auto niCamPtr = GetNiCamera();
			bool unboundedUp = bounds.first == FLT_MAX;
			bool unboundedDown = bounds.second == -FLT_MAX;
			if (unboundedUp || unboundedDown)
			{
				// Set the bound Z coordinate to the worldspace Z coordinate for the position 
				// that corresponds to the edge-of-screen position 
				// directly above or below the player.
				// Along the top (if unbound above) or bottom (if unbound below) of the screen.
				if (niCamPtr)
				{					
					RE::NiPoint3 origin{ };
					RE::NiPoint3 dir{ };
					if (unboundedUp)
					{
						niCamPtr->WindowPointToRay
						(
							WorldToScreenPoint3(lookingAtLoc).x, 
							0.0f, 
							origin,
							dir, 
							DebugAPI::screenResX,
							DebugAPI::screenResY
						);
						float rayPitch = -GetPitchBetweenPositions
						(
							RE::NiPoint3(), dir
						);
						float xyOriginToLookingAt = GetXYDistance(origin, lookingAtLoc);
						RE::NiPoint3 topScreenPosNearPlayer = 
						(
							lookingAtLoc + 
							RE::NiPoint3
							(
								0.0f, 
								0.0f, 
								xyOriginToLookingAt * tanf(rayPitch) + (origin.z - lookingAtLoc.z)
							)
						);
						// Ensure the upper bound is above the looking at position.
						bounds.first = 
						(
							lookingAtLoc.z + 
							max
							(
								0.25f * a_observer->GetHeight(), 
								(topScreenPosNearPlayer.z - lookingAtLoc.z)
							)
						);
					}

					if (unboundedDown)
					{
						niCamPtr->WindowPointToRay
						(
							WorldToScreenPoint3(lookingAtLoc).x, 
							DebugAPI::screenResY, 
							origin,
							dir, 
							DebugAPI::screenResX,
							DebugAPI::screenResY
						);
						float rayPitch = -GetPitchBetweenPositions
						(
							RE::NiPoint3(), dir
						);
						float xyOriginToLookingAt = GetXYDistance(origin, lookingAtLoc);
						RE::NiPoint3 bottomScreenPosNearPlayer = 
						(
							lookingAtLoc + 
							RE::NiPoint3
							(
								0.0f, 
								0.0f, 
								xyOriginToLookingAt * tanf(rayPitch) + (origin.z - lookingAtLoc.z)
							)
						);
						// Ensure the lower bound is below the looking at position.
						bounds.second = 
						(
							lookingAtLoc.z - 
							max
							(
								0.25f * a_observer->GetHeight(), 
								(lookingAtLoc.z - bottomScreenPosNearPlayer.z)
							)
						);
					}
				}
				
				// Just in case my calcs go haywire, one more validation check.
				unboundedUp = 
				(
					bounds.first == FLT_MAX || isnan(bounds.first) || isinf(bounds.first)
				);
				if (unboundedUp)
				{
					bounds.first = lookingAtLoc.z + a_observer->GetHeight() * 0.25f;
				}
				
				unboundedDown = 
				(
					bounds.second == -FLT_MAX || isnan(bounds.second) || isinf(bounds.second)
				);
				if (unboundedDown)
				{
					bounds.second = lookingAtLoc.z - a_observer->GetHeight() * 1.25f;
				}
			}
			
			Raycast::RayResult result{ };
			glm::vec4 endPos = ToVec4(a_targetRefr->data.location);
			// If requested, use the crosshair position as the raycast target point.
			if (a_checkCrosshairPos)
			{
				endPos = ToVec4(a_crosshairWorldPos);
			}

			// Break up bounds interval into two sections:
			// 1. Eye pos to upper bound.
			// 2. Eye pos to lower bound.

			// Starting z coordinate increment between casts.
			float zInc = (bounds.first - lookingAtLoc.z) / static_cast<float>(numCasts / 2);
			glm::vec4 startPos = ToVec4(lookingAtLoc);
			// Upper bound is below the observer's eye level, 
			// which is an error, so return false.
			if (zInc <= 0.0f)
			{
				return false;
			}

			// Eye position to the upper bound.
			for (auto i = 0; i < numCasts / 2; ++i)
			{
				// Extend ray through the target position.
				endPos = startPos + (endPos - startPos) * 2.0f;
				result = Raycast::hkpCastRay
				(
					startPos, 
					endPos, 
					a_excluded3DObjects, 
					RE::COL_LAYER::kUnidentified, 
					false
				);
				auto hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
				if (a_showDebugDraws)
				{
					SPDLOG_DEBUG
					(
						"[Util] HasRaycastLOSAlongObserverAxis: "
						"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
						"Raycast along upper observer axis: [{}] ({}, 0x{:X}, type: {:X}).",
						a_targetRefr->GetName(),
						a_targetRefr->formID,
						a_targetRefr->GetBaseObject() ? 
						*a_targetRefr->GetBaseObject()->formType : 
						RE::FormType::None,
						result.hit,
						result.hit,
						hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
						hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
						hitRefrPtr && hitRefrPtr->GetObjectReference() ? 
						*hitRefrPtr->GetObjectReference()->formType : 
						RE::FormType::None
					);
					DebugAPI::QueueArrow3D
					(
						startPos,
						endPos, 
						0x00FF0033, 
						10.0f, 
						2.0f,
						Settings::fSecsBetweenActivationChecks
					);
					if (result.hit)
					{
						DebugAPI::QueuePoint3D
						(
							result.hitPos, 0x00FF00FF, 5.0f, Settings::fSecsBetweenActivationChecks
						);
						DebugAPI::QueueArrow3D
						(
							startPos, 
							result.hitPos, 
							0x00FF00FF, 
							10.0f, 
							2.0f, 
							Settings::fSecsBetweenActivationChecks
						);
					}
				}
				
				// Hit the target = has LOS.
				if (result.hit)
				{
					auto hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
					if ((hitRefrPtr && hitRefrPtr.get() == a_targetRefr) ||
						(
							result.hitObjectPtr && 
							IsRefrAccessibleInside3DObject(a_targetRefr, result.hitObjectPtr.get())
						))
					{
						return true;
					}
				}

				// Move up.
				startPos.z += zInc;
			}

			// Minus one since we already raycast from the looking at position.
			zInc = (lookingAtLoc.z - bounds.second) / (numCasts / 2 - 1);
			// Lower bound is above the observer's eye level,
			// which is an error, so return false.
			if (zInc <= 0.0f)
			{
				return false;
			}

			startPos = ToVec4(lookingAtLoc);
			// Don't need to cast from the looking at position again.
			startPos.z -= zInc;
			// One Z increment below the eye position to the lower bound.
			for (auto i = 0; i < numCasts / 2 - 1; ++i) 
			{
				// Extend ray through the target position.
				endPos = startPos + (endPos - startPos) * 2.0f;
				result = Raycast::hkpCastRay
				(
					startPos,
					endPos,
					a_excluded3DObjects,
					RE::COL_LAYER::kUnidentified,
					false
				);
				auto hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
				if (a_showDebugDraws)
				{
					SPDLOG_DEBUG
					(
						"[Util] HasRaycastLOSAlongObserverAxis: "
						"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
						"Raycast along lower observer axis: [{}] ({}, 0x{:X}, type: {:X}).",
						a_targetRefr->GetName(),
						a_targetRefr->formID,
						a_targetRefr->GetBaseObject() ? 
						*a_targetRefr->GetBaseObject()->formType :
						RE::FormType::None,
						result.hit,
						result.hit,
						hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
						hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
						hitRefrPtr && hitRefrPtr->GetObjectReference() ? 
						*hitRefrPtr->GetObjectReference()->formType : 
						RE::FormType::None
					);
					DebugAPI::QueueArrow3D
					(
						startPos, 
						endPos, 
						0xFF000033, 
						10.0f,
						2.0f, 
						Settings::fSecsBetweenActivationChecks
					);
					if (result.hit)
					{
						DebugAPI::QueuePoint3D
						(
							result.hitPos, 0xFF0000FF, 5.0f, Settings::fSecsBetweenActivationChecks
						);
						DebugAPI::QueueArrow3D
						(
							startPos, 
							result.hitPos, 
							0xFF0000FF,
							10.0f, 
							2.0f, 
							Settings::fSecsBetweenActivationChecks
						);
					}
				}
				
				// Hit the target = has LOS.
				if (result.hit)
				{
					if ((hitRefrPtr && hitRefrPtr.get() == a_targetRefr) ||
						(
							result.hitObjectPtr && 
							IsRefrAccessibleInside3DObject(a_targetRefr, result.hitObjectPtr.get())
						))
					{
						return true;
					}
				}

				// Move down.
				startPos.z -= zInc;
			}

			// No hits, no LOS.
			return false;
		}

		bool HasRaycastLOSFromPos
		(
			const RE::NiPoint3& a_startPos, 
			RE::TESObjectREFR* a_targetRefr,
			const std::vector<RE::NiAVObject*>& a_excluded3DObjects,
			bool a_checkCrosshairPos, 
			const RE::NiPoint3& a_crosshairWorldPos,
			bool a_showDebugInfo
		)
		{
			// Checks for raycast 'LOS' by casting from the start position
			// to up to 4 different reported positions for the target refr.

			// Invalid refr, so nope.
			if (!a_targetRefr)
			{
				return false;
			}

			auto niCamPtr = GetNiCamera(); 
			// No NiCamera, so nope.
			if (!niCamPtr)
			{
				return false;
			}

			auto refr3DPtr = GetRefr3D(a_targetRefr); 
			// No refr 3D, so nope.
			if (!refr3DPtr)
			{
				return false;
			}

			bool hasLOS = false;
			glm::vec4 startPos = ToVec4(a_startPos);
			std::vector<RE::FormType> filteredOutTypes{ };
			// Filter out activators if the targeted refr is not an activator.
			if ((!a_targetRefr->As<RE::TESObjectACTI>()) && 
				(!a_targetRefr->GetBaseObject() || 
				!a_targetRefr->GetBaseObject()->As<RE::TESObjectACTI>()))
			{
				filteredOutTypes.emplace_back(RE::FormType::Activator);
			}
						
			RE::TESObjectREFRPtr hitRefrPtr{ nullptr };
			Raycast::RayResult result{ };
			
			//
			// Cast to crosshair position first, if requested.
			//
			
			if (a_checkCrosshairPos)
			{
				glm::vec4 refrPos = ToVec4(a_crosshairWorldPos);
				// Cast through the target position.
				auto dirToPos = 2.0f * (refrPos - startPos);
				refrPos = startPos + dirToPos;
				result = Raycast::hkpCastRay
				(
					startPos, refrPos, a_excluded3DObjects, filteredOutTypes
				);
				hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
				// Check if the target was hit.
				hasLOS = 
				(
					(
						hitRefrPtr && hitRefrPtr.get() == a_targetRefr
					) || 
					(
						result.hitObjectPtr && 
						IsRefrAccessibleInside3DObject(a_targetRefr, result.hitObjectPtr.get())
					)
				);
				if (a_showDebugInfo)
				{
					SPDLOG_DEBUG
					(
						"[Util] HasRaycastLOSFromPos: "
						"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
						"Raycast to crosshair pos: [{}] ({}, 0x{:X}, type: {:X}).",
						a_targetRefr->GetName(),
						a_targetRefr->formID,
						a_targetRefr->GetBaseObject() ? 
						*a_targetRefr->GetBaseObject()->formType : 
						RE::FormType::None,
						hasLOS,
						result.hit,
						hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
						hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
						hitRefrPtr && hitRefrPtr->GetObjectReference() ?
						*hitRefrPtr->GetObjectReference()->formType :
						RE::FormType::None
					);
					DebugAPI::QueueArrow3D
					(
						startPos,
						refrPos,
						0xFFFFFF33,
						10.0f, 2.0f,
						Settings::fSecsBetweenActivationChecks
					);
					if (result.hit)
					{
						DebugAPI::QueuePoint3D
						(
							result.hitPos, 0xFFFFFFFF, 5.0f, Settings::fSecsBetweenActivationChecks
						);
						DebugAPI::QueueArrow3D
						(
							startPos, 
							result.hitPos, 
							0xFFFFFFFF, 
							10.0f, 
							2.0f, 
							Settings::fSecsBetweenActivationChecks
						);
					}
				}

				if (hasLOS)
				{
					return true;
				}
			}

			// Now, raycast to three different object positions
			// if there was no requested crosshair position raycast
			// or no hit target.

			//
			// Refr data location position.
			//
			
			auto refrPos1 = ToVec4(a_targetRefr->data.location);
			// Cast through the target position.
			auto dirToPos1 = 2.0f * (refrPos1 - startPos);
			refrPos1 = startPos + dirToPos1;
			result = Raycast::hkpCastRay
			(
				startPos, refrPos1, a_excluded3DObjects, filteredOutTypes
			);
			hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
			// Check if the target was hit.
			hasLOS = 
			(
				(
					hitRefrPtr && hitRefrPtr.get() == a_targetRefr
				) || 
				(
					result.hitObjectPtr && 
					IsRefrAccessibleInside3DObject(a_targetRefr, result.hitObjectPtr.get())
				)
			);
			if (a_showDebugInfo)
			{
				SPDLOG_DEBUG
				(
					"[Util] HasRaycastLOSFromPos: "
					"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
					"Raycast to data location pos: [{}] ({}, 0x{:X}, type: {:X}).",
					a_targetRefr->GetName(),
					a_targetRefr->formID,
					a_targetRefr->GetBaseObject() ? 
					*a_targetRefr->GetBaseObject()->formType : 
					RE::FormType::None,
					hasLOS,
					result.hit,
					hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
					hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
					hitRefrPtr && hitRefrPtr->GetObjectReference() ? 
					*hitRefrPtr->GetObjectReference()->formType : 
					RE::FormType::None
				);
				DebugAPI::QueueArrow3D
				(
					startPos, 
					refrPos1,
					0xFFFF0033, 
					10.0f, 
					2.0f, 
					Settings::fSecsBetweenActivationChecks
				);
				if (result.hit)
				{
					DebugAPI::QueuePoint3D
					(
						result.hitPos, 0xFFFF00FF, 5.0f, Settings::fSecsBetweenActivationChecks
					);
					DebugAPI::QueueArrow3D
					(
						startPos, 
						result.hitPos, 
						0xFFFF00FF,
						10.0f, 
						2.0f, 
						Settings::fSecsBetweenActivationChecks
					);
				}
			}

			if (hasLOS)
			{
				return true;
			}

			//
			// Next, the refr 3D's world position.
			//

			auto refrPos2 = ToVec4(refr3DPtr->world.translate);
			// Cast through the target position.
			auto dirToPos2 = 2.0f * (refrPos2 - startPos);
			refrPos2 = startPos + dirToPos2;
			result = Raycast::hkpCastRay
			(
				startPos, refrPos2, a_excluded3DObjects, filteredOutTypes
			);
			hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
			// Check if the target was hit.
			hasLOS = 
			(
				(
					hitRefrPtr && hitRefrPtr.get() == a_targetRefr
				) || 
				(
					result.hitObjectPtr && 
					IsRefrAccessibleInside3DObject(a_targetRefr, result.hitObjectPtr.get())
				)
			);
			if (a_showDebugInfo)
			{
				SPDLOG_DEBUG
				(
					"[Util] HasRaycastLOSFromPos: "
					"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
					"Raycast to world translate pos: [{}] ({}, 0x{:X}, type: {:X}).",
					a_targetRefr->GetName(),
					a_targetRefr->formID,
					a_targetRefr->GetBaseObject() ? 
					*a_targetRefr->GetBaseObject()->formType : 
					RE::FormType::None,
					hasLOS,
					result.hit,
					hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
					hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
					hitRefrPtr && hitRefrPtr->GetObjectReference() ? 
					*hitRefrPtr->GetObjectReference()->formType : 
					RE::FormType::None
				);
				DebugAPI::QueueArrow3D
				(
					startPos, 
					refrPos2, 
					0xFF00FF33, 
					10.0f, 
					2.0f, 
					Settings::fSecsBetweenActivationChecks
				);
				if (result.hit)
				{
					DebugAPI::QueuePoint3D
					(
						result.hitPos, 0xFF00FFFF, 5.0f, Settings::fSecsBetweenActivationChecks
					);
					DebugAPI::QueueArrow3D
					(
						startPos, 
						result.hitPos, 
						0xFF00FFFF, 
						10.0f,
						2.0f, 
						Settings::fSecsBetweenActivationChecks
					);
				}
			}

			if (hasLOS)
			{
				return true;
			}

			//
			// Lastly, the refr 3D's world bound center position.
			//

			auto refrPos3 = ToVec4(refr3DPtr->worldBound.center);
			// Cast through the target position.
			auto dirToPos3 = 2.0f * (refrPos3 - startPos);
			refrPos3 = startPos + dirToPos3;
			result = Raycast::hkpCastRay
			(
				startPos, refrPos3, a_excluded3DObjects, filteredOutTypes
			);
			hitRefrPtr = GetRefrPtrFromHandle(result.hitRefrHandle);
			// Check if the target was hit.
			hasLOS = 
			(
				(
					hitRefrPtr && hitRefrPtr.get() == a_targetRefr
				) || 
				(
					result.hitObjectPtr && 
					IsRefrAccessibleInside3DObject(a_targetRefr, result.hitObjectPtr.get())
				)
			);
			if (a_showDebugInfo)
			{
				SPDLOG_DEBUG
				(
					"[Util] HasRaycastLOSFromPos: "
					"A player HasLOS of {} (0x{:X}, type {:X}): [{}]. "
					"Raycast to 3D center pos: [{}] ({}, 0x{:X}, type: {:X}).",
					a_targetRefr->GetName(),
					a_targetRefr->formID,
					a_targetRefr->GetBaseObject() ? 
					*a_targetRefr->GetBaseObject()->formType : 
					RE::FormType::None,
					hasLOS,
					result.hit,
					hitRefrPtr ? hitRefrPtr->GetName() : "NONE",
					hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
					hitRefrPtr && hitRefrPtr->GetObjectReference() ? 
					*hitRefrPtr->GetObjectReference()->formType : 
					RE::FormType::None
				);
				DebugAPI::QueueArrow3D
				(
					startPos, 
					refrPos3,
					0x00FFFF33, 
					10.0f, 
					2.0f, 
					Settings::fSecsBetweenActivationChecks
				);
				if (result.hit)
				{
					DebugAPI::QueuePoint3D
					(
						result.hitPos, 0x00FFFFFF, 5.0f, Settings::fSecsBetweenActivationChecks
					);
					DebugAPI::QueueArrow3D
					(
						startPos, 
						result.hitPos, 
						0x00FFFFFF, 
						10.0f, 
						2.0f, 
						Settings::fSecsBetweenActivationChecks
					);
				}
			}

			return hasLOS;
		}

		float InterpolateEaseIn
		(
			const float& a_prev, const float& a_next, float a_ratio, const float& a_pow
		)
		{
			// Ease in from the previous value to the next value using the interpolation ratio.
			// Diverges from the previous endpoint slower, 
			// but converges to the next endpoint faster at higher powers
			// when comparing values computed from the same interpolation ratio and endpoints.

			a_ratio = std::clamp(a_ratio, 0.0f, 1.0f);
			float powUpper = ceilf(a_pow);
			float powLower = floorf(a_pow);
			float upperPowerCoeff = fmodf(a_pow, powLower);
			float lowerPowCoeff = 1.0f - upperPowerCoeff;
			float newTRatio = 
			(
				lowerPowCoeff * powf(a_ratio, powLower) + 
				upperPowerCoeff * powf(a_ratio, powUpper)
			);

			return std::lerp(a_prev, a_next, newTRatio);
		}

		float InterpolateEaseOut
		(
			const float& a_prev, const float& a_next, float a_ratio, const float& a_pow
		)
		{
			// Ease out from the previous value to the next value using the interpolation ratio.
			// Diverges from the previous endpoint faster, 
			// but converges to the next endpoint slower at higher powers
			// when comparing values computed from the same interpolation ratio and endpoints.

			a_ratio = std::clamp(a_ratio, 0.0f, 1.0f);
			float powUpper = ceilf(a_pow);
			float powLower = floorf(a_pow);
			float upperPowerCoeff = fmodf(a_pow, powLower);
			float lowerPowCoeff = 1.0f - upperPowerCoeff;
			float newTRatio = 
			(
				1.0f - 
				(
					lowerPowCoeff * powf(1.0f - a_ratio, powLower) + 
					upperPowerCoeff * powf(1.0f - a_ratio, powUpper)
				)
			);

			return std::lerp(a_prev, a_next, newTRatio);
		}

		float InterpolateEaseInEaseOut
		(
			const float& a_prev, const float& a_next, float a_ratio, const float& a_pow
		)
		{
			// Ease in from the previous value then ease out to the next value
			// using the interpolation ratio.
			// Raising the interpolation power increases the steepness of the interpolation curve.

			a_ratio = std::clamp(a_ratio, 0.0f, 1.0f);
			float powUpper = ceilf(a_pow);
			float powLower = floorf(a_pow);
			float upperPowerCoeff = fmodf(a_pow, powLower);
			float lowerPowCoeff = 1.0f - upperPowerCoeff;
			float easeInT = 
			(
				0.5f * 
				(
					lowerPowCoeff * powf(2.0f * a_ratio, powLower) + 
					upperPowerCoeff * powf(2.0f * a_ratio, powUpper)
				)
			);
			float easeOutT = 
			(
				0.5f * 
				(
					1.0f - 
					(
						lowerPowCoeff * powf(2.0f * (1.0f - a_ratio), powLower) + 
						upperPowerCoeff * powf(2.0f * (1.0f - a_ratio), powUpper)
					)
				) + 
				0.5f
			);
			float newTRatio = (a_ratio < 0.5f) ? easeInT : easeOutT;

			return std::lerp(a_prev, a_next, newTRatio);
		}

		RE::NiMatrix3 InterpolateRotMatrix
		(
			const RE::NiMatrix3& a_matA, const RE::NiMatrix3& a_matB, const float& a_ratio
		)
		{
			// Interpolate between matrices A and B using the given ratio 
			// and return the resulting matrix.
			// Must convert matrices to quaternions before interpolating.

			RE::NiQuaternion qA{ };
			RE::NiQuaternion qB{ };
			// Credits to ersh1:
			// https://github.com/ersh1/Precision/blob/main/src/Offsets.h#L98
			NativeFunctions::NiMatrixToNiQuaternion(qA, a_matA);
			NativeFunctions::NiMatrixToNiQuaternion(qB, a_matB);
			return QuaternionToRotationMatrix(QuaternionSlerp(qA, qB, a_ratio));
		}

		bool IsFavorited(RE::Actor* a_actor, RE::TESForm* a_form)
		{
			// Check if the form is favorited by the actor.
			// Return true if favorited, false if not.

			if (!a_actor || !a_form)
			{
				return false;
			}

			if (a_form->Is(RE::FormType::Spell, RE::FormType::Shout)) 
			{
				auto magicFavorites = RE::MagicFavorites::GetSingleton();
				if (!magicFavorites)
				{
					return false;
				}

				for (auto magForm : magicFavorites->spells)
				{
					if (magForm == a_form)
					{
						return true;
					}
				}
			}
			else
			{
				auto inventory = a_actor->GetInventory();
				// Iterate through the actor's inventory entries.
				for (auto& inventoryEntry : inventory)
				{
					// Not the form.
					if (!inventoryEntry.first || inventoryEntry.first != a_form)
					{
						continue;
					}

					// Look for the hotkey extra data type
					// in the inventory entry's extra data lists.
					const auto& ied = inventoryEntry.second.second; 
					if (!ied)
					{
						continue;
					}

					auto extraLists = ied->extraLists; 
					if (!extraLists)
					{
						continue;
					}

					for (auto& exData : *extraLists)
					{
						// Is favorited since the hotkey extra data exists.
						if (exData->HasType(RE::ExtraDataType::kHotkey))
						{
							return true;
						}
					}
				}
			}

			return false;
		}
		
		bool IsHotkeyed(RE::Actor* a_actor, RE::TESForm* a_form)
		{
			// Check if the given form is hotkeyed for the given player.

			if (!a_actor || !a_form)
			{
				return false;
			}

			if (a_form->Is(RE::FormType::Spell, RE::FormType::Shout))
			{
				auto magicFavorites = RE::MagicFavorites::GetSingleton();
				if (!magicFavorites)
				{
					return false;
				}

				for (auto magForm : magicFavorites->hotkeys)
				{
					if (magForm == a_form)
					{
						return true;
					}
				}
			}
			else
			{
				auto inventory = a_actor->GetInventory();
				// Iterate through the actor's inventory entries.
				for (auto& inventoryEntry : inventory)
				{
					// Not the form.
					if (!inventoryEntry.first || inventoryEntry.first != a_form)
					{
						continue;
					}
					
					// Look for the hotkey extra data type
					// in the inventory entry's extra data lists.
					const auto& ied = inventoryEntry.second.second;
					if (!ied)
					{
						continue;
					}

					auto extraLists = ied->extraLists;
					if (!extraLists)
					{
						continue;
					}

					for (auto& exData : *extraLists)
					{
						// Only consider favorited entries,
						// since only favorited items can be hotkeyed.
						auto exHotkeyData = exData->GetByType<RE::ExtraHotkey>();
						if (!exHotkeyData)
						{
							continue;
						}

						// Lastly, determine if there is a bound hotkey slot.
						return 
						(
							(int8_t)(*exHotkeyData->hotkey) != 
							(int8_t)(RE::ExtraHotkey::Hotkey::kUnbound)
						);
					}
				}
			}

			return false;
		}

		bool IsInFrontOfCam(const RE::NiPoint3& a_point)
		{
			// Check if the point is in front of the current camera position.

			auto playerCam = RE::PlayerCamera::GetSingleton(); 
			// Camera is invalid.
			if (!playerCam || !playerCam->cameraRoot)
			{
				return false;
			}

			// Camera node position.
			auto camNodePos = playerCam->cameraRoot->world.translate;
			// Camera forward direction.
			auto camDir = RotationToDirectionVect
			(
				-glob.cam->GetCurrentPitch(), 
				ConvertAngle(glob.cam->GetCurrentYaw())
			);
			auto camToPointDir = a_point - camNodePos;
			camToPointDir.Unitize();
				
			// Less than 90 degrees away (what I'm using as the vertical FOV)
			// from camera's forward direction.
			return camToPointDir.Dot(camDir) > 0.0f;
		}

		bool IsLootableRefr(RE::TESObjectREFR* a_refr)
		{
			// Check if the refr is lootable.

			// Refr, its 3D, its handle, or base object are invalid,
			// or it is deleted or disabled.
			if (!a_refr || 
				!a_refr->loadedData || 
				a_refr->IsDisabled() || 
				a_refr->IsDeleted() ||
				!a_refr->IsHandleValid() ||
				!a_refr->GetObjectReference())
			{
				return false;
			}

			auto baseObj = a_refr->GetObjectReference();
			// Base object must be of a lootable type,
			// or if a flora object, it must not have been harvested,
			// or if a projectile, it must have become limited 
			// (has impact or damaging collision checks are inactive),
			// or if a light, it must be carryable (a torch).
			return 
			(
				baseObj->Is
				(
					RE::FormType::AlchemyItem,
					RE::FormType::Apparatus,
					RE::FormType::ConstructibleObject,
					RE::FormType::Note,
					RE::FormType::Ingredient,
					RE::FormType::Scroll,
					RE::FormType::Ammo,
					RE::FormType::KeyMaster,
					RE::FormType::Armature,
					RE::FormType::Armor,
					RE::FormType::Book,
					RE::FormType::Misc,
					RE::FormType::Weapon,
					RE::FormType::SoulGem,
					RE::FormType::LeveledItem
				) ||
				(
					(baseObj->Is(RE::FormType::Flora, RE::FormType::Tree)) && 
					(a_refr->formFlags & RE::TESObjectREFR::RecordFlags::kHarvested) == 0
				) ||
				(
					a_refr->As<RE::Projectile>() && 
					a_refr->As<RE::Projectile>()->ShouldBeLimited()
				) ||
				(
					baseObj->As<RE::TESObjectLIGH>() && 
					baseObj->As<RE::TESObjectLIGH>()->CanBeCarried()
				)
			);
		}

		bool IsRefrAccessibleInside3DObject(RE::TESObjectREFR* a_refr, RE::NiAVObject* a_object3D)
		{
			// Checks if the given refr's center is within the bound extents 
			// of the given 3D object.
			// Return true if so.
			// Return false otherwise or if the 3D object's associated refr is locked.
			// Do not want to access a refr inside a locked container.
			// NOTE:
			// Meant to handle edge cases where certain fully-visible refrs, such as doors,
			// are within the bounds of other refrs that are not interactible and break LOS, 
			// such as door frames (static).
			// Example:
			// Trying to target the interior door leading outside from the Whiterun Stables
			// will fail an LOS check since the raycast hits the door frame surrounding the door.
			// And thus the door is not crosshair-selectable or activatable.

			if (!a_refr || !a_object3D)
			{
				return false;
			}
			
			auto object3DRefr = a_object3D->userData;
			// Should not be accessible if within a locked object, so return early.
			if (object3DRefr && object3DRefr->IsLocked())
			{
				return false;
			}

			// NOTE:
			// Currently only supporting detection of TESObjectDOOR refrs within
			// TESObjectSTAT refrs with 'door' in their names.
			// Good enough to support selection of most doors that weren't targetable before.
			// Opening doors without a hitch is especially important
			// when attempting to flee the scene after pilfering a sweetroll or two.
			// 
			// Also do not want to allow targeting of objects through the walls of
			// 'outdoor enclosures', which are composed of static objects 
			// and have targetable objects within them.
			// Was hoping such static objects had child nodes that better conform to their geometry
			// and would allow for individual boundary checks on each child node
			// before determining if the given refr is visible from within the object.
			
			auto baseObj = a_refr->GetBaseObject();
			bool searchForDoorWithinDoorframe = baseObj->Is(RE::FormType::Door);
			if (searchForDoorWithinDoorframe)
			{
				std::string lowercaseName = a_object3D->name.data(); 
				ToLowercase(lowercaseName);
				searchForDoorWithinDoorframe = lowercaseName.contains("door");
			}

			if (!searchForDoorWithinDoorframe)
			{
				return false;
			}

			// Check if refr center is ithin the bound extents of the 3D object.
			auto refr3DPtr = GetRefr3D(a_refr);
			// Grab the center position from the world bound
			// or the refr data position if no 3D is available.
			auto refr3DCenter = 
			(
				refr3DPtr ? refr3DPtr->worldBound.center : a_refr->data.location
			);
			// Fall back to the refr data location offset by half the refr's height.
			if (refr3DCenter.Length() == 0.0f)
			{
				refr3DCenter = 
				(
					a_refr->data.location + 
					RE::NiPoint3(0.0f, 0.0f, 0.5f * a_refr->GetHeight())
				);
			}

			// Fall back to the object refr's data location offset by half the refr's height.
			auto object3DCenter = a_object3D->worldBound.center;
			if (object3DRefr && object3DCenter.Length() == 0.0f)
			{
				object3DCenter = 
				(
					object3DRefr->data.location + 
					RE::NiPoint3(0.0f, 0.0f, 0.5f * object3DRefr->GetHeight())
				);
			}

			// Get extents from the object refr.
			auto object3DHalfExtent = 
			(
				object3DRefr ? 
				0.5f * 
				(object3DRefr->GetBoundMax() - object3DRefr->GetBoundMin()) : 
				RE::NiPoint3()
			);
			// Fall back to the world bound radius and then half the object refr's height.
			if (object3DHalfExtent.Length() == 0.0f)
			{
				object3DHalfExtent = 
				(
					RE::NiPoint3(0.0f, 1.0f, 0.0f) * a_object3D->worldBound.radius
				);
				if (object3DRefr && object3DHalfExtent.Length() == 0.0f)
				{
					object3DHalfExtent = 
					(
						RE::NiPoint3(0.0f, 1.0f, 0.0f) * 0.5f * object3DRefr->GetHeight()
					);
				}
			}
			
			return 
			(
				refr3DCenter.x <
				object3DCenter.x + object3DHalfExtent.x &&	
				refr3DCenter.x > 
				object3DCenter.x - object3DHalfExtent.x &&	
				refr3DCenter.y < 
				object3DCenter.y + object3DHalfExtent.y &&	
				refr3DCenter.y > 
				object3DCenter.y - object3DHalfExtent.y &&	
				refr3DCenter.z < 
				object3DCenter.z + object3DHalfExtent.z &&	
				refr3DCenter.z > 
				object3DCenter.z - object3DHalfExtent.z
			);
		}

		bool IsSelectableRefr(RE::TESObjectREFR* a_refr) 
		{
			// Check if the refr is player crosshair-selectable.

			auto baseObj = a_refr->GetBaseObject(); 
			if (!baseObj)
			{
				return false;
			}

			// First, base object must be of a supported type.
			bool isSupportedType = baseObj->Is
			(
				RE::FormType::Activator,
				RE::FormType::ActorCharacter,
				RE::FormType::AlchemyItem,
				RE::FormType::Ammo,
				RE::FormType::Apparatus,
				RE::FormType::Armature,
				RE::FormType::Armor,
				RE::FormType::Book,
				RE::FormType::ConstructibleObject,
				RE::FormType::Container,
				RE::FormType::Door,
				RE::FormType::Flora,
				RE::FormType::Furniture,
				RE::FormType::Ingredient,
				RE::FormType::KeyMaster,
				RE::FormType::LeveledItem,
				RE::FormType::Light,
				RE::FormType::Misc,
				RE::FormType::Note,
				RE::FormType::NPC,
				RE::FormType::Projectile,
				RE::FormType::ProjectileArrow,
				RE::FormType::Ragdoll,
				RE::FormType::Scroll,
				RE::FormType::SoulGem,
				RE::FormType::TalkingActivator,
				RE::FormType::Tree,
				RE::FormType::Weapon
			);

			if (!isSupportedType)
			{
				return false;
			}

			// If the object is a plant, it is selectable only if not already harvested.
			bool ifPlantAndCantHarvest = 
			{
				(a_refr->Is(RE::FormType::Flora, RE::FormType::Tree)) && 
				(a_refr->formFlags & RE::TESObjectREFR::RecordFlags::kHarvested) != 0
			};
			// If the object is a projectile, it is selectable only if it has impacted something 
			// or is no longer active for damaging collisions.
			bool ifProjAndCantPickup = 
			{
				a_refr->As<RE::Projectile>() && 
				!a_refr->As<RE::Projectile>()->ShouldBeLimited()
			};
			// If the object is a light, it is selectable only if the player can pick it up.
			bool ifTorchAndCantPickup = 
			{
				baseObj->As<RE::TESObjectLIGH>() &&
				!baseObj->As<RE::TESObjectLIGH>()->CanBeCarried()
			};
			if (ifPlantAndCantHarvest || ifProjAndCantPickup || ifTorchAndCantPickup) 
			{
				return false;
			}

			// Check for valid activate text next.
			RE::BSString activateText = "";
			baseObj->GetActivateText(a_refr, activateText);
			// Empty here is ok, since activated containers 
			// have their activation text wiped (???) 
			// while the Container/Loot Menu is open.
			
			// Valid if the refr can contain items
			// or if it has a linked teleport door.
			bool hasValidActivationText = 
			{
				a_refr->HasContainer() || a_refr->extraList.GetTeleportLinkedDoor()
			};
			// One last attempt:
			// The activate text must contain characters after its newline character.
			// E.g 'Take\nCoins' is valid, while 'Take\n' is not.
			// No characters after the newline is the default activate text for any base object,
			// and indicates that they are not selectable.
			if (!hasValidActivationText) 
			{
				std::string activateStr{ activateText.c_str() };
				auto firstNewlinePos = activateStr.find_first_of("\n") + 1;
				std::string truncActivateText = "";
				if (firstNewlinePos != std::string::npos && firstNewlinePos < activateStr.size())
				{
					truncActivateText = activateStr.substr(firstNewlinePos);
				}

				// Actors are always selectable, regardless of their activate text.
				hasValidActivationText = !truncActivateText.empty() || a_refr->As<RE::Actor>();
			}

			// Invalid activate text, cannot select.
			if (!hasValidActivationText)
			{
				return false;
			}

			// Ignore DynDOLOD LOD activators, which shouldn't be selectable.
			// TODO: 
			// Have to figure out a more efficient method to check if the refr is a DynDOLOD refr. 
			// Filename comparison is haphazard, especially if the DynDOLOD plugin was renamed.
			// Needs testing.
			bool isActivator = baseObj->Is
			(
				RE::FormType::Activator, RE::FormType::TalkingActivator
			); 
			if (isActivator) 
			{
				bool isLightMod = (a_refr->formID >> 24) == 0xFE;
				auto modIndex = 
				(
					isLightMod ? 
					(a_refr->formID >> 12) & 0xFFF :
					(a_refr->formID >> 24) & 0xFF
				); 
				if (modIndex > 0) 
				{
					if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) 
					{
						auto mod = 
						(
							isLightMod ? 
							dataHandler->LookupLoadedLightModByIndex(modIndex) :
							dataHandler->LookupLoadedModByIndex(modIndex)
						); 
						if (mod && mod->GetFilename().contains("DynDOLOD"))
						{
							return false;
						}
					}
				}
			}

			// Activation text checks passed.
			return true;
		}

		RE::NiMatrix3 MatrixFromAxisAndAngle(RE::NiPoint3 a_axis, const float& a_angle)
		{
			// Construct a rotation matrix given an axis 
			// and an angle to rotate about that axis.

			RE::NiMatrix3 mat{ };
			a_axis.Unitize();
			const float cosT = cosf(a_angle);
			const float sinT = sinf(a_angle);
			const float aX = a_axis.x;
			const float aY = a_axis.y;
			const float aZ = a_axis.z;
			const float omCosT = 1.0f - cosT;
			mat.entry[0][0] = (cosT + aX * aX * omCosT);
			mat.entry[0][1] = (aX * aY * omCosT - aZ * sinT);
			mat.entry[0][2] = (aX * aZ * omCosT + aY * sinT);
			mat.entry[1][0] = (aY * aX * omCosT + aZ * sinT);
			mat.entry[1][1] = (cosT + aY * aY * omCosT);
			mat.entry[1][2] = (aY * aZ * omCosT - aX * sinT);
			mat.entry[2][0] = (aZ * aX * omCosT - aY * sinT);
			mat.entry[2][1] = (aZ * aY * omCosT + aX * sinT);
			mat.entry[2][2] = (cosT + aZ * aZ * omCosT);

			return mat;
		}

		bool MenusOnlyAlwaysOpen() 
		{
			// Return true if a currently-open menu
			// introduces a non-gameplay/TFC context onto the menu context stack
			// or if the QuickLoot menu is open.

			if (ALYSLC::QuickLootCompat::g_quickLootInstalled)
			{
				auto ui = RE::UI::GetSingleton(); 
				if (ui && ui->IsMenuOpen(GlobalCoopData::LOOT_MENU))
				{
					return false;
				}
			}

			if (const auto controlMap = RE::ControlMap::GetSingleton(); controlMap)
			{
				for (const auto& context : controlMap->contextPriorityStack)
				{
					if (context != RE::ControlMap::InputContextID::kGameplay && 
						context != RE::ControlMap::InputContextID::kTFCMode)
					{
						return false;
					}
				}

				return true;
			}

			return true;
		}

		bool MenusOnlyAlwaysUnpaused()
		{
			// Returns true if only 'always open' menus, the Dialogue Menu,
			// or the LootMenu are open.

			auto ui = RE::UI::GetSingleton(); 
			if (!ui)
			{
				return false;
			}

			bool onlyAlwaysOpen = MenusOnlyAlwaysOpen();
			return 
			(
				onlyAlwaysOpen ||
				ui->IsMenuOpen(GlobalCoopData::LOOT_MENU) || 
				ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)
			);
		}

		bool Player1AddPerk(RE::BGSPerk* a_perk, int32_t a_rank)
		{
			// Add the perk to P1.
			// Return true if successful.

			bool succ = false;
			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (!p1)
			{
				return false;
			}

			// Re-add to singleton perk list.
			// Keep everything consistent.
			if (!Player1PerkListHasPerk(a_perk))
			{
				p1->perks.emplace_back(a_perk);
			}
			
			// Add perk to actor base and actor.
			// NOTE:
			// Have to use both funcs since neither consistently
			// removes/adds perks in time on their own before the Stats Menu opens.
			ChangePerk(p1, a_perk, true);
			return p1->HasPerk(a_perk);
		}

		bool Player1PerkListHasPerk(RE::BGSPerk* a_perk)
		{
			// Returns true if the P1 singleton perk list contains the perk.

			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (!p1)
			{
				return false;
			}

			return 
			(
				std::any_of
				(
					p1->perks.begin(), p1->perks.end(), 
					[a_perk](RE::BGSPerk* perk) 
					{ 
						return a_perk == perk; 
					}
				)
			);
		}

		bool Player1RemovePerk(RE::BGSPerk* a_perk)
		{
			// Remove the perk from P1.
			// Return true if successful.

			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (!p1)
			{
				return false;
			}

			bool succ = false;

			// Remove the perk from the P1 singleton perk list.
			// Keep everything consistent.
			if (Player1PerkListHasPerk(a_perk))
			{
				// Construct a list of perks without the removed perk.
				RE::BSTArray<RE::BGSPerk*> newPerksList{ };
				std::for_each
				(
					p1->perks.begin(), p1->perks.end(),
					[&newPerksList, a_perk](RE::BGSPerk* perk) 
					{
						if (perk != a_perk)
						{
							newPerksList.emplace_back(perk);
						}
					}
				);

				// Clear and re-add all other perks.
				p1->perks.clear();
				std::for_each
				(
					newPerksList.begin(), newPerksList.end(),
					[p1](RE::BGSPerk* perk) 
					{
						p1->perks.emplace_back(perk);
					}
				);
			}

			// Remove perk from actor base and actor.
			// NOTE: 
			// Have to use both funcs since neither consistently 
			// removes/adds perks in time on their own before the Stats Menu opens.
			ChangePerk(p1, a_perk, false);
			return !p1->HasPerk(a_perk);
		}

		bool PointIsOnScreen(const RE::NiPoint3& a_point, const float& a_marginPixels)
		{
			// Returns true if the world position is on screen,
			// after adjusting the screen dimensions to account for 
			// a pixel margin along the borders of the screen.

			auto niCamPtr = GetNiCamera();
			const auto hud = DebugAPI::GetHUD(); 
			// Need the NiCamera and Debug Overlay Menu view.
			if (!niCamPtr || !hud || !hud->uiMovie)
			{
				return false;
			}

			RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
			const float rectWidth = fabsf(gRect.right - gRect.left);
			const float rectHeight = fabsf(gRect.bottom - gRect.top);
			RE::NiRect<float> port{ gRect.left, gRect.right, gRect.top, gRect.bottom };
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			RE::NiCamera::WorldPtToScreenPt3
			(
				niCamPtr->worldToCam, port, a_point, x, y, z, 1e-5f
			);

			// Adjust the dimensions of the screen based on the pixel margin,
			// and then check if the screen point falls within these new dimensions.
			return 
			(
				x >= gRect.left + a_marginPixels && 
				x <= gRect.right - a_marginPixels && 
				y >= gRect.top + a_marginPixels && 
				y <= gRect.bottom - a_marginPixels && 
				z < 1.0f && 
				z > -1.0f
			);
		}

		bool PointIsOnScreen
		(
			const RE::NiPoint3& a_point, 
			RE::NiPoint3& a_screenPointOut, 
			float&& a_marginPixels, 
			bool&& a_shouldClamp
		)
		{
			// Returns true if the world position is on screen,
			// after adjusting the screen dimensions to account for
			// a pixel margin along the borders of the screen.
			// Also clamps the screen position on request 
			// so that it lies within these dimensions.
			// Store the screen position, on-screen or not, in the screen point outparam.

			auto niCamPtr = GetNiCamera();
			const auto hud = DebugAPI::GetHUD();
			// Need the NiCamera and Debug Overlay Menu view.
			if (!niCamPtr || !hud || !hud->uiMovie)
			{
				return false;
			}

			RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
			const float rectWidth = fabsf(gRect.right - gRect.left);
			const float rectHeight = fabsf(gRect.bottom - gRect.top);
			RE::NiRect<float> port{ gRect.left, gRect.right, gRect.top, gRect.bottom };
			float x = 0.0;
			float y = 0.0f;
			float z = 0.0f;
			RE::NiCamera::WorldPtToScreenPt3
			(
				niCamPtr->worldToCam, port, a_point, x, y, z, 1e-5f
			);
			if (a_shouldClamp)
			{
				a_screenPointOut.x = std::clamp(x, gRect.left, gRect.right);
				a_screenPointOut.y = std::clamp(y, gRect.top, gRect.bottom);
				a_screenPointOut.z = std::clamp(z, -1.0f, 1.0f);
			}
			else
			{
				a_screenPointOut.x = x;
				a_screenPointOut.y = y;
				a_screenPointOut.z = z;
			}
			
			// Adjust the dimensions of the screen based on the pixel margin,
			// and then check if the screen point falls within these new dimensions.
			return 
			(
				x >= gRect.left + a_marginPixels && 
				x <= gRect.right - a_marginPixels && 
				y >= gRect.top + a_marginPixels && 
				y <= gRect.bottom - a_marginPixels && 
				z < 1.0f && 
				z > -1.0f
			);
		}

		void PushActorAway
		(
			RE::Actor* a_actorToPush, 
			const RE::NiPoint3& a_contactPos, 
			const float& a_force,
			bool&& a_downedRagdoll
		)
		{
			// Apply an actor knock explosion with the provided force at the contact position.
			// If requested, apply a 'downed ragdoll' when the actor's health hits 0.
			// The actor is paralyzed, preventing them from getting up 
			// if they are set to essential.
			// Used to set players as downed if the revive system is enabled.

			if (!a_actorToPush ||
				!a_actorToPush->currentProcess || 
				a_actorToPush->GetKnockState() == RE::KNOCK_STATE_ENUM::kQueued)
			{
				return;
			}

			// [Temp workaround]:
			// Having Precision's ragdoll system enabled 
			// while triggering a knock explosion here 
			// seems to result in more occurrences of a ragdoll reset position glitch 
			// on knock explosion where the hit actor is teleported to their last ragdoll position 
			// instead of staying at their current position.
			// Is a major issue if the last ragdoll position 
			// was far away or in another cell entirely.
			// Precision is re-enabled after the knock explosion.
			if (Settings::bApplyTemporaryRagdollWarpWorkaround)
			{
				if (auto api = ALYSLC::PrecisionCompat::g_precisionAPI4; api)
				{
					api->ToggleDisableActor(a_actorToPush->GetHandle(), true);
				}
			}
			
			// Sometimes, if an actor is in an idle animation when an impulse is applied,
			// they will teleport a short distance to some cached position as well.
			// Stop idling before ragdolling too.
			a_actorToPush->NotifyAnimationGraph("IdleStop");
			a_actorToPush->NotifyAnimationGraph("IdleStopOffset");
			a_actorToPush->currentProcess->StopCurrentIdle(a_actorToPush, true);

			// Sheathe before downing to prevent an equip glitch
			// that requires re-equipping the actor's hand forms
			// once they get up.
			if (a_downedRagdoll)
			{
				RunPlayerActionCommand(RE::DEFAULT_OBJECT::kActionSheath, a_actorToPush);
				// Prevent the downed actor from moving and getting back up.
				a_actorToPush->boolBits.set(RE::Actor::BOOL_BITS::kParalyzed);
				// Prevent the game from downing the actor and entering bleedout
				// once the deferred kill timer reaches 0.
				// NOTE: 
				// May have side effects, so keeping an eye out for any odd behavior 
				// after ragdolling.
				if (a_actorToPush->currentProcess->middleHigh) 
				{
					a_actorToPush->currentProcess->middleHigh->deferredKillTimer = FLT_MAX;
				}
			}

			// Prevents (most of the time?) actors who are interacting with objects 
			// from clipping through the ground upon ragdolling.
			auto strings = RE::FixedStrings::GetSingleton(); 
			bool result = false;
			bool isInteracting = 
			(
				(a_actorToPush->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal) && 
				(
					(a_actorToPush->IsOnMount()) ||
					(HandleIsValid(a_actorToPush->GetOccupiedFurniture())) ||
					(strings && a_actorToPush->IsAnimationDriven())
				)
			);
			if (isInteracting)
			{
				// 'True' seems to forgo any associated exit animation for the interaction
				// and thus quickly unsets the animation driven state.
				a_actorToPush->StopInteractingQuick(true);
				// Re-init havok.
				a_actorToPush->DetachHavok(a_actorToPush->GetCurrent3D());
				a_actorToPush->InitHavok();
				a_actorToPush->MoveHavok(true);
			}

			// BOOM!
			a_actorToPush->currentProcess->KnockExplosion(a_actorToPush, a_contactPos, a_force);
		}
		
		RE::NiQuaternion QuaternionSlerp
		(
			const RE::NiQuaternion& a_quatA, const RE::NiQuaternion& a_quatB, double a_t
		)
		{
			// Credits to ersh1: https://github.com/ersh1/Precision/blob/main/src/Utils.cpp#L139
			// Slerp from quaternion A to quaternion B with the given ratio t.

			// quaternion to return
			RE::NiQuaternion result{ };
			// Calculate angle between them.
			float cosHalfTheta = 
			(
				a_quatA.w * a_quatB.w + 
				a_quatA.x * a_quatB.x + 
				a_quatA.y * a_quatB.y + 
				a_quatA.z * a_quatB.z
			);
			// if qa=qb or qa=-qb then theta = 0 and we can return qb
			if (fabs(cosHalfTheta) >= 0.99999)
			{
				result.w = a_quatB.w;
				result.x = a_quatB.x;
				result.y = a_quatB.y;
				result.z = a_quatB.z;
				return result;
			}

			// If the dot product is negative, slerp won't take
			// the shorter path. Note that qb and -qb are equivalent when
			// the negation is applied to all four components. Fix by
			// reversing one quaternion.
			RE::NiQuaternion q2 = a_quatB;
			if (cosHalfTheta < 0)
			{
				q2.w *= -1;
				q2.x *= -1;
				q2.y *= -1;
				q2.z *= -1;
				cosHalfTheta *= -1;
			}

			// Calculate temporary values.
			float halfTheta = acosf(cosHalfTheta);
			float sinHalfTheta = sqrtf(1.0 - cosHalfTheta * cosHalfTheta);
			// if theta = 180 degrees then result is not fully defined
			// we could rotate around any axis normal to qa or qb
			if (fabs(sinHalfTheta) < 0.001)
			{  // fabs is floating point absolute
				result.w = (a_quatA.w * 0.5 + q2.w * 0.5);
				result.x = (a_quatA.x * 0.5 + q2.x * 0.5);
				result.y = (a_quatA.y * 0.5 + q2.y * 0.5);
				result.z = (a_quatA.z * 0.5 + q2.z * 0.5);
				return result;
			}

			float ratioA = sinf((1 - a_t) * halfTheta) / sinHalfTheta;
			float ratioB = sinf(a_t * halfTheta) / sinHalfTheta;
			// calculate Quaternion
			result.w = (a_quatA.w * ratioA + q2.w * ratioB);
			result.x = (a_quatA.x * ratioA + q2.x * ratioB);
			result.y = (a_quatA.y * ratioA + q2.y * ratioB);
			result.z = (a_quatA.z * ratioA + q2.z * ratioB);

			return result;
		}

		RE::NiMatrix3 QuaternionToRotationMatrix(const RE::NiQuaternion& a_quat)
		{
			// Credits to ersh1: https://github.com/ersh1/Precision/blob/main/src/Utils.cpp#L201
			// Convert quaternion to rotation matrix.

			float sqw = a_quat.w * a_quat.w;
			float sqx = a_quat.x * a_quat.x;
			float sqy = a_quat.y * a_quat.y;
			float sqz = a_quat.z * a_quat.z;

			RE::NiMatrix3 ret{ };

			// invs (inverse square length) is only required 
			// if quaternion is not already normalised
			float invs = 1.f / (sqx + sqy + sqz + sqw);
			// since sqw + sqx + sqy + sqz =1/invs*invs
			ret.entry[0][0] = (sqx - sqy - sqz + sqw) * invs;  
			ret.entry[1][1] = (-sqx + sqy - sqz + sqw) * invs;
			ret.entry[2][2] = (-sqx - sqy + sqz + sqw) * invs;

			float tmp1 = a_quat.x * a_quat.y;
			float tmp2 = a_quat.z * a_quat.w;
			ret.entry[1][0] = 2.f * (tmp1 + tmp2) * invs;
			ret.entry[0][1] = 2.f * (tmp1 - tmp2) * invs;

			tmp1 = a_quat.x * a_quat.z;
			tmp2 = a_quat.y * a_quat.w;
			ret.entry[2][0] = 2.f * (tmp1 - tmp2) * invs;
			ret.entry[0][2] = 2.f * (tmp1 + tmp2) * invs;
			tmp1 = a_quat.y * a_quat.z;
			tmp2 = a_quat.x * a_quat.w;
			ret.entry[2][1] = 2.f * (tmp1 + tmp2) * invs;
			ret.entry[1][2] = 2.f * (tmp1 - tmp2) * invs;

			return ret;
		}

		RE::TESObjectREFR* RecurseForRefr(RE::NiNode* a_parentNode, uint8_t a_recursionDepth) 
		{
			// Look for a refr associated with the parent node.

			// No/invalid parent or max recursion depth reached.
			if (!a_parentNode || a_recursionDepth >= MAX_NODE_RECURSION_DEPTH)
			{
				return nullptr;
			}

			if (a_parentNode->userData) 
			{
				// User data is available, so return it.
				return a_parentNode->userData;
			}
			else
			{
				// Then move up the node tree if nothing is found,
				// searching the node's parents for a refr.
				if (a_parentNode->parent)
				{
					return RecurseForRefr(a_parentNode->parent, ++a_recursionDepth);
				}
				else
				{
					// No parent, so the search ends.
					return nullptr;
				}
			}
		}

		void ResetFadeOnAllObjectsInCell(RE::TESObjectCELL* a_cell)
		{
			// Iterate through all loaded objects in the cell 
			// and reset their fade amount(s) to fully faded in.

			if (!a_cell || a_cell->formID == 0x0)
			{
				return;
			}

			bool exteriorCell = a_cell->IsExteriorCell();
			{
				RE::BSSpinLockGuard lock(a_cell->spinLock);
				for (auto refrPtr : a_cell->references)
				{
					if (!refrPtr)
					{
						continue;
					}

					auto object3DPtr = GetRefr3D(refrPtr.get()); 
					if (!object3DPtr || object3DPtr->GetRefCount() <= 0)
					{
						continue;
					}

					// Reset 3D fade amount to full.
					if (object3DPtr->fadeAmount != 1.0f)
					{
						object3DPtr->fadeAmount = 1.0f;
					}

					// Reset fade node current 3D amount to full as well.
					auto fadeNode = object3DPtr->AsFadeNode();
					if (fadeNode && fadeNode->currentFade != 1.0f)
					{
						fadeNode->currentFade = 1.0;
					}
				}
			}
		}

		void ResetTPCamOrientation()
		{
			// Reset the game's third person camera to its default orientation.

			if (!glob.globalDataInit)
			{
				return;
			}

			auto playerCam = RE::PlayerCamera::GetSingleton();
			if (!playerCam || !playerCam->currentState)
			{
				return;
			}

			auto tpState = skyrim_cast<RE::ThirdPersonState*>(playerCam->currentState.get());
			if (tpState)
			{
				// Reset free rotation, zoom offset, yaw, positional offset (estimated default),
				// and process draw/sheathe change.
				tpState->freeRotationEnabled = false;
				tpState->currentZoomOffset = tpState->targetZoomOffset = 1.0f;
				tpState->targetYaw = tpState->currentYaw;
				tpState->posOffsetActual = RE::NiPoint3(0.0f, 0.0f, 50.0f);
				tpState->posOffsetExpected = RE::NiPoint3(0.0f, 0.0f, 50.0f);
				tpState->freeRotation.x = tpState->freeRotation.y = 0.0f;
				tpState->ProcessWeaponDrawnChange(true);
				tpState->ProcessWeaponDrawnChange(false);
			}

			// Switch to third person camera.
			playerCam->ForceThirdPerson();
		}

		void RotateVectorAboutAxis(RE::NiPoint3& a_vectorOut, RE::NiPoint3 a_axis, float a_angle)
		{
			// Precondition: Angle must be in radians.
			// Rotate the vector about the axis by an amount given by the angle.
			// NOTE: 
			// Vector is unitized and set through the outparam.
			// 
			// How to rotate a vector in 3d space around arbitrary axis:
			// https://math.stackexchange.com/q/4034978,
			// https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula

			if (a_axis.Length() == 0.0f || a_vectorOut.Length() == 0.0f)
			{
				return;
			}

			a_axis.Unitize();
			RE::NiMatrix3 k
			{
				RE::NiPoint3(0.0f, -a_axis.z, a_axis.y),
				RE::NiPoint3(a_axis.z, 0.0f, -a_axis.x),
				RE::NiPoint3(-a_axis.y, a_axis.x, 0.0f)
			};

			const float cosT = cosf(a_angle);
			const float sinT = sinf(a_angle);
			const float omCosT = 1.0f - cosT;
			a_vectorOut = 
			(
				(a_vectorOut * cosT) + 
				((a_axis.Cross(a_vectorOut)) * sinT) +
				(a_axis * (a_axis.Dot(a_vectorOut) * omCosT))
			);

			a_vectorOut.Unitize();
		}

		void RunPlayerActionCommand(RE::DEFAULT_OBJECT&& a_defObj, RE::Actor* a_actor)
		{
			// Run a console command which directs the actor 
			// to perform the action corresponding to the given default object.

			auto defObjMgr = RE::BGSDefaultObjectManager::GetSingleton();
			if (!defObjMgr)
			{
				return;
			}

			// Credits to Ryan for the original ConsoleUtilSSE source,
			// which has since been taken down.
			// Credits to lfrazer for the VR fork of that mod.
			// Source here: 
			// https://github.com/lfrazer/ConsoleUtilVR/blob/master/src/Papyrus.cpp
			const auto scriptFactory = 
			(
				RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
			);
			const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
			if (script)
			{
				script->SetCommand
				(
					fmt::format("pa {}", defObjMgr->objects[a_defObj]->GetFormEditorID()).c_str()
				);
				script->CompileAndRun(a_actor);
				// Cleanup.
				delete script;
			}
		}

		void SendButtonEvent
		(
			RE::INPUT_DEVICE a_inputDevice,
			RE::BSFixedString a_ueString,
			uint32_t a_buttonMask, 
			float a_pressedValue,
			float a_heldTimeSecs, 
			bool a_toggleAIDriven, 
			bool a_setPadProxiedFlag
		)
		{
			// Send a button event using the provided data
			// and free the input event pointer afterward.
			// Can also toggle AI driven for P1 and/or indicate that the event was proxied 
			// and shouldn't be discarded later when filtering input events for P1.

			// Cannot send an input event without the input device manager.
			auto bsInputMgr = RE::BSInputDeviceManager::GetSingleton();
			if (!bsInputMgr)
			{
				return;
			}

			std::unique_ptr<RE::InputEvent* const> inputEvent = 
			(
				std::make_unique<RE::InputEvent* const>
				(
					RE::ButtonEvent::Create
					(
						a_inputDevice, 
						a_ueString, 
						a_buttonMask, 
						a_pressedValue, 
						a_heldTimeSecs
					)
				)
			);
			// Sent by P1 and shouldn't be ignored.
			if (a_setPadProxiedFlag)
			{
				(*inputEvent.get())->AsButtonEvent()->pad24 = 0xC0DA;
			}

			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (p1 && a_toggleAIDriven)
			{
				// Toggle AI driven to false before sending/processing the button event.
				SetPlayerAIDriven(false);
				bsInputMgr->lock.Lock();
				bsInputMgr->SendEvent(inputEvent.get());
				bsInputMgr->lock.Unlock();
				// Clear input event pad and free.
				(*inputEvent.get())->AsButtonEvent()->pad24 = 0x0;
				RE::free(*inputEvent.get());
				// Toggle back on afterward.
				SetPlayerAIDriven(true);
			}
			else
			{
				bsInputMgr->lock.Lock();
				bsInputMgr->SendEvent(inputEvent.get());
				bsInputMgr->lock.Unlock();
				// Clear input event pad and free.
				(*inputEvent.get())->AsButtonEvent()->pad24 = 0x0;
				RE::free(*inputEvent.get());
			}
		}

		void SendCrosshairEvent
		(
			RE::TESObjectREFR* a_crosshairRefrToSet, const int32_t& a_requestingCID
		)
		{
			// Send a crosshair refr event after setting the crosshair refr.
			// Will trigger the QuickLoot menu to open, if the refr is valid,
			// or close the open QuickLoot menu if the refr is nullptr.
			// Free the event pointer when done.
			// // IMPORTANT:
			// Specify a CID for any player trying to set the crosshair refr (non-nullptr).
			// To clear the crosshair refr, pass in nullptr as the crosshair refr to set 
			// and do not provide a requesting CID.

			const auto eventSource = SKSE::GetCrosshairRefEventSource();
			if (!eventSource)
			{
				return;
			}

			SKSE::CrosshairRefEvent* crosshairEvent = RE::malloc<SKSE::CrosshairRefEvent>
			(
				sizeof(SKSE::CrosshairRefEvent)
			);
			std::memset(crosshairEvent, 0, sizeof(SKSE::CrosshairRefEvent));
			if (crosshairEvent) 
			{
				// Save container handle to match against in the crosshair event handler
				// and for menu CID resolution later.
				glob.reqQuickLootContainerHandle = 
				(
					a_crosshairRefrToSet ? 
					a_crosshairRefrToSet->GetHandle() :
					RE::ObjectRefHandle()
				);
				glob.quickLootReqCID = a_requestingCID;
				crosshairEvent->crosshairRef = RE::NiPointer<RE::TESObjectREFR>
				(
					a_crosshairRefrToSet
				);
				eventSource->SendEvent(crosshairEvent);
				RE::free(crosshairEvent);

				SPDLOG_DEBUG
				(
					"[Util] SendCrosshairEvent: Set to {}, CID: {}.",
					a_crosshairRefrToSet ? a_crosshairRefrToSet->GetName() : "NONE",
					a_requestingCID
				);
			}
		}

		void SendHitData
		(
			const RE::ActorHandle& a_aggressor, 
			const RE::ActorHandle& a_target, 
			const RE::ObjectRefHandle& a_source,
			RE::InventoryEntryData* a_invEntryData,
			const float& a_damage, 
			const SKSE::stl::enumeration<RE::HitData::Flag,std::uint32_t>& a_flags, 
			const float& a_stagger,
			const float& a_pushBack,
			const RE::NiPoint3& a_hitPos, 
			const RE::NiPoint3& a_hitDir
		)
		{
			// Send a hit event with damage and characteristics based on all the given data.

			if (!HandleIsValid(a_aggressor) || !HandleIsValid(a_target))
			{
				return;
			}

			RE::HitData hitData{ };
			NativeFunctions::HitData_Ctor(std::addressof(hitData));
			hitData.Populate(a_aggressor.get().get(), a_target.get().get(), a_invEntryData);

			// Zero everything except physical and total damage.
			hitData.bonusHealthDamageMult =
			hitData.criticalDamageMult =
			hitData.reflectedDamage =
			hitData.resistedPhysicalDamage =
			hitData.resistedTypedDamage =
			hitData.targetedLimbDamage = 0.0f;
			hitData.physicalDamage =
			hitData.totalDamage = a_damage;
			// Stagger and push back.
			hitData.stagger = a_stagger;
			hitData.pushBack = a_pushBack;

			// Hit position and direction.
			if (a_hitPos != RE::NiPoint3())
			{
				hitData.hitPosition = a_hitPos;
			}

			if (a_hitDir != RE::NiPoint3())
			{
				hitData.hitDirection = a_hitDir;
			}

			// Set source, if any.
			if (HandleIsValid(a_source))
			{
				hitData.sourceRef = a_source;
			}

			hitData.flags = a_flags;
			if (hitData.flags.all(RE::HitData::Flag::kSneakAttack))
			{
				hitData.sneakAttackBonus = 2.0f;
			}
			else
			{
				hitData.sneakAttackBonus = 1.0f;
			}

			// Applies the hit and sends the event.
			NativeFunctions::Actor_ApplyHitData
			(
				a_target.get().get(), std::addressof(hitData)
			);
		};

		void SendInputEvent(std::unique_ptr<RE::InputEvent* const>& a_inputEvent)
		{
			// Send the already-constructed input event and free it after handling.

			auto bsInputMgr = RE::BSInputDeviceManager::GetSingleton();
			if (!bsInputMgr) 
			{
				return;
			}

			bsInputMgr->lock.Lock();
			bsInputMgr->SendEvent(a_inputEvent.get());
			bsInputMgr->lock.Unlock();
			RE::free(*a_inputEvent.get());
		}

		void SendThumbstickEvent
		(
			const RE::BSFixedString& a_ueString, float a_xValue, float a_yValue, bool a_isLS
		)
		{
			// Send a thumbstick event for the left/right analog stick 
			// using the provided X, Y displacements and user event name.
			// Free the input event pointer after handling.

			auto bsInputMgr = RE::BSInputDeviceManager::GetSingleton();
			if (!bsInputMgr)
			{
				return;
			}

			std::unique_ptr<RE::InputEvent* const> inputEvent = 
			(
				std::make_unique<RE::InputEvent* const>
				(
					CreateThumbstickEvent(a_ueString, a_xValue, a_yValue, a_isLS)
				)
			);
			// Set proxied bypass flag for all thumbstick events.
			(*inputEvent.get())->AsIDEvent()->pad24 = 0xC0DA;
			bsInputMgr->lock.Lock();
			bsInputMgr->SendEvent(inputEvent.get());
			bsInputMgr->lock.Unlock();
			// Clear the pad and then free after sending.
			(*inputEvent.get())->AsIDEvent()->pad24 = 0x0;
			RE::free(*inputEvent.get());
		}

		void Set3DCollisionFilterInfo(RE::NiAVObject* a_refr3D, const bool& a_set)
		{
			// Set/unset the layer bitfields to allow/disallow collisions between
			// the char controller, biped, biped no char controller, and dead biped layers.

			if (!a_refr3D)
			{
				return;
			}

			auto collisionObject = a_refr3D->GetCollisionObject();
			if (!collisionObject)
			{
				return;
			}

			auto rigidBody = collisionObject->GetRigidBody();
			if (!rigidBody) 
			{
				return;
			}

			auto world = rigidBody->GetWorld1(); 
			if (!world)
			{
				world = rigidBody->GetWorld2();
				if (!world)
				{
					return;
				}
			}

			auto filterInfo = (RE::bhkCollisionFilter*)world->collisionFilter; 
			if (!filterInfo)
			{
				return;
			}

			// Credits to ersh1 for the code on setting what other collision layers 
			// collide with a collision layer:
			// https://github.com/ersh1/Precision/blob/main/src/Hooks.cpp#L848
			if (a_set)
			{
				filterInfo->layerBitfields[!RE::COL_LAYER::kCharController] |= 
				(
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
					)
				);
				filterInfo->layerBitfields[!RE::COL_LAYER::kBiped] |= 
				(
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
					)
				);
				filterInfo->layerBitfields[!RE::COL_LAYER::kBipedNoCC] |= 
				(
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
					)
				);
				filterInfo->layerBitfields[!RE::COL_LAYER::kDeadBip] |= 
				(
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
					)
				);
			}
			else
			{
				filterInfo->layerBitfields[!RE::COL_LAYER::kCharController] &= 
				~(
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
					)
				);
				filterInfo->layerBitfields[!RE::COL_LAYER::kBiped] &= 
				~(
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
					)
				);
				filterInfo->layerBitfields[!RE::COL_LAYER::kBipedNoCC] &= 
				~(
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
					)
				);
				filterInfo->layerBitfields[!RE::COL_LAYER::kDeadBip] &= 
				~(
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBiped)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kBipedNoCC)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
					) |
					(
						static_cast<uint64_t>(1) << 
						static_cast<uint8_t>(!RE::COL_LAYER::kDeadBip)
					)
				);
			}

			// NOTE:
			// Unsure if traversing the node tree to modify each node's filterinfo is necessary,
			// so will be removed in the future.
			if (auto node = a_refr3D->AsNode(); node)
			{
				for (auto childPtr : node->children)
				{
					if (!childPtr)
					{
						continue;
					}

					Set3DCollisionFilterInfo(childPtr.get(), a_set);
				}
			}
		}

		void SetActorsDetectionEvent
		(
			RE::Actor* a_actor, 
			RE::TESObjectREFR* a_collidingRefr, 
			float a_collidingMass,
			const RE::NiPoint3 & a_contactPoint
		)
		{
			// Major thanks to powerof3 for Grab and Throw's method of doing this:
			// https://github.com/powerof3/GrabAndThrow/blob/master/src/GrabThrowHandler.cpp#L250

			auto currentProc = a_actor->currentProcess; 
			if (!currentProc)
			{
				return;
			}

			RE::SOUND_LEVEL level = RE::SOUND_LEVEL::kNormal;
			if (a_collidingMass == 0.0f)
			{
				level = RE::SOUND_LEVEL::kSilent;
			}
			else if (a_collidingMass < 3.0f)
			{
				level = RE::SOUND_LEVEL::kQuiet;
			}
			else if (a_collidingMass < 10.0f)
			{
				level = RE::SOUND_LEVEL::kNormal;
			}
			else if (a_collidingMass < 30.0f)
			{
				level = RE::SOUND_LEVEL::kLoud;
			}
			else
			{
				level = RE::SOUND_LEVEL::kVeryLoud;
			}

			currentProc->SetActorsDetectionEvent
			(
				a_actor, 
				a_contactPoint, 
				RE::AIFormulas::GetSoundLevelValue(level),
				a_collidingRefr
			);
		}

		void SetCameraPosition(RE::TESCamera* a_cam, const RE::NiPoint3& a_position)
		{
			// Place the current camera at the provided position.
			// Ensure positioning consistency: 
			// NiAVObject node positions, NiCamera world position, 
			// and third person state positions.

			if (!a_cam || !a_cam->cameraRoot)
			{
				return;
			}

			// Node 3D local and world positions should match.
			a_cam->cameraRoot->local.translate = a_cam->cameraRoot->world.translate = a_position;
			// Set the NiCamera world position.
			if (auto niCamPtr = GetNiCamera(); niCamPtr) 
			{
				niCamPtr->world.translate = a_position;
			}
			
			const auto playerCam = RE::PlayerCamera::GetSingleton(); 
			if (playerCam && playerCam->currentState)
			{
				// Set the third person state world position and collision position.
				auto tpState = skyrim_cast<RE::ThirdPersonState*>(playerCam->currentState.get());
				if (tpState)
				{
					tpState->translation = a_position;
					tpState->collisionPos = a_position;
				}
			}
		}

		void SetCameraRotation
		(
			RE::TESCamera* a_cam, 
			const float& a_pitch,
			const float& a_yaw,
			bool a_overrideLocalRotation
		)
		{
			// Set the rotation (pitch, yaw) of the camera to the pitch/yaw angles provided.
			// Ensure rotational consistency: 
			// NiAVObject node world rotation, NiCamera world rotation, 
			// and third person state rotation quaternion.
			// All credit goes to mwilsnd for the process 
			// of setting the camera's rotation properly:
			// https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/source/thirdperson.cpp#L685

			if (!glob.globalDataInit)
			{
				return;
			}

			RE::NiMatrix3 nodeMat{ };
			SetRotationMatrixPY(nodeMat, a_pitch, a_yaw);
			// Other mods have features that make changes to the camera's local rotation, 
			// such as Precision's hitstop + camera shake 
			// (https://www.nexusmods.com/skyrimspecialedition/mods/72347)
			// and Camera Noise's Perlin noise
			// (https://www.nexusmods.com/skyrimspecialedition/mods/77185).
			// So we do not always want to modify it,
			// unless we have to override the positioning entirely or prevent screen shake.
			a_cam->cameraRoot->world.rotate = nodeMat;
			if (a_overrideLocalRotation)
			{
				a_cam->cameraRoot->local.rotate = nodeMat;
			}

			RE::NiMatrix3 niMat{ };
			// Rearrange matrix entries for the NiCamera's world rotation matrix.
			niMat.entry[0][0] = nodeMat.entry[0][1];
			niMat.entry[0][1] = nodeMat.entry[0][2];
			niMat.entry[0][2] = nodeMat.entry[0][0];
			niMat.entry[1][0] = nodeMat.entry[1][1];
			niMat.entry[1][1] = nodeMat.entry[1][2];
			niMat.entry[1][2] = nodeMat.entry[1][0];
			niMat.entry[2][0] = nodeMat.entry[2][1];
			niMat.entry[2][1] = nodeMat.entry[2][2];
			niMat.entry[2][2] = nodeMat.entry[2][0];
			if (auto niCamPtr = GetNiCamera(); niCamPtr) 
			{
				niCamPtr->world.rotate = niMat;
			}

			auto playerCam = RE::PlayerCamera::GetSingleton(); 
			if (playerCam && playerCam->currentState)
			{
				auto tpState = skyrim_cast<RE::ThirdPersonState*>(playerCam->currentState.get()); 
				if (tpState)
				{
					glm::quat q = glm::quat
					(
						glm::vec3(-a_pitch, 0.0f, -a_yaw)
					);
					tpState->rotation = RE::NiQuaternion(q.w, q.x, q.y, q.z);
				}
			}
		}

		void SetLinearVelocity(RE::Actor* a_actor, RE::NiPoint3 a_velocity)
		{
			// Set the actor's linear velocity to the provided velocity.

			auto charController = a_actor->GetCharController();
			if (!charController)
			{
				return;
			}

			a_velocity *= GAME_TO_HAVOK;
			RE::hkVector4 oldVelVect{ };
			charController->GetLinearVelocityImpl(oldVelVect);
			RE::hkVector4 velVect{ };
			// Retain w component.
			velVect.quad = _mm_setr_ps
			(
				a_velocity.x, a_velocity.y, a_velocity.z, oldVelVect.quad.m128_f32[3]
			);
			charController->SetLinearVelocityImpl(velVect);
		}

		bool SetPlayerAIDriven(const bool&& a_shouldSet)
		{
			// Change P1's AI driven flag to the requested value.
			// Return true if the request was fulfilled successfully
			// and P1's AI driven state changed.

			if (!glob.globalDataInit)
			{
				return false;
			}

			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			if (!p1 || !p1->movementController)
			{
				return false;
			}

			// Reset AI driven if this func is called 
			// when outside of co-op or if P1's managers are not active.
			bool shouldResetAIDriven = 
			{
				!glob.coopSessionActive || 
				!glob.coopPlayers[glob.player1CID]->IsRunning()
			};
			if (shouldResetAIDriven)
			{
				p1->SetAIDriven(false);
				return true;
			}

			// Reset/set if different from the current value.
			if ((p1->movementController->controlsDriven && a_shouldSet) ||
				(!p1->movementController->controlsDriven && !a_shouldSet))
			{
				p1->SetAIDriven(a_shouldSet);
				return true;
			}

			return false;
		}

		void SetPosition(RE::Actor* a_actor, RE::NiPoint3 a_position)
		{
			// Set the actor's position within the actor's
			// current worldspace and parent cell.

			auto charController = a_actor->GetCharController(); 
			if (!charController)
			{
				return;
			}

			a_position *= GAME_TO_HAVOK;
			// Save old position's w component.
			RE::hkVector4 oldPosVect{ };
			charController->GetPositionImpl(oldPosVect, true);
			RE::hkVector4 newPosVect{ };
			// Set w component to the old position's.
			newPosVect.quad = _mm_setr_ps
			(
				a_position.x, a_position.y, a_position.z, oldPosVect.quad.m128_f32[3]
			);
			charController->lock.Lock();
			charController->SetPositionImpl(newPosVect, false, true);
			charController->lock.Unlock();
			a_actor->Update3DPosition(true);
		}

		void SetRotationMatrixPY
		(
			RE::NiMatrix3& a_rotMatrix, const float& a_pitch, const float& a_yaw
		)
		{
			// Construct a rotation matrix based on the game pitch and yaw angles.
			// Then set the rotation matrix outparam to this new matrix.

			auto rightAxis = RE::NiPoint3(1.0f, 0.0f, 0.0f);
			auto upAxis = RE::NiPoint3(0.0f, 0.0f, 1.0f);
			const auto matPitch = MatrixFromAxisAndAngle(rightAxis, a_pitch);
			const auto matYaw = MatrixFromAxisAndAngle(upAxis, a_yaw);
			const auto resultMat = matYaw * matPitch;

			// Manual corrections to match Skyrim's left-handed extrinsic ZYX Euler convention.
			a_rotMatrix.entry[0][0] = resultMat.entry[0][0];
			a_rotMatrix.entry[0][1] = -resultMat.entry[0][1];
			a_rotMatrix.entry[0][2] = resultMat.entry[0][2];
			a_rotMatrix.entry[1][0] = -resultMat.entry[1][0];
			a_rotMatrix.entry[1][1] = resultMat.entry[1][1];
			a_rotMatrix.entry[1][2] = -resultMat.entry[1][2];
			a_rotMatrix.entry[2][0] = resultMat.entry[2][0];
			a_rotMatrix.entry[2][1] = -resultMat.entry[2][1];
			a_rotMatrix.entry[2][2] = resultMat.entry[2][2];
		}

		void SetRotationMatrixPYR
		(
			RE::NiMatrix3& a_rotMatrix, 
			const float& a_pitch,
			const float& a_yaw,
			const float& a_roll
		)
		{
			// Construct a rotation matrix based on the game pitch, roll, and yaw angles.
			// Then set the rotation matrix outparam to this new matrix.

			auto rightAxis = RE::NiPoint3(1.0f, 0.0f, 0.0f);
			auto upAxis = RE::NiPoint3(0.0f, 0.0f, 1.0f);
			auto forwardAxis = RE::NiPoint3(0.0f, 1.0f, 0.0f);
			const auto matPitch = MatrixFromAxisAndAngle(rightAxis, a_pitch);
			const auto matYaw = MatrixFromAxisAndAngle(upAxis, a_yaw);
			const auto matRoll = MatrixFromAxisAndAngle(forwardAxis, a_roll);
			const auto resultMat = matYaw * matRoll * matPitch;

			// Manual sign corrections to match Skyrim's left hand extrinsic ZYX Euler convention.
			a_rotMatrix.entry[0][0] = resultMat.entry[0][0];
			a_rotMatrix.entry[0][1] = -resultMat.entry[0][1];
			a_rotMatrix.entry[0][2] = resultMat.entry[0][2];
			a_rotMatrix.entry[1][0] = -resultMat.entry[1][0];
			a_rotMatrix.entry[1][1] = resultMat.entry[1][1];
			a_rotMatrix.entry[1][2] = -resultMat.entry[1][2];
			a_rotMatrix.entry[2][0] = resultMat.entry[2][0];
			a_rotMatrix.entry[2][1] = -resultMat.entry[2][1];
			a_rotMatrix.entry[2][2] = resultMat.entry[2][2];
		}

		void SetRotationMatrixPYRAndAxes
		(
			RE::NiMatrix3& a_rotMatrix, 
			const RE::NiPoint3& a_xAxis,
			const RE::NiPoint3& a_yAxis, 
			const RE::NiPoint3& a_zAxis,
			const float& a_pitch,
			const float& a_yaw, 
			const float& a_roll
		) 
		{
			// Construct a rotation matrix by rotating the three axes provided 
			// by the game pitch, roll, and yaw angles.
			// Then set the rotation matrix outparam to this new matrix.

			const auto matPitch = MatrixFromAxisAndAngle(a_xAxis, a_pitch);
			const auto matYaw = MatrixFromAxisAndAngle(a_zAxis, a_yaw);
			const auto matRoll = MatrixFromAxisAndAngle(a_yAxis, a_roll);
			const auto resultMat = matYaw * matRoll * matPitch;
			
			// Manual sign corrections to match Skyrim's left hand extrinsic ZYX Euler convention.
			a_rotMatrix.entry[0][0] = resultMat.entry[0][0];
			a_rotMatrix.entry[0][1] = -resultMat.entry[0][1];
			a_rotMatrix.entry[0][2] = resultMat.entry[0][2];
			a_rotMatrix.entry[1][0] = -resultMat.entry[1][0];
			a_rotMatrix.entry[1][1] = resultMat.entry[1][1];
			a_rotMatrix.entry[1][2] = -resultMat.entry[1][2];
			a_rotMatrix.entry[2][0] = resultMat.entry[2][0];
			a_rotMatrix.entry[2][1] = -resultMat.entry[2][1];
			a_rotMatrix.entry[2][2] = resultMat.entry[2][2];
		}

		bool ShouldCastWithP1(RE::SpellItem* a_spell)
		{
			// Return true if the spell should be cast by one of P1's magic casters.

			if (!a_spell)
			{
				return false;
			}

			// Eventually will check against a hardcoded list of spells 
			// that have been tested to work when cast by P1 and no one else.
			// Only one so far.

			// Battle Cry.
			bool shouldP1Cast = 
			{ 
				a_spell->formID == 0xE40C3 
			};
			if (shouldP1Cast)
			{
				return true;
			}

			// If the spell is NOT a self-targeted concentration spell,
			// do not cast with P1.
			bool isSelfConc = 
			{
				a_spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration &&
				a_spell->GetDelivery() == RE::MagicSystem::Delivery::kSelf
			};
			if (!isSelfConc)
			{
				return false;
			}

			// Is a self-targeted concentration spell, 
			// so check if the spell has an imagespace modifier.
			// Most spells with imagespace modifiers 
			// cannot be cast by NPCs and must be cast via P1.
			if (a_spell->avEffectSetting && a_spell->avEffectSetting->data.imageSpaceMod)
			{
				return true;
			}

			// Check all effects for an imagespace modifier next.
			for (auto effect : a_spell->effects)
			{
				if (effect && effect->baseEffect && effect->baseEffect->data.imageSpaceMod)
				{
					return true;
				}
			}

			return false;
		}

		void StartEffectShader
		(
			RE::TESObjectREFR* a_refr, RE::TESEffectShader* a_shader, const float& a_timeSecs
		)
		{
			// Start playing the effect shader on the refr for the provided number of seconds,
			// or continue playing it indefinitely if the requested play time is -1.

			if (!a_refr || 
				!a_refr->loadedData || 
				a_refr->IsDisabled() || 
				a_refr->IsDeleted() || 
				!a_refr->IsHandleValid() || 
				!a_shader)
			{
				return;
			}

			// Check if the effect is already active, 
			// and if so just update its lifetime instead of applying it again.
			bool found = false;
			if (const auto processLists = RE::ProcessLists::GetSingleton(); processLists)
			{
				processLists->magicEffectsLock.Lock();
				for (const auto tempEffectPtr : processLists->magicEffects)
				{
					if (!tempEffectPtr || !tempEffectPtr->As<RE::ShaderReferenceEffect>())
					{
						continue;
					}

					// Check if the shader effect is currently playing on the target refr.
					auto shaderEffect = tempEffectPtr->As<RE::ShaderReferenceEffect>();
					if (shaderEffect && 
						HandleIsValid(shaderEffect->target) && 
						shaderEffect->target.get().get() == a_refr &&
						shaderEffect->effectData == a_shader)
					{
						// Update its lifetime if not playing indefinitely.
						if (shaderEffect->lifetime != -1.0f)
						{
							shaderEffect->lifetime = a_timeSecs;
						}
							
						shaderEffect->finished = false;
						shaderEffect->Resume();
						found = true;
						break;
					}
				}
				processLists->magicEffectsLock.Unlock();
			}

			// Apply a new instance of the effect if it isn't already playing.
			if (!found)
			{
				a_refr->ApplyEffectShader(a_shader, a_timeSecs);
			}
		}

		void StartHitArt
		(
			RE::TESObjectREFR* a_refr,
			RE::BGSArtObject* a_artObj, 
			RE::TESObjectREFR* a_facingRefr, 
			const float& a_timeSecs, 
			bool&& a_faceTarget, 
			bool&& a_attachToCamera
		)
		{
			// Start playing the hit art on the refr, facing the other provided refr,
			// for the provided number of seconds,
			// or continue playing it indefinitely if the requested play time is -1.
			
			if (!a_refr || 
				!a_refr->loadedData || 
				a_refr->IsDisabled() ||
				a_refr->IsDeleted() || 
				!a_refr->IsHandleValid() || 
				!a_artObj)
			{
				return;
			}

			// Find and stop the current instance of the hit art, if any,
			// before applying a new instance.
			const auto processLists = RE::ProcessLists::GetSingleton(); 
			if (!processLists)
			{
				return;
			}

			processLists->magicEffectsLock.Lock();
			for (const auto tempEffectPtr : processLists->magicEffects)
			{
				if (!tempEffectPtr || !tempEffectPtr->As<RE::ModelReferenceEffect>())
				{
					continue;
				}

				// Check if the hit art is playing on the target already
				// and mark it as finished, since we'll restart the effect instead
				// to ensure the effect properly plays fully from the beginning to the end.
				auto hitArtEffect = tempEffectPtr->As<RE::ModelReferenceEffect>();
				if (hitArtEffect && 
					HandleIsValid(hitArtEffect->target) && 
					hitArtEffect->target.get().get() == a_refr &&
					hitArtEffect->artObject == a_artObj)
				{
					hitArtEffect->finished = true;
					hitArtEffect->Suspend();
					hitArtEffect->Detach();
					break;
				}
			}
			processLists->magicEffectsLock.Unlock();

			a_refr->ApplyArtObject
			(
				a_artObj, a_timeSecs, a_facingRefr, a_faceTarget, a_attachToCamera
			);
		}

		void StopAllEffectShaders(RE::TESObjectREFR* a_refr)
		{
			// Stop all effect shaders playing on the refr.
			// Credits to po3:
			// Adapted from their papyrus extender code found here:
			// https://github.com/powerof3/PapyrusExtenderSSE/blob/master/src/Papyrus/Util/Graphics.cpp#L42

			if (!a_refr ||
				!a_refr->loadedData ||
				a_refr->IsDisabled() || 
				a_refr->IsDeleted() || 
				!a_refr->IsHandleValid())
			{
				return;
			}

			const auto processLists = RE::ProcessLists::GetSingleton(); 
			if (!processLists)
			{
				return;
			}

			processLists->magicEffectsLock.Lock();
			for (const auto tempEffectPtr : processLists->magicEffects)
			{
				if (!tempEffectPtr || !tempEffectPtr->As<RE::ShaderReferenceEffect>())
				{
					continue;
				}

				auto shaderEffect = tempEffectPtr->As<RE::ShaderReferenceEffect>();
				// The effect is playing on the refr, so set it as finished
				// and allow the game to clean it up at its own pace.
				if (shaderEffect && 
					HandleIsValid(shaderEffect->target) && 
					shaderEffect->target.get().get() == a_refr)
				{
					shaderEffect->finished = true;
				}
			}
			processLists->magicEffectsLock.Unlock();
		}

		void StopAllHitArtEffects(RE::TESObjectREFR* a_refr)
		{
			// Stop all hit art objects playing on the refr.

			if (!a_refr ||
				!a_refr->loadedData || 
				a_refr->IsDisabled() ||
				a_refr->IsDeleted() ||
				!a_refr->IsHandleValid())
			{
				return;
			}

			const auto processLists = RE::ProcessLists::GetSingleton(); 
			if (!processLists)
			{
				return;
			}
			
			processLists->magicEffectsLock.Lock();
			for (const auto tempEffectPtr : processLists->magicEffects)
			{
				if (!tempEffectPtr || !tempEffectPtr->As<RE::ModelReferenceEffect>())
				{
					continue;
				}

				auto hitArtEffect = tempEffectPtr->As<RE::ModelReferenceEffect>();
				// The effect is playing on the refr, so set it as finished
				// and allow the game to clean it up at its own pace.
				if (hitArtEffect && 
					HandleIsValid(hitArtEffect->target) && 
					hitArtEffect->target.get().get() == a_refr)
				{
					hitArtEffect->finished = true;
				}
			}
			processLists->magicEffectsLock.Unlock();
		}

		void StopEffectShader
		(
			RE::TESObjectREFR* a_refr, RE::TESEffectShader* a_shader, float&& a_delayedStopSecs
		)
		{
			// Stop the effect shader on the refr, optionally setting its lifetime 
			// to the provided delayed stop time to have it stop at a later point.

			if (!a_refr ||
				!a_refr->loadedData || 
				a_refr->IsDisabled() ||
				a_refr->IsDeleted() || 
				!a_refr->IsHandleValid() || 
				!a_shader)
			{
				return;
			}

			const auto processLists = RE::ProcessLists::GetSingleton(); 
			if (!processLists)
			{
				return;
			}

			// Have one instance of the effect stop after the requested number of seconds, 
			// while all others stop instantly.
			// Or stop all instances if there was no specified delayed stop time.
			bool shouldStop = a_delayedStopSecs == -1.0f;
			processLists->magicEffectsLock.Lock();
			for (const auto tempEffectPtr : processLists->magicEffects)
			{
				if (!tempEffectPtr || !tempEffectPtr->As<RE::ShaderReferenceEffect>())
				{
					continue;
				}

				const auto shaderEffect = tempEffectPtr->As<RE::ShaderReferenceEffect>();
				if (shaderEffect && 
					HandleIsValid(shaderEffect->target) && 
					shaderEffect->target.get().get() == a_refr && 
					shaderEffect->effectData == a_shader)
				{
					if (shouldStop)
					{
						shaderEffect->finished = true;
					}
					else
					{
						shaderEffect->lifetime = a_delayedStopSecs;
						shouldStop = true;
					}
				}
			}
			processLists->magicEffectsLock.Unlock();
		}

		void StopHitArt
		(
			RE::TESObjectREFR* a_refr, RE::BGSArtObject* a_artObj, float&& a_delayedStopSecs
		)
		{
			// Stop the hit art from playing on the refr, optionally setting its lifetime
			// to the provided delayed stop time to have it stop at a later point.

			if (!a_refr || 
				!a_refr->loadedData ||
				a_refr->IsDisabled() ||
				a_refr->IsDeleted() ||
				!a_refr->IsHandleValid() || 
				!a_artObj)
			{
				return;
			}

			const auto processLists = RE::ProcessLists::GetSingleton(); 
			if (!processLists)
			{
				return;
			}
			
			// Have one instance of the hit art stop after the requested number of seconds,
			// while all others stop instantly.
			// Or stop all instances if there was no specified delayed stop time.
			bool shouldStop = a_delayedStopSecs == -1.0f;
			processLists->magicEffectsLock.Lock();
			for (const auto tempEffectPtr : processLists->magicEffects)
			{
				if (!tempEffectPtr || !tempEffectPtr->As<RE::ModelReferenceEffect>())
				{
					continue;
				}

				const auto hitArtEffect = tempEffectPtr->As<RE::ModelReferenceEffect>();
				if (hitArtEffect && 
					HandleIsValid(hitArtEffect->target) && 
					hitArtEffect->target.get().get() == a_refr && 
					hitArtEffect->artObject == a_artObj)
				{
					if (shouldStop)
					{
						hitArtEffect->finished = true;
					}
					else
					{
						hitArtEffect->lifetime = a_delayedStopSecs;
						shouldStop = true;
					}
				}
			}
			processLists->magicEffectsLock.Unlock();
		}

		void TeleportToActor(RE::Actor* a_teleportingActor, RE::Actor* a_target)
		{
			// Teleport the actor to the target actor by placing down
			// entry and exit portals and moving the actor between the two.

			if (!a_teleportingActor || !a_target)
			{
				return;
			}

			// Get the portal object.
			auto teleportalActivator = RE::TESForm::LookupByID<RE::TESObjectACTI>(0x7CD55);
			// No portal, no teleportation.
			if (!teleportalActivator)
			{
				return;
			}

			// Stop the actor from moving first.
			NativeFunctions::SetDontMove(a_teleportingActor, true);

			// Use MoveTo() instead of SetPosition() 
			// if either parent cell is invalid,
			// if the actors are in different types of cells,
			// or if the teleporting actor has been unloaded 
			// while the target actor's cell is attached.
			bool shouldMoveTo = 
			{
				(!a_target->parentCell || !a_teleportingActor->parentCell) ||
				(
					a_target->parentCell->IsExteriorCell() &&
					a_teleportingActor->parentCell->IsInteriorCell()
				) ||
				(
					a_target->parentCell->IsInteriorCell() && 
					a_teleportingActor->parentCell->IsExteriorCell()
				) ||
				(a_target->parentCell->IsAttached() && !a_teleportingActor->Is3DLoaded())
			};
			// Set down the entry portal at the teleporting actor's location
			// and move the teleporting actor to it.
			auto portalPtr = a_teleportingActor->PlaceObjectAtMe
			(
				teleportalActivator, false
			);
			// If no portal materializes, don't move the teleporting actor at all.
			if (portalPtr)
			{
				if (shouldMoveTo)
				{
					a_teleportingActor->MoveTo(portalPtr.get());
				}
				else
				{
					a_teleportingActor->SetPosition(portalPtr.get()->data.location, true);
				}
			}

			// Set down the exit portal at the target's location
			// and move the teleporting actor to it.
			portalPtr = a_target->PlaceObjectAtMe(teleportalActivator, false);
			if (portalPtr)
			{
				if (shouldMoveTo)
				{
					a_teleportingActor->MoveTo(portalPtr.get());
				}
				else
				{
					a_teleportingActor->SetPosition(portalPtr.get()->data.location, true);
				}
			}
			else
			{
				// If no portal materializes, move to the target actor's location instead.
				if (shouldMoveTo)
				{
					a_teleportingActor->MoveTo(a_target);
				}
				else
				{
					a_teleportingActor->SetPosition(a_target->data.location, true);
				}
			}

			// Re-enable movement for the teleporting actor when done.
			NativeFunctions::SetDontMove(a_teleportingActor, false);
		}

		void ToggleAllControls(bool a_shouldEnable)
		{
			// Toggle all P1's controls on or off.

			auto controlMap = RE::ControlMap::GetSingleton();
			if (!controlMap)
			{
				return;
			}

			// All except for kInvalid and kNone.
			controlMap->lock.Lock();
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kActivate, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kAll, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kConsole, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kFighting, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kJumping, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kLooking, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kMainFour, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kMenu, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kMovement, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kPOVSwitch, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kSneaking, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kVATS, a_shouldEnable);
			controlMap->ToggleControls(RE::ControlMap::UEFlag::kWheelZoom, a_shouldEnable);
			controlMap->ignoreKeyboardMouse = false;
			controlMap->lock.Unlock();

			// Run console command to enable all player controls as well, 
			// just in case something slipped through the cracks.
			auto taskInterface = SKSE::GetTaskInterface(); 
			if (!taskInterface) 
			{
				return;
			}

			taskInterface->AddTask
			(
				[]() 
				{
					const auto scriptFactory = 
					(
						RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
					);
					const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
					if (script)
					{
						auto p1 = RE::PlayerCharacter::GetSingleton();
						if (p1)
						{
							script->SetCommand("epc");
							script->CompileAndRun(p1);
						}

						// Cleanup.
						delete script;
					}
				}
			);
		}

		void ToggleMagicCasterChecks
		(
			RE::Actor* a_actor, const RE::MagicSystem::CastingSource& a_source, bool a_on
		)
		{
			// Toggling on restores all checks that are performed before casting a spell.
			// Toggling off allows the actor to cast any spell
			// without magicka and target location check restrictions.

			if (!a_actor)
			{
				return;
			}

			auto caster = a_actor->magicCasters[!a_source];
			if (!caster)
			{
				return;
			}

			if (a_on)
			{
				caster->flags.reset(RE::ActorMagicCaster::Flags::kSkipCheckCast);
			}
			else
			{
				caster->flags.set(RE::ActorMagicCaster::Flags::kSkipCheckCast);
			}
		}

		void TraverseAllPerks
		(
			RE::Actor* a_actor, 
			std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> a_visitor
		)
		{
			// Run the provided visitor function on all nodes in all P1's skill perk trees.

			auto avList = RE::ActorValueList::GetSingleton(); 
			if (!avList || !avList->actorValues)
			{
				return;
			}

			for (auto av : avList->actorValues)
			{
				// Each individual skill's perk tree.
				if (!av || !av->perkTree)
				{
					continue;
				}

				TraversePerkTree(av->perkTree, a_actor, a_visitor);
			}
		}

		void TraverseChildNodesDFS
		(
			RE::NiAVObject* a_current3D, std::function<void(RE::NiAVObject* a_node)> a_visitor
		) 
		{
			// Run the visitor function on each child node of the passed-in node.
			// Recursive pre-order, depth-first traversal.

			if (!a_current3D || a_current3D->GetRefCount() == 0) 
			{
				return;
			}

			// Run on the current 3D object.
			a_visitor(a_current3D);

			// Then check for valid children to visit.
			auto asNode = a_current3D->AsNode(); 
			if (!asNode || asNode->children.empty()) 
			{
				return;
			}

			for (auto childPtr : asNode->children)
			{
				if (!childPtr || childPtr->GetRefCount() <= 0)
				{
					continue;
				}
				
				TraverseChildNodesDFS(childPtr.get(), a_visitor);
			}
		}

		void TraversePerkTree
		(
			RE::BGSSkillPerkTreeNode* a_node,
			RE::Actor* a_actor, 
			std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> a_visitor
		)
		{
			// Run the visitor function on each node in the skill tree 
			// starting from the provided root node.
			// Recurse on the current node's children, if any.
			// NOTE: 
			// The first node passed in should always be the root node 
			// for the perk tree, which does not have an associated perk 
			// and simply points to the first perk node in the tree
			// (its first child node).

			if (!a_node)
			{
				return;
			}

			if (a_node->children.empty())
			{
				// No children. 
				// Visit the current leaf node and return.
				a_visitor(a_node, a_actor);
				return;
			}
			else
			{
				for (const auto perkChildNode : a_node->children)
				{
					if (!perkChildNode)
					{
						continue;
					}

					// Visit the child node and then recurse on its children.
					a_visitor(perkChildNode, a_actor);
					TraversePerkTree(perkChildNode, a_actor, a_visitor);
				}
			}
		}

		bool TriggerFalseSkillLevelUp
		(
			const RE::ActorValue& a_avSkill, 
			const Skill& a_skill, 
			const std::string& a_skillName, 
			const float& a_newLevel
		)
		{
			// Trigger a temporary 'false' level up to display the skill up widget 
			// for the provided skill, as P1 levels up to the new skill level.
			// Modify P1's skill XP briefly to trigger the message
			// and then restore the skill's original level and XP
			// so that no permanent changes are made.
			// Also prevent saving while this occurs because we don't want another player's stats 
			// to be permanently copied over to P1 if the game saves.
			// Return true if successful.

			auto p1 = RE::PlayerCharacter::GetSingleton();
			auto ui = RE::UI::GetSingleton();
			auto hud = ui ? ui->GetMenu<RE::HUDMenu>() : nullptr;
			if (!p1 ||
				!p1->skills ||
				!p1->skills->data || 
				!glob.globalDataInit ||
				!ui ||
				!hud)
			{
				return false;
			}

			{
				std::unique_lock<std::mutex> lock(glob.p1SkillXPMutex, std::try_to_lock);
				if (lock)
				{
					SPDLOG_DEBUG
					(
						"[Util] TriggerFalseSkillLevelUp: Lock obtained. (0x{:X})",
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);

					// Prevent saving during our level changes.
					p1->byCharGenFlag = RE::PlayerCharacter::ByCharGenFlag::kDisableSaving;
					// Save old level, XP, level threshold, and skill data.
					// Will be restored after skill level up triggers.
					const auto oldLevel = p1->GetBaseActorValue(a_avSkill);
					const auto oldLvlXP = p1->skills->data->xp;
					const auto oldLvlThreshold = p1->skills->data->levelThreshold;
					const auto oldSkill = p1->skills->data->skills[a_skill];
					const auto scriptFactory = 
					(
						RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
					);
					const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
					if (script)
					{
						// Lower P1's skill level to the target level - 1,
						// so that when it is incremented via the console command,
						// it will reach the targeted level.
						p1->SetBaseActorValue(a_avSkill, a_newLevel - 1);
						script->SetCommand("incPCS " + a_skillName);
						script->CompileAndRun(p1);

						// Now, restore P1's original skill level,
						// XP, level threshold, and skill data.
						p1->SetBaseActorValue(a_avSkill, oldLevel);
						p1->skills->data->xp = oldLvlXP;
						p1->skills->data->levelThreshold = oldLvlThreshold;
						p1->skills->data->skills[a_skill] = oldSkill;
						// Cleanup.
						delete script;
						
						// Re-enable saving.
						p1->byCharGenFlag = RE::PlayerCharacter::ByCharGenFlag::kNone;
						return true;
					}

					// Re-enable saving.
					p1->byCharGenFlag = RE::PlayerCharacter::ByCharGenFlag::kNone;
				}
				else
				{
					SPDLOG_DEBUG
					(
						"[Util] TriggerFalseSkillLevelUp: Failed to obtain lock. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
				}
			}

			return false;
		}

		RE::NiPoint3 WorldToScreenPoint3(const RE::NiPoint3& a_worldPos, bool&& a_shouldClamp)
		{
			// Get the screen position corresponding to the provided world position,
			// potentially clamping the returned position's components 
			// to fit the screen's dimensions, if requested.

			auto hud = DebugAPI::GetHUD();
			if (!hud || !hud->uiMovie)
			{
				return RE::NiPoint3();
			}

			auto niCamPtr = GetNiCamera(); 
			if (!niCamPtr)
			{
				return RE::NiPoint3();
			}

			RE::NiPoint3 screenPoint{ };
			// Get the debug overlay's dimensions.
			RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
			const float rectWidth = fabsf(gRect.right - gRect.left);
			const float rectHeight = fabsf(gRect.bottom - gRect.top);
			RE::NiRect<float> port{ gRect.left, gRect.right, gRect.top, gRect.bottom };
			// Screen position components.
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			RE::NiCamera::WorldPtToScreenPt3
			(
				niCamPtr->worldToCam, port, a_worldPos, x, y, z, 1e-5f
			);
			
			// Clamp to fit the overlay's dimensions, as needed.
			if (a_shouldClamp) 
			{
				screenPoint.x = std::clamp(x, gRect.left, gRect.right);
				screenPoint.y = std::clamp(y, gRect.top, gRect.bottom);
			}
			else
			{
				screenPoint.x = x;
				screenPoint.y = y;
			}

			screenPoint.z = z;
			return screenPoint;
		}
	}
}

#include "Raycast.h"
#include <Util.h>
namespace Raycast
{
	RayCollector::RayCollector() {}

	void RayCollector::AddRayHit
	(
		const RE::hkpCdBody& a_body, const RE::hkpShapeRayCastCollectorOutput& a_hitInfo
	)
	{
		// After potentially filtering out certain hits, enqueue the hit result.

		HitResult hit{ };
		// Set hit fraction and normal from input info.
		hit.hitFraction = a_hitInfo.hitFraction;
		hit.normal = 
		{
			a_hitInfo.normal.quad.m128_f32[0],
			a_hitInfo.normal.quad.m128_f32[1],
			a_hitInfo.normal.quad.m128_f32[2]
		};

		// Get parent of the hit body.
		const RE::hkpCdBody* obj = &a_body;
		while (obj)
		{
			if (!obj->parent) 
			{
				break;
			}

			obj = obj->parent;
		}

		hit.body = obj;
		// Must have hit a collision detection body.
		if (!hit.body) 
		{
			return;
		}

		// Clear out hit refr.
		hit.hitRefrPtr = RE::TESObjectREFRPtr();
		RE::NiPointer<RE::NiAVObject> hit3DPtr{ hit.GetAVObject() };
		// Get hit object refr.
		// Precedence of checks:
		// 1. Check the hit 3D's user data; fastest way to obtain if valid.
		// 2. Attempt to obtain the hit refr from the collidable.
		// 3. If the hit 3D was invalid, obtain 3D via second method and then check its user data.
		// 4. As a last resort, walk up the node tree and look for a valid parent refr.
		if (hit3DPtr && hit3DPtr->userData)
		{
			hit.hitRefrPtr = RE::TESObjectREFRPtr(hit3DPtr->userData);
		}
		else
		{
			const auto collisionObj = static_cast<const RE::hkpCollidable*>(hit.body);
			hit.hitRefrPtr = RE::TESObjectREFRPtr
			(
				RE::TESHavokUtilities::FindCollidableRef(*collisionObj)
			);
			if (!hit.hitRefrPtr)
			{
				if (!hit3DPtr)
				{
					hit3DPtr = RE::NiPointer<RE::NiAVObject>
					(
						RE::TESHavokUtilities::FindCollidableObject(*collisionObj)
					);
				}

				if (hit3DPtr)
				{
					if (hit3DPtr->userData)
					{
						hit.hitRefrPtr = RE::TESObjectREFRPtr(hit3DPtr->userData);
					}
					else
					{
						hit.hitRefrPtr = RE::TESObjectREFRPtr
						(
							ALYSLC::Util::GetRefrFrom3D(hit3DPtr.get())
						);
					}
				}
			}
		}

		// Is there an associated refr with the hit 3D object?
		if (useOriginalFilter)
		{
			// Use SmoothCam's original collision filter mask here.
			const auto collisionObj = static_cast<const RE::hkpCollidable*>(hit.body);
			const auto flags = collisionObj->broadPhaseHandle.collisionFilterInfo;

			const uint64_t m = 1ULL << static_cast<uint64_t>(flags);
			constexpr uint64_t filter = 0x40122716;	 //@TODO
			if ((m & filter) != 0)
			{
				if (!objectFilters.empty() && hit3DPtr)
				{
					for (const auto filteredObjPtr : objectFilters)
					{
						if (!filteredObjPtr)
						{
							continue;
						}

						// Hit an object that is not in the included list 
						// or hit an object in the excluded list,
						// so skip this hit.
						bool shouldExclude = 
						(
							(isIncludeFilter && hit3DPtr != filteredObjPtr) ||
							(!isIncludeFilter && hit3DPtr == filteredObjPtr)
						);
						if (shouldExclude)
						{
							return;
						}
					
						// Also check and compare the associated hit and filtered refrs 
						// for inclusion or exclusion.
						// If there is a hit refr and it is not an associated refr 
						// for a 3D object in the included list,
						// or if there is a hit refr and it is an associated refr
						// for a 3D object in the excluded list,
						// skip this hit.
						const auto filteredRefr = ALYSLC::Util::GetRefrFrom3D
						(
							filteredObjPtr.get()
						);
						shouldExclude = 
						(
							(
								isIncludeFilter && 
								hit.hitRefrPtr &&
								hit.hitRefrPtr.get() != filteredRefr
							) ||
							(
								!isIncludeFilter && 
								hit.hitRefrPtr &&
								hit.hitRefrPtr.get() == filteredRefr
							)
						);
						if (shouldExclude)
						{
							return;
						}
					}
				}
				
				if (hit.hitRefrPtr && !refrFilters.empty())
				{
					for (const auto filteredRefrPtr : refrFilters)
					{
						if (!filteredRefrPtr)
						{
							continue;
						}

						// Hit a refr that is not in the included list 
						// or hit a refr in the excluded list.
						if ((isIncludeFilter && hit.hitRefrPtr != filteredRefrPtr) ||
							(!isIncludeFilter && hit.hitRefrPtr == filteredRefrPtr))
						{
							return;
						}
					}
				}

				earlyOutHitFraction = hit.hitFraction;
				hits.push_back(std::move(hit));
			}
		}
		else
		{
			// Consider all hits but filter with formtype or object 3D lists exclusively instead.
			// Form type filters.
			if (hit.hitRefrPtr && !formTypesToFilter.empty())
			{
				auto baseObject = hit.hitRefrPtr ? hit.hitRefrPtr->GetBaseObject() : nullptr;
				for (const auto& filteredFormType : formTypesToFilter)
				{
					if (isIncludeFilter)
					{
						// Include this type, so skip over objects of a different form type.
						if ((*hit.hitRefrPtr->formType != filteredFormType) || 
							(baseObject && *baseObject->formType != filteredFormType)) 
						{
							return;
						}
					}
					else
					{
						// Exclude this type, so skip over objects of the same form type.
						if ((*hit.hitRefrPtr->formType == filteredFormType) || 
							(baseObject && *baseObject->formType == filteredFormType))
						{
							return;
						}
					}
				}
			}

			if (!objectFilters.empty() && hit3DPtr)
			{
				for (const auto filteredObjPtr : objectFilters)
				{
					if (!filteredObjPtr)
					{
						continue;
					}

					// Hit an object that is not in the included list 
					// or hit an object in the excluded list,
					// so skip this hit.
					bool shouldExclude = 
					(
						(isIncludeFilter && hit3DPtr != filteredObjPtr) ||
						(!isIncludeFilter && hit3DPtr == filteredObjPtr)
					);
					if (shouldExclude)
					{
						return;
					}
					
					// Also check and compare the associated hit and filtered refrs 
					// for inclusion or exclusion.
					// If there is a hit refr and it is not an associated refr 
					// for a 3D object in the included list,
					// or if there is a hit refr and it is an associated refr
					// for a 3D object in the excluded list,
					// skip this hit.
					const auto filteredRefr = ALYSLC::Util::GetRefrFrom3D(filteredObjPtr.get());
					shouldExclude = 
					(
						(
							isIncludeFilter && 
							hit.hitRefrPtr &&
							hit.hitRefrPtr.get() != filteredRefr
						) ||
						(
							!isIncludeFilter && 
							hit.hitRefrPtr &&
							hit.hitRefrPtr.get() == filteredRefr
						)
					);
					if (shouldExclude)
					{
						return;
					}
				}
			}

			if (hit.hitRefrPtr && !refrFilters.empty())
			{
				for (const auto filteredRefrPtr : refrFilters)
				{
					if (!filteredRefrPtr)
					{
						continue;
					}

					// Hit a refr that is not in the included list 
					// or hit a refr in the excluded list.
					if ((isIncludeFilter && hit.hitRefrPtr != filteredRefrPtr) ||
						(!isIncludeFilter && hit.hitRefrPtr == filteredRefrPtr))
					{
						return;
					}
				}
			}

			// Set hit fraction and then enqueue our hit result.
			earlyOutHitFraction = hit.hitFraction;
			hits.push_back(std::move(hit));
		}
	}

	const std::vector<RayCollector::HitResult>& RayCollector::GetHits()
	{
		// Get a preliminary list of hit results from the completed raycast.

		return hits;
	}

	void RayCollector::Reset()
	{
		// Reset to defaults.

		earlyOutHitFraction = 1.0f;
		isIncludeFilter = false;
		useOriginalFilter = false;
		hits.clear();
		objectFilters.clear();
		formTypesToFilter.clear();
		refrFilters.clear();
	}

	RE::NiAVObject* RayCollector::HitResult::GetAVObject()
	{
		// Get object 3D from the collision detection body.

		return body ? ALYSLC::Util::NativeFunctions::hkpCdBody_GetUserData(body) : nullptr;
	}

	RayCollector* GetCastCollector() noexcept
	{
		// Get the cast collector.

		static RayCollector collector = RayCollector();
		return &collector;
	}

	RayResult CastRay(glm::vec4 a_start, glm::vec4 a_end, float a_traceHullSize) noexcept
	{
		// Cast a ray using the game's camera caster, 
		// which performs a sphere cast of the given hullsize.
		// NOTE: 
		// Does not provide info on the object that was hit, unless it is a character.
		// Occasional int3 raycast crash occurs.

		const auto ply = RE::PlayerCharacter::GetSingleton();
		const auto cam = RE::PlayerCamera::GetSingleton();
		// If P1 is invalid, or no player camera, or no parent cell, 
		// or no cam caster simple shape phantoms, return early.
		if (!ply || !cam || !ply->parentCell || !cam->unk120) 
		{
			return { };
		}

		auto physicsWorld = ply->parentCell->GetbhkWorld();
		if (!physicsWorld)
		{
			return { };
		}
		
		RayResult res{ };
		RE::Character* hitCharacter{ nullptr };
		{
			res.hit = ALYSLC::Util::NativeFunctions::PlayerCamera_LinearCast
			(
				cam->unk120,
				physicsWorld,
				a_start,
				a_end, 
				static_cast<uint32_t*>(res.data), 
				&hitCharacter,
				a_traceHullSize
			);
		}

		if (res.hit)
		{
			res.hitObjectPtr = 
			(
				hitCharacter ? 
				ALYSLC::Util::GetRefr3D(hitCharacter) : 
				nullptr
			);
			// Set hit position and distance cast before the hit.
			res.hitPos = a_end;
			res.rayLength = glm::length(res.hitPos - a_start);
		}

		return res;
	}

	RayResult hkpCastRay
	(
		const glm::vec4& a_start, 
		const glm::vec4& a_end, 
		const bool& a_ignoreActors, 
		const bool& a_onlyActors
	) noexcept
	{
		// Cast ray and either completely ignore hit actors or only consider hit actors.

		constexpr auto hkpScale = 0.0142875f;
		const auto dif = a_end - a_start;

		constexpr auto one = 1.0f;
		const auto from = a_start * hkpScale;
		const auto to = dif * hkpScale;

		auto collector = GetCastCollector();
		collector->Reset();
		
		collector->AddFilteredFormTypes({ RE::FormType::ActorCharacter });
		if (a_ignoreActors)
		{
			collector->SetFilterType(false);
		}
		else if (a_onlyActors)
		{
			collector->SetFilterType(true);
		}

		RE::bhkPickData pickData{ };
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);

		return PerformHavokRaycast(pickData, a_start, dif);
	}

	RayResult hkpCastRay
	(
		const glm::vec4& a_start,
		const glm::vec4& a_end,
		const std::vector<RE::NiAVObject*>& a_filteredObjects,
		RE::COL_LAYER&& a_collisionLayer, 
		bool&& a_defaultFilter
	) noexcept
	{
		// Cast ray, filtering out certain 3D objects 
		// and objects that do not collide with the given collision filter.

		constexpr auto hkpScale = 0.0142875f;
		const auto dif = a_end - a_start;

		constexpr auto one = 1.0f;
		const auto from = a_start * hkpScale;
		const auto to = dif * hkpScale;

		auto collector = GetCastCollector();
		collector->Reset();
		collector->SetUseOriginalFilter(a_defaultFilter);
		// Exclusion filter.
		collector->SetFilterType(false);
		collector->AddFilteredObjects(a_filteredObjects);

		RE::bhkPickData pickData{ };
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);
		// If not using the all-inclusive unidentified layer, set the filter info for the cast.
		if (a_collisionLayer != RE::COL_LAYER::kUnidentified) 
		{
			pickData.rayInput.filterInfo = 
			(
				RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup() << 16 | 
				std::to_underlying(a_collisionLayer)
			);
		}

		return PerformHavokRaycast(pickData, a_start, dif);
	}

	RayResult hkpCastRay
	(
		const glm::vec4& a_start,
		const glm::vec4& a_end, 
		const std::vector<RE::TESObjectREFR*>& a_filteredRefrs, 
		RE::COL_LAYER&& a_collisionLayer,
		bool&& a_defaultFilter
	) noexcept
	{
		// Cast ray, filtering out certain 3D objects 
		// and objects that do not collide with the given collision filter.

		constexpr auto hkpScale = 0.0142875f;
		const auto dif = a_end - a_start;

		constexpr auto one = 1.0f;
		const auto from = a_start * hkpScale;
		const auto to = dif * hkpScale;

		auto collector = GetCastCollector();
		collector->Reset();
		collector->SetUseOriginalFilter(a_defaultFilter);
		// Exclusion filter.
		collector->SetFilterType(false);
		collector->AddFilteredRefrs(a_filteredRefrs);

		RE::bhkPickData pickData{ };
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);
		// If not using the all-inclusive unidentified layer, set the filter info for the cast.
		if (a_collisionLayer != RE::COL_LAYER::kUnidentified) 
		{
			pickData.rayInput.filterInfo = 
			(
				RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup() << 16 |
				std::to_underlying(a_collisionLayer)
			);
		}

		return PerformHavokRaycast(pickData, a_start, dif);
	}

	RayResult hkpCastRay
	(
		const glm::vec4& a_start, 
		const glm::vec4& a_end,
		const std::vector<RE::NiAVObject*>& a_filteredObjects, 
		const std::vector<RE::FormType>& a_filteredFormTypes,
		bool&& a_isIncludeFilter
	) noexcept
	{
		// Cast ray and in/exclusively filter 3D objects and formtypes.

		constexpr auto hkpScale = 0.0142875f;
		const auto dif = a_end - a_start;

		constexpr auto one = 1.0f;
		const auto from = a_start * hkpScale;
		const auto to = dif * hkpScale;

		auto collector = GetCastCollector();
		collector->Reset();
		collector->SetFilterType(a_isIncludeFilter);
		collector->AddFilteredObjects(a_filteredObjects);
		collector->AddFilteredFormTypes(a_filteredFormTypes);

		RE::bhkPickData pickData{ };
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);

		return PerformHavokRaycast(pickData, a_start, dif);
	}

	RayResult hkpCastRay
	(
		const glm::vec4& a_start, 
		const glm::vec4& a_end, 
		const std::vector<RE::TESObjectREFR*>& a_filteredRefrs, 
		const std::vector<RE::FormType>& a_filteredFormTypes,
		bool&& a_isIncludeFilter
	) noexcept
	{
		// Cast ray and in/exclusively filter refrs and formtypes.

		constexpr auto hkpScale = 0.0142875f;
		const auto dif = a_end - a_start;

		constexpr auto one = 1.0f;
		const auto from = a_start * hkpScale;
		const auto to = dif * hkpScale;

		auto collector = GetCastCollector();
		collector->Reset();
		collector->SetFilterType(a_isIncludeFilter);
		collector->AddFilteredRefrs(a_filteredRefrs);
		collector->AddFilteredFormTypes(a_filteredFormTypes);

		RE::bhkPickData pickData{ };
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);

		return PerformHavokRaycast(pickData, a_start, dif);
	}

	RayResult hkpCastRayWithColLayerFilter
	(
		const glm::vec4& a_start, 
		const glm::vec4& a_end, 
		RE::COL_LAYER&& a_collisionLayer
	) noexcept
	{
		// Cast ray filtering out all hits that do not collide with the given collision layer.

		RE::bhkPickData pickData;
		const auto havokWorldScale = RE::bhkWorld::GetWorldScale();
		pickData.rayInput.from = ALYSLC::TohkVector4(a_start) * havokWorldScale;
		pickData.rayInput.to = ALYSLC::TohkVector4(a_end) * havokWorldScale;
		pickData.rayInput.enableShapeCollectionFilter = false;
		// Set filter info to use our collision layer.
		pickData.rayInput.filterInfo = 
		(
			RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup() << 16 | 
			std::to_underlying(a_collisionLayer)
		);

		// Construct hit result.
		// Default data.
		RayResult result{ };
		result.hit = false;
		result.hitObjectPtr = nullptr;
		result.hitRefrHandle = RE::ObjectRefHandle();
		result.hitPos = a_end;
		result.rayLength = glm::length(a_end - a_start);
		result.rayNormal = glm::vec4();
		const auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (!p1 || !p1->parentCell)
		{
			return result;
		}

		auto physicsWorld = p1->parentCell->GetbhkWorld(); 
		if (!physicsWorld) 
		{
			// No physics world, return.
			return result;
		}

		{
			RE::BSReadLockGuard lock(physicsWorld->worldLock);
			physicsWorld->PickObject(pickData);
		}

		if (pickData.rayOutput.HasHit())
		{
			// Set hit flag, position, cast length, and normal.
			result.hit = true;
			const auto fullRay = a_end - a_start;
			result.hitPos = a_start + (fullRay * pickData.rayOutput.hitFraction);
			result.rayLength = glm::length(fullRay * pickData.rayOutput.hitFraction);
			result.rayNormal = ALYSLC::ToVec4(pickData.rayOutput.normal);

			// Set hit object/refr data.
			auto hitCollidable = pickData.rayOutput.rootCollidable;
			auto hitRefr = RE::TESHavokUtilities::FindCollidableRef(*hitCollidable);
			result.hitObjectPtr = hitRefr ? ALYSLC::Util::GetRefr3D(hitRefr) : nullptr;
			result.hitRefrHandle = hitRefr ? hitRefr->GetHandle() : RE::ObjectRefHandle();
		}

		return result;
	}

	std::vector<RayResult> GetAllCamCastHitResults
	(
		const glm::vec4& a_start, const glm::vec4& a_end, const float& a_hullSize
	) noexcept
	{
		// Gets all (most of the time) hits between the start and end positions 
		// using the cam caster with the given hull size.

		const auto ply = RE::PlayerCharacter::GetSingleton();
		if (!ply || !ply->parentCell) 
		{
			return { };
		}

		// Holds our results.
		std::vector<RayResult> results{ };
		results.clear();

		constexpr auto pi = 3.14159265358979323846f;
		constexpr auto hkpScale = 0.0142875f;
		// Result to add, if there's a hit.
		RayResult camCastResult{};
		// Moving raycast start position.
		glm::vec4 newStart{ a_start };
		// Full ray.
		glm::vec4 ray{ a_end - a_start };
		// Normalized ray.
		glm::vec4 unitRay = glm::normalize(ray);
		// Length and length squared for the ray.
		const float rayLength = glm::length(ray);
		const float rayLengthSquared = pow(rayLength, 2.0f);
		// Stop casting if over this many casts.
		// Failsafe for my crappy code.
		// Should never need this many casts anyway,
		// unless the start and end points enclose tons of objects,
		// and probably most of the current worldspace.
		// (if you value your framerate, don't zoom out this far please)
		uint16_t maxCasts = 20;
		uint16_t casts = 0;
		// Stop when the cast start and end positions are the same
		// or when the max number of casts is exceeded.
		while (newStart != a_end && casts < maxCasts)
		{
			if (camCastResult = CastRay(newStart, a_end, a_hullSize); camCastResult.hit)
			{
				// Ray from start to hit pos.
				glm::vec4 hitFromNewStart = camCastResult.hitPos - newStart;
				// Only want hits that are further towards the end position than the previous hit 
				// to ensure that the cast chain eventually ends.
				float angBetweenHitPosAndRay = 0.0f;
				float distFromStart = glm::length(hitFromNewStart); 
				if (distFromStart != 0.0f && rayLength != 0.0f) 
				{
					angBetweenHitPosAndRay = acosf
					(
						glm::dot(hitFromNewStart, ray) / (distFromStart * rayLength)
					); 
				}

				// NOTE: 
				// To prevent subsequent casts from returning the same hit position 
				// and causing the cast chain to stall at this position,
				// we inch the next cast's start position forward by a little over the hull size.
				// Obviously, this could miss hitting some objects that are smaller
				// than the tiny offset. Small price to pay for avoiding a soft lock.
				if (angBetweenHitPosAndRay < pi / 2.0f)
				{
					// Also, all collision normals should be at most 90 degrees 
					// from the reversed ray (from end to start).
					// Only want the normals facing the start position, 
					// since each hit surface has 2 normals to choose from.
					float angBetweenNormAndRayToStart = acosf
					(
						glm::dot(camCastResult.rayNormal, -unitRay)
					); 
					if (angBetweenNormAndRayToStart <= pi / 2.0f)
					{
						// This hit is valid since its normal is facing our start position.
						results.emplace_back(camCastResult);
					}

					// Move a bit over 1 hull's length past the hit point projected onto the ray.
					newStart = 
					(
						newStart + 
						(glm::dot(hitFromNewStart, ray) / rayLengthSquared) * ray + 
						(unitRay * (a_hullSize + 1.0f))
					);
				}
				else
				{
					// Keep moving forward by one hull size and change.
					newStart = newStart + (unitRay * (a_hullSize + 1.0f));
				}

				// Check if next cast's start position is past the end position, 
				// and if so, we're done casting.
				if (glm::length(newStart - a_start) > rayLength)
				{
					newStart = a_end;
				}
			}
			else
			{
				// No hit from this cast's start position to the end position.
				// We've reached the end.
				newStart = a_end;
			}

			++casts;
		}

		return results;
	}

	std::vector<RayResult> GetAllHavokCastHitResults
	(
		const glm::vec4& a_start, 
		const glm::vec4& a_end, 
		const std::vector<RE::NiAVObject*>& a_filteredObjects, 
		const std::vector<RE::FormType>& a_filteredFormTypesList,
		bool&& a_isIncludeFilter
	) noexcept
	{
		// Gets all hits from the start position to the end position,
		// only in/excluding hit objects of the given form types.

		constexpr auto hkpScale = 0.0142875f;
		const auto dif = a_end - a_start;

		constexpr auto one = 1.0f;
		const auto from = a_start * hkpScale;
		const auto to = dif * hkpScale;

		auto collector = GetCastCollector();
		collector->Reset();
		// To include or exclude, that is the question.
		collector->SetFilterType(a_isIncludeFilter);

		RE::bhkPickData pickData{ };
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);
		pickData.rayInput.enableShapeCollectionFilter = false;

		const auto ply = RE::PlayerCharacter::GetSingleton();
		if (!ply || !ply->parentCell) 
		{
			return { };
		}

		// Add filtered objects and form types.
		collector->AddFilteredObjects(a_filteredObjects);
		collector->AddFilteredFormTypes(a_filteredFormTypesList);

		auto physicsWorld = ply->parentCell->GetbhkWorld(); 
		if (!physicsWorld)
		{
			return { };
		}

		{
			RE::BSReadLockGuard lock(physicsWorld->worldLock);
			physicsWorld->PickObject(pickData);
		}

		// Add all results.
		std::vector<RayResult> results{ };
		results.clear();
		for (auto hit : collector->GetHits())
		{
			const auto pos = (dif * hit.hitFraction) + a_start;
			RayResult result{ };
			result.hitPos = pos;
			result.rayLength = glm::length(pos - a_start);
			result.rayNormal = { hit.normal.x, hit.normal.y, hit.normal.z, 0.0f };
			if (hit.body)
			{
				auto av = hit.GetAVObject();
				result.hit = av != nullptr;
				result.hitObjectPtr = av ? RE::NiPointer<RE::NiAVObject>(av) : nullptr;
				result.hitRefrHandle = 
				(
					hit.hitRefrPtr ? hit.hitRefrPtr->GetHandle() : RE::ObjectRefHandle()
				);
			}
			else
			{
				result.hit = false;
				result.hitObjectPtr = nullptr;
				result.hitRefrHandle = RE::ObjectRefHandle();
			}

			results.emplace_back(result);
		}

		// Sort by distance from start position in ascending order.
		std::sort
		(
			results.begin(), results.end(),
			[&a_start](const RayResult& a_result1, const RayResult& a_result2) 
			{
				return 
				(
					glm::distance(a_result1.hitPos, a_start) < 
					glm::distance(a_result2.hitPos, a_start)
				);
			}
		);

		return results;
	}

	RayResult PerformHavokRaycast
	(
		RE::bhkPickData& a_pickDataIn, const glm::vec4& a_start, const glm::vec4& a_rayDir
	)
	{
		// Helper for performing a havok raycast with the given pick data, 
		// ray start position, and ray direction.
		// Returns the closest hit result to the start position.

		const auto ply = RE::PlayerCharacter::GetSingleton();
		// No player or parent cell, return early.
		if (!ply || !ply->parentCell)
		{
			return { };
		}

		auto physicsWorld = ply->parentCell->GetbhkWorld(); 
		if (!physicsWorld)
		{
			return { };
		}
		
		// Perform cast if parent cell havok world is available.
		{
			RE::BSReadLockGuard lock(physicsWorld->worldLock);
			physicsWorld->PickObject(a_pickDataIn);
		}

		// Get closest valid result.
		RayCollector::HitResult best{ };
		best.hitFraction = 1.0f;
		glm::vec4 bestPos{ };
		glm::vec4 normal{ };
		const auto collector = GetCastCollector();
		for (auto& hit : collector->GetHits())
		{
			// Hit position is the ray direction 
			// multiplied by the ray length added to the start pos.
			const auto pos = (a_rayDir * hit.hitFraction) + a_start;
			// Add hits without a hit body that aren't too close to the start position.
			if (best.body == nullptr && glm::distance(pos, a_start) > 1e-2f)
			{
				best = hit;
				bestPos = pos;
				normal = glm::vec4(hit.normal, 0.0f);
				continue;
			}

			// Only consider hits that are closer than the current best hit result
			// and not too close to the start position.
			if (hit.hitFraction < best.hitFraction && glm::distance(pos, a_start) > 1e-2f)
			{
				best = hit;
				bestPos = pos;
				normal = glm::vec4(hit.normal, 0.0f);
			}
		}

		// Construct result with our data.
		RayResult result{ };
		result.hitPos = bestPos;
		result.rayLength = glm::length(bestPos - a_start);
		result.rayNormal = normal;

		// No hit body, so nothing to set below.
		if (!best.body)
		{
			return result;
		}

		// Set as hit and get hit object and refr.
		auto av = best.GetAVObject();
		result.hit = av != nullptr;
		result.hitObjectPtr = av ? RE::NiPointer<RE::NiAVObject>(av) : nullptr;
		result.hitRefrHandle = 
		(
			best.hitRefrPtr ? best.hitRefrPtr->GetHandle() : RE::ObjectRefHandle()
		);

		return result;
	}
};

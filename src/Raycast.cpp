#include "Raycast.h"
#include <Util.h>
namespace Raycast
{
	RayCollector::RayCollector() {}

	void RayCollector::AddRayHit(const RE::hkpCdBody& a_body, const RE::hkpShapeRayCastCollectorOutput& a_hitInfo)
	{
		// After potentially filtering out certain hits, enqueue the hit result.

		HitResult hit;
		// Set hit fraction and normal from output info.
		hit.hitFraction = a_hitInfo.hitFraction;
		hit.normal = 
		{
			a_hitInfo.normal.quad.m128_f32[0],
			a_hitInfo.normal.quad.m128_f32[1],
			a_hitInfo.normal.quad.m128_f32[2]
		};

		// Get root parent of the hit body.
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
		// If there's no hit collision detection body, return early.
		if (!hit.body) 
		{
			return;
		}

		const auto collisionObj = static_cast<const RE::hkpCollidable*>(hit.body);
		// Get hit object refr, if any.
		hit.hitRefr = RE::TESHavokUtilities::FindCollidableRef(*collisionObj);
		
		if (useOriginalFilter)
		{
			// Use SmoothCam's original collision filter mask here.
			const auto collisionObj = static_cast<const RE::hkpCollidable*>(hit.body);
			const auto flags = collisionObj->broadPhaseHandle.collisionFilterInfo;

			const uint64_t m = 1ULL << static_cast<uint64_t>(flags);
			constexpr uint64_t filter = 0x40122716;	 //@TODO
			if ((m & filter) != 0)
			{
				if (objectFilters.size() > 0)
				{
					for (const auto filteredObj : objectFilters)
					{
						if (hit.GetAVObject() == filteredObj) 
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
			// Allow all hits through but filter with formtype or object 3D lists exclusively instead.
			// Form type filters.
			if (!formTypesToFilter.empty())
			{
				for (const auto& filteredFormType : formTypesToFilter)
				{
					if (auto hitRefrPtr = RE::TESObjectREFRPtr(hit.hitRefr); hitRefrPtr)
					{
						if (isIncludeFilter)
						{
							// Include this type, so skip over objects of a different form type.
							if (*hitRefrPtr->formType != filteredFormType || (hitRefrPtr->GetBaseObject() && *hitRefrPtr->GetBaseObject()->formType != filteredFormType)) 
							{
								return;
							}
						}
						else
						{
							// Exclude this type, so skip over objects of the same form type.
							if (*hit.hitRefr->formType == filteredFormType || (hit.hitRefr->GetBaseObject() && *hit.hitRefr->GetBaseObject()->formType == filteredFormType))
							{
								return;
							}
						}
					}
				}
			}

			if (objectFilters.size() > 0)
			{
				for (const auto filteredObj : objectFilters)
				{
					RE::NiPointer<RE::NiAVObject> hit3D{ hit.GetAVObject() };
					// Hit an object that is not in the included list 
					// or hit an object in the excluded list.
					if ((isIncludeFilter && hit3D.get() != filteredObj) ||
						(!isIncludeFilter && hit3D.get() == filteredObj))
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
	}

	RE::NiAVObject* RayCollector::HitResult::GetAVObject()
	{
		// Get object 3D from collision detection body.
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
		// Cast a ray using the game's camera caster, which is a sphere cast of the given hullsize.
		// NOTE: Does not provide info on the object that was hit.

		RayResult res{ };
		const auto ply = RE::PlayerCharacter::GetSingleton();
		const auto cam = RE::PlayerCamera::GetSingleton();
		// If P1 is invalid, or no player camera, or no parent cell, or cam caster simple shape phantoms, return early.
		if (!ply || !cam || !ply->parentCell || !cam->unk120) 
		{
			return res;
		}

		auto physicsWorld = ply->parentCell->GetbhkWorld();
		if (physicsWorld)
		{
			{
				RE::BSReadLockGuard lock(physicsWorld->worldLock);
				RE::NiAVObject* object3D{ nullptr };
				res.hit = ALYSLC::Util::NativeFunctions::PlayerCamera_LinearCast
				(
					cam->unk120, physicsWorld,
					a_start, a_end, 
					static_cast<uint32_t*>(res.data), &object3D,
					a_traceHullSize
				);

				if (res.hit)
				{
					// Does not seem to ever modify object3D during the cast.
					res.hitObject = 
					(
						object3D && object3D->GetRefCount() > 0 ? 
						RE::NiPointer<RE::NiAVObject>(object3D) : 
						nullptr
					);
					// Set hit position and distance cast before the hit.
					res.hitPos = a_end;
					res.rayLength = glm::length(static_cast<glm::vec3>(res.hitPos) - static_cast<glm::vec3>(a_start));
				}
			}
		}

		return res;
	}

	RayResult hkpCastRay(const glm::vec4& a_start, const glm::vec4& a_end, const bool& a_ignoreActors, const bool& a_onlyActors) noexcept
	{
		// Cast ray either completely ignoring actors or only hitting actors.

		constexpr auto hkpScale = 0.0142875f;
		const auto dif = a_end - a_start;

		constexpr auto one = 1.0f;
		const auto from = a_start * hkpScale;
		const auto to = dif * hkpScale;

		auto collector = GetCastCollector();
		collector->Reset();

		if (a_ignoreActors)
		{
			collector->SetFilterType(false);
			collector->AddFilteredFormTypes({ RE::FormType::ActorCharacter });
		}
		else if (a_onlyActors)
		{
			collector->SetFilterType(true);
			collector->AddFilteredFormTypes({ RE::FormType::ActorCharacter });
		}

		RE::bhkPickData pickData{};
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);

		return PerformHavokCast(pickData, a_start, dif);
	}

	RayResult hkpCastRay(const glm::vec4& a_start, const glm::vec4& a_end, const std::vector<RE::NiAVObject*>& a_filteredObjects, RE::COL_LAYER&& a_collisionLayer, bool&& a_defaultFilter) noexcept
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
		collector->SetFilterType(false);

		// Add all objects to filter out.
		for (const auto filtered3D : a_filteredObjects)
		{
			collector->AddFilteredObject(filtered3D);
		}

		RE::bhkPickData pickData{};
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);
		// If not using the all-inclusive unidentified layer, set the filter info for the cast.
		if (a_collisionLayer != RE::COL_LAYER::kUnidentified) 
		{
			pickData.rayInput.filterInfo = RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup() << 16 | std::to_underlying(a_collisionLayer);
		}

		return PerformHavokCast(pickData, a_start, dif);
	}

	RayResult hkpCastRay(const glm::vec4& a_start, const glm::vec4& a_end, const std::vector<RE::NiAVObject*>& a_filteredObjects, const std::vector<RE::FormType>& a_filteredFormTypes, bool&& a_isIncludeFilter) noexcept
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

		for (const auto filtered3D : a_filteredObjects)
		{
			collector->AddFilteredObject(filtered3D);
		}

		if (!a_filteredFormTypes.empty()) 
		{
			collector->AddFilteredFormTypes(a_filteredFormTypes);
		}


		RE::bhkPickData pickData{};
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);

		return PerformHavokCast(pickData, a_start, dif);
	}

	RayResult hkpCastRayWithColLayerFilter(const glm::vec4& a_start, const glm::vec4& a_end, RE::COL_LAYER&& a_collisionLayer) noexcept
	{
		// Cast ray filtering out all hits that do not collide with the given collision layer.

		RE::bhkPickData pickData;
		const auto havokWorldScale = RE::bhkWorld::GetWorldScale();
		pickData.rayInput.from = ALYSLC::TohkVector4(a_start) * havokWorldScale;
		pickData.rayInput.to = ALYSLC::TohkVector4(a_end) * havokWorldScale;
		pickData.rayInput.enableShapeCollectionFilter = false;
		// Set filter info to use our collision layer.
		pickData.rayInput.filterInfo = RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup() << 16 | std::to_underlying(a_collisionLayer);

		// Construct hit result.
		// Default data.
		RayResult result;
		result.hit = false;
		result.hitObject = nullptr;
		result.hitRefrHandle = RE::ObjectRefHandle();
		result.hitPos = a_end;
		result.rayLength = glm::length(a_end - a_start);
		result.rayNormal = glm::vec4();
		if (const auto p1 = RE::PlayerCharacter::GetSingleton(); p1 && p1->parentCell)
		{
			if (auto physicsWorld = p1->parentCell->GetbhkWorld(); physicsWorld) 
			{
				RE::BSReadLockGuard lock(physicsWorld->worldLock);
				physicsWorld->PickObject(pickData);
			}
			else
			{
				// No physics world, return.
				return result;
			}

			if (pickData.rayOutput.HasHit())
			{
				// Set hit flag, position, normal, ray data.
				result.hit = true;
				const auto fullRay = a_end - a_start;
				result.hitPos = a_start + (fullRay * pickData.rayOutput.hitFraction);
				result.rayLength = glm::length(fullRay * pickData.rayOutput.hitFraction);
				result.rayNormal = ALYSLC::ToVec4(pickData.rayOutput.normal);

				// Set hit object/refr data.
				auto hitCollidable = pickData.rayOutput.rootCollidable;
				auto hitRefr = RE::TESHavokUtilities::FindCollidableRef(*hitCollidable);
				result.hitObject = hitRefr ? ALYSLC::Util::GetRefr3D(hitRefr) : nullptr;
				result.hitRefrHandle = hitRefr ? hitRefr->GetHandle() : RE::ObjectRefHandle();
			}
		}

		return result;
	}

	std::vector<RayResult> GetAllCamCastHitResults(const glm::vec4& a_start, const glm::vec4& a_end, const float& a_hullSize) noexcept
	{
		// Gets all (most of the time) hits between the start and end positions using the cam caster with the given hull size.

		const auto ply = RE::PlayerCharacter::GetSingleton();
		if (!ply || !ply->parentCell) 
		{
			return {};
		}

		// Holds our results.
		std::vector<RayResult> results;
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
		// and probably most of Skyrim's worldspace.
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
				if (float distFromStart = glm::length(hitFromNewStart); distFromStart != 0.0f && rayLength != 0.0f) 
				{
					angBetweenHitPosAndRay = acosf(glm::dot(hitFromNewStart, ray) / (distFromStart * rayLength)); 
				}

				// NOTE: To prevent subsequent casts from returning the same hit position 
				// and causing the cast chain to stall at this position,
				// we inch the next cast's start position forward by a little over the hull size.
				// Obviously, this could miss hitting some objects that are smaller than the tiny offset.
				// Small price to pay for avoiding a soft lock.
				if (angBetweenHitPosAndRay < pi / 2.0f)
				{
					// Also, all collision normals should be at most 90 degrees from the reversed ray (from end to start).
					// Only want the normals facing the start position, since each hit surface has 2 normals to choose from.
					float angBetweenNormAndRayToStart = acosf(glm::dot(camCastResult.rayNormal, -unitRay)); 
					if (angBetweenNormAndRayToStart <= pi / 2.0f)
					{
						// This hit is valid since its normal is facing our start position.
						results.emplace_back(camCastResult);
						newStart = newStart + (glm::dot(hitFromNewStart, ray) / rayLengthSquared) * ray + (unitRay * (a_hullSize + 1.0f));
					}

					// Move a bit over 1 hull's length past the hit point projected onto the ray.
					newStart = newStart + (glm::dot(hitFromNewStart, ray) / rayLengthSquared) * ray + (unitRay * (a_hullSize + 1.0f));
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
				// We've reached the end, well done.
				newStart = a_end;
			}

			++casts;
		}

		return results;
	}

	std::vector<RayResult> GetAllHavokCastHitResults(const glm::vec4& a_start, const glm::vec4& a_end, const std::vector<RE::FormType>& a_filteredFormTypesList, bool&& a_isIncludeFilter) noexcept
	{
		// Gets all hits from the start position to the end position, only in/excluding hit objects of the given form types.

		constexpr auto hkpScale = 0.0142875f;
		const auto dif = a_end - a_start;

		constexpr auto one = 1.0f;
		const auto from = a_start * hkpScale;
		const auto to = dif * hkpScale;

		auto collector = GetCastCollector();
		collector->Reset();
		// To include or exclude, that is the question.
		collector->SetFilterType(a_isIncludeFilter);

		RE::bhkPickData pickData{};
		pickData.rayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
		pickData.rayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);
		pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
		pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);
		pickData.rayInput.enableShapeCollectionFilter = false;

		const auto ply = RE::PlayerCharacter::GetSingleton();
		if (!ply || !ply->parentCell) 
		{
			return {};
		}

		// Add out filtered form types.
		if (!a_filteredFormTypesList.empty())
		{
			collector->AddFilteredFormTypes(a_filteredFormTypesList);
		}

		auto physicsWorld = ply->parentCell->GetbhkWorld(); 
		if (!physicsWorld)
		{
			return {};
		}
		else
		{
			RE::BSReadLockGuard lock(physicsWorld->worldLock);
			physicsWorld->PickObject(pickData);
		}

		// Add all results.
		std::vector<RayResult> results;
		results.clear();
		for (auto hit : collector->GetHits())
		{
			const auto pos = (dif * hit.hitFraction) + a_start;
			RayResult result;
			result.hitPos = pos;
			result.rayLength = glm::length(pos - a_start);
			result.rayNormal = { hit.normal.x, hit.normal.y, hit.normal.z, 0.0f };

			if (hit.body)
			{
				auto av = hit.GetAVObject();
				result.hit = av != nullptr;
				result.hitObject = av ? RE::NiPointer<RE::NiAVObject>(av) : nullptr;
				result.hitRefrHandle = hit.hitRefr ? hit.hitRefr->GetHandle() : RE::ObjectRefHandle();
			}
			else
			{
				result.hit = false;
				result.hitObject = nullptr;
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
				return glm::distance(a_result1.hitPos, a_start) < glm::distance(a_result2.hitPos, a_start);
			}
		);

		return results;
	}

	RayResult PerformHavokCast(RE::bhkPickData& a_pickDataIn, const glm::vec4& a_start, const glm::vec4& a_rayDir)
	{
		// Helper for performing a havok raycast with the given pick data, ray start position, and ray direction.
		// Returns the closest hit result to the start position.

		const auto ply = RE::PlayerCharacter::GetSingleton();
		// No player or parent cell, return early.
		if (!ply || !ply->parentCell)
		{
			return {};
		}

		// Perform cast if parent cell havok world is available.
		if (auto physicsWorld = ply->parentCell->GetbhkWorld(); physicsWorld)
		{
			RE::BSReadLockGuard lock(physicsWorld->worldLock);
			physicsWorld->PickObject(a_pickDataIn);
		}

		// Get closest valid result.
		RayCollector::HitResult best{};
		best.hitFraction = 1.0f;
		glm::vec4 bestPos = {};
		glm::vec4 normal = {};
		const auto collector = GetCastCollector();
		for (auto& hit : collector->GetHits())
		{
			// Hit position is the ray direction multiplied by the ray length added to the start pos.
			const auto pos = (a_rayDir * hit.hitFraction) + a_start;
			// Add hits without a hit body that aren't too close to the start position.
			if (best.body == nullptr && glm::distance(pos, a_start) > 1e-2f)
			{
				best = hit;
				bestPos = pos;
				normal = glm::vec4(hit.normal, 0.0f);
				continue;
			}

			// Ignore hits that are closer than the current best hit result
			// or too close to the start position.
			if (hit.hitFraction < best.hitFraction && glm::distance(pos, a_start) > 1e-2f)
			{
				best = hit;
				bestPos = pos;
				normal = glm::vec4(hit.normal, 0.0f);
			}
		}

		// Construct result with our data.
		RayResult result;
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
		result.hitObject = av ? RE::NiPointer<RE::NiAVObject>(av) : nullptr;
		result.hitRefrHandle = best.hitRefr ? best.hitRefr->GetHandle() : RE::ObjectRefHandle();
		return result;
	}
};

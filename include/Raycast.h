#pragma once

// Copied and slightly modified from:
// https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/include/raycast.h
// Full credits to mwilsnd.
namespace Raycast
{
	struct hkpGenericShapeData
	{
		intptr_t* unk;
		uint32_t shapeType;
	};

	struct RayHitShapeInfo
	{
		hkpGenericShapeData* hitShape;
		uint32_t unk;
	};

	class RayCollector
	{
	public:
		struct HitResult
		{
			// Normal ray to the hit surface.
			glm::vec3 normal;
			// Fraction along the raycast at which the hit occurred.
			// [0.0, 1.0], ray start position to ray end position.
			float hitFraction;
			// Hit body.
			const RE::hkpCdBody* body;
			// Hit refr derived from hit body.
			RE::TESObjectREFR* hitRefr;
			// Get hit 3D object.
			RE::NiAVObject* GetAVObject();
		};

	public:
		RayCollector();
		~RayCollector() = default;

		// Add hit result information to the hit result queue based on the given hit body and raycast output hit info.
		virtual void AddRayHit(const RE::hkpCdBody& a_body, const RE::hkpShapeRayCastCollectorOutput& a_hitInfo);

		// Add a 3D object to filter out from hit results.
		inline void AddFilteredObject(const RE::NiAVObject* a_obj) noexcept
		{
			objectFilters.push_back(a_obj);
		}

		// Add formtype(s) to either filter out or exclusively include in hit results.
		inline void AddFilteredFormTypes(const std::vector<RE::FormType>& a_formTypesList)
		{
			formTypesToFilter = a_formTypesList;
		}

		inline void SetFilterType(const bool& a_include) 
		{
			isIncludeFilter = a_include;
		}

		// Use the original SmoothCam collision filter when adding hit results.
		// Otherwise, any given formtype and object filters will be used.
		inline void SetUseOriginalFilter(const bool& a_set) 
		{
			useOriginalFilter = a_set;
		}

		// Get all hit results.
		const std::vector<HitResult>& GetHits();

		// Reset ray collector data.
		void Reset();

	private:
		// Fraction of ray's length traveled before hit.
		// [0.0, 1.0]
		float earlyOutHitFraction{ 1.0f };	// 08
		// Are the object/formtypes filters an inclusion or exclusion filter?
		bool isIncludeFilter = false;
		// Use SmoothCam's original collision filter info when filtering hit objects.
		bool useOriginalFilter = false;
		// Output data for raycast.
		RE::hkpWorldRayCastOutput rayHit;  // 10
		std::uint32_t pad0C{};

		// Hits from the performed raycast.
		std::vector<HitResult> hits{};
		// Hit 3D objects to include/exclude.
		std::vector<const RE::NiAVObject*> objectFilters;
		// Formtypes for hit objects to include/exclude.
		std::vector<RE::FormType> formTypesToFilter;
	};

#pragma warning(push)
#pragma warning(disable : 26495)
	typedef __declspec(align(16)) struct RayResult
	{
		union
		{
			uint32_t data[16];
			struct
			{
				// Might be surface normal.
				glm::vec4 rayUnkPos;
				// The normal vector of the ray.
				glm::vec4 rayNormal;

				RayHitShapeInfo colA;
				RayHitShapeInfo colB;
			};
		};

		// True if the trace hit something before reaching its end position.
		bool hit = false;
		// If the ray hit an AV object, this will point to it.
		RE::NiPointer<RE::NiAVObject> hitObject = nullptr;
		// Reference handle for the hit object, if any.
		RE::ObjectRefHandle hitRefrHandle{ };
		// The length of the ray from start to hitPos.
		float rayLength = 0.0f;
		// The position the ray hit, in world space.
		glm::vec4 hitPos{ };

		// pad to 128
		uint64_t _pad1{ };
		uint64_t _pad2{ };
		uint64_t _pad3{ };
	} RayResult;
	static_assert(sizeof(RayResult) == 128);
#pragma warning(pop)

	// Get the ray collector.
	RayCollector* GetCastCollector() noexcept;

	// Cast a ray from 'a_start' to 'a_end', returning the first thing it hits.
	// This variant is used by the camera to test for clipping during positional changes.
	// Params:
	//		glm::vec4 a_start:		Starting position for the trace in world space.
	//		glm::vec4 a_end:		End position for the trace in world space.
	//		float a_traceHullSize:	Size of the collision hull used for the trace.
	//
	// Returns:
	//		RayResult:
	//		A structure holding the results of the ray cast.
	//		If the ray hit something, result.hit will be true.
	RayResult CastRay(glm::vec4 a_start, glm::vec4 a_end, float a_traceHullSize) noexcept;

	// Cast a ray from 'a_start' to 'a_end', returning the first filtered thing it hits
	// If both actor filter flags are false, this variant collides with pretty much any solid geometry.
	// Params:
	//		glm::vec4 a_start:		Starting position for the trace in world space.
	//		glm::vec4 a_end:		End position for the trace in world space.
	//		bool a_ignoreActors:	Filter out collisions with actors.
	//		bool a_onlyActors:		Filter out all collisions, except for those with actors.
	// 
	// Returns:
	//		RayResult:
	//		A structure holding the results of the ray cast.
	//		If the ray hit something, result.hit will be true.
	RayResult hkpCastRay(const glm::vec4& a_start, const glm::vec4& a_end, const bool& a_ignoreActors = false, const bool& a_onlyActors = false) noexcept;
	
	// Cast a ray from 'a_start' to 'a_end', returning the first thing it hits
	// This variant allows for filtering of specific objects 
	// and objects with collision layers that do not collide with the passed-in collision layer.
	// Params:
	//		glm::vec4 a_start:									Starting position for the trace in world space.
	//		glm::vec4 a_end:									End position for the trace in world space.
	//		std::vector<RE::NiAVObject*> a_filteredObjects:		Filtered 3D objects to not consider for collisions.
	//		RE::COL_LAYER a_collisionLayer:						Collision layer to use for the raycast. Filters out objects that do not collide with this layer.
	//		bool a_defaultFilter:								Use SmoothCam's default collision filter when filtering out hit objects.
	// 
	// Returns:
	//		RayResult:
	//		A structure holding the results of the ray cast.
	//		If the ray hit something, result.hit will be true.
	RayResult hkpCastRay(const glm::vec4& a_start, const glm::vec4& a_end, const std::vector<RE::NiAVObject*>& a_filteredObjects, RE::COL_LAYER&& a_collisionLayer = RE::COL_LAYER::kUnidentified, bool&& a_defaultFilter = false) noexcept;
	
	// Cast a ray from 'a_start' to 'a_end', returning the first filtered thing it hits.
	// This variant allows for inclusive/exclusive filtering of specific objects and form types.
	// Params:
	//		glm::vec4 a_start:									Starting position for the trace in world space.
	//		glm::vec4 a_end:									End position for the trace in world space.
	//		std::vector<RE::NiAVObject*> a_filteredObjects:		Filtered 3D objects to only/not consider for collisions.
	//		std::vector<RE::FormType> a_filteredFormTypes:		Filtered formtypes for hit objects to only/not consider for collisions.
	//		bool a_isIncludeFilter:								Objects/formtypes in the lists provided should be exclusively included in/excluded from hit results.
	// 
	// Returns:
	//		RayResult:
	//		A structure holding the results of the ray cast.
	//		If the ray hit something, result.hit will be true.
	RayResult hkpCastRay(const glm::vec4& a_start, const glm::vec4& a_end, const std::vector<RE::NiAVObject*>& a_filteredObjects, const std::vector<RE::FormType>& a_filteredFormTypes = {}, bool&& a_isIncludeFilter = false) noexcept;

	// Cast a ray from 'a_start' to 'a_end', returning the first filtered thing it hits.
	// This variant allows for filtering out of all objects that do not collide with the given collision layer.
	// Params:
	//		glm::vec4 a_start:					Starting position for the trace in world space.
	//		glm::vec4 a_end:					End position for the trace in world space.
	//		RE::COL_LAYER a_collisionLayer:		Collision layer to use for the raycast. Filters out objects that do not collide with this layer.
	// 
	// Returns:
	//		RayResult:
	//		A structure holding the results of the ray cast.
	//		If the ray hit something, result.hit will be true.
	RayResult hkpCastRayWithColLayerFilter(const glm::vec4& a_start, const glm::vec4& a_end, RE::COL_LAYER&& a_collisionLayer) noexcept;

	// Get all camera raycast hit results sorted by distance from the cast's start point in ascending order.
	// Params:
	//		glm::vec4 a_start:		Starting position for the trace in world space
	//		glm::vec4 a_end:		End position for the trace in world space
	//		float a_hullSize:		Size of the collision hull used for the trace.
	// 
	// Returns:
	//		RayResult:
	//		A structure holding the results of the ray cast.
	//		If the ray hit something, result.hit will be true.
	std::vector<RayResult> GetAllCamCastHitResults(const glm::vec4& a_start, const glm::vec4& a_end, const float& a_hullSize) noexcept;

	// Get all havok raycast hit results sorted by distance from the cast's start point in ascending order.
	// Allows for inclusive or exclusive filtering of specific formtypes.
	// Params:
	//		glm::vec4 a_start:									Starting position for the trace in world space
	//		glm::vec4 a_end:									End position for the trace in world space
	//		std::vector<RE::FormType> a_filteredFormTypes:		Filtered formtypes for hit objects to only/not consider for collisions.
	//		bool a_isIncludeFilter:								Formtypes in the list provided should be exclusively included in or excluded from hit results.
	//
	// Returns:
	//		RayResult:
	//		A structure holding the results of the ray cast.
	//		If the ray hit something, result.hit will be true.
	std::vector<RayResult> GetAllHavokCastHitResults(const glm::vec4& a_start, const glm::vec4& a_end, const std::vector<RE::FormType>& a_filteredFormTypesList = {}, bool&& a_isIncludeFilter = false) noexcept;

	// Helper for performing havok raycasts with a collector.
	RayResult PerformHavokCast(RE::bhkPickData& a_pickDataIn, const glm::vec4& a_start, const glm::vec4& a_rayDir);
}

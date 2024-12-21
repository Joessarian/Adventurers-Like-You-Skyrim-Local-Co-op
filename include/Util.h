#pragma once
#include <Compatibility.h>
#include <DebugAPI.h>
#include <Enums.h>
#include <Raycast.h>
#include <chrono>
#include <complex>

//==============
//[Conversions]:
//==============

// Convert from the game's units to havok units and vice versa.
#define GAME_TO_HAVOK		(0.0142875f)
#define HAVOK_TO_GAME		(69.99125f)
// I'd like some pi.
#define PI (3.14159265358979323846f)
// Convert radians to degrees and vice versa.
#define TO_DEGREES			(180.0f / PI)
#define TO_RADIANS			(PI / 180.0f)

//==============
//[Global Data]:
//==============
// Max number of players supported.
static constexpr uint8_t ALYSLC_MAX_PLAYER_COUNT = 4;

// Max number of recursion calls to execute when traversing a node tree.
static constexpr uint8_t MAX_NODE_RECURSION_DEPTH = 100;

// All credits go to ersh1:
// https://github.com/ersh1/Precision/blob/main/src/Offsets.h
static float* g_deltaTimeRealTime = (float*)RELOCATION_ID(523661, 410200).address();	// 2F6B94C, 30064CC


//==========
//[Hashing]:
//==========

// Full credits to Bingle, ersh1, and dTry.
// Used to hash strings for insertion into sets/maps and for comparison operations.
// TODO: Remove unnecessary calls when comparing BSFixedString's.
inline constexpr uint32_t Hash(const char* a_data, size_t const a_size) noexcept
{
	uint32_t hash = 5381;

	for (const char* c = a_data; c < a_data + a_size; ++c)
	{
		hash = ((hash << 5) + hash) + (unsigned char)*c;
	}

	return hash;
}

inline const uint32_t Hash(const char* a_data) noexcept
{
	uint32_t hash = 5381;

	for (const char* c = a_data; c < a_data + strlen(a_data); ++c)
	{
		hash = ((hash << 5) + hash) + (unsigned char)*c;
	}

	return hash;
}

inline const uint32_t Hash(const RE::BSFixedString& a_string) noexcept
{
	uint32_t hash = 5381;

	for (const char* c = a_string.data(); c < a_string.data() + a_string.size(); ++c)
	{
		hash = ((hash << 5) + hash) + (unsigned char)*c;
	}

	return hash;
}


inline const uint32_t Hash(const std::string& a_string) noexcept
{
	uint32_t hash = 5381;

	for (const char* c = a_string.data(); c < a_string.data() + a_string.size(); ++c)
	{
		hash = ((hash << 5) + hash) + (unsigned char)*c;
	}

	return hash;
}

inline constexpr uint32_t operator"" _h(const char* a_str, size_t a_size) noexcept
{
	return Hash(a_str, a_size);
}

//==========================
//[Hashing and Comparators]:
//==========================

// Hash NiAVObjects wrapped in NiPointers.
// For use in sets and maps.
template <>
struct std::hash<RE::NiPointer<RE::NiAVObject>>
{
	std::size_t operator()(const RE::NiPointer<RE::NiAVObject>& a_ptr) const
	{
		if (a_ptr) 
		{
			return std::hash<RE::NiAVObject*>()(a_ptr.get());
		}
		else
		{
			return std::hash<RE::NiAVObject*>()(nullptr);
		}
	}
};

struct NiPointerNiAVObjectComp
{
	bool operator()(const RE::NiPointer<RE::NiAVObject>& a_lhs, const RE::NiPointer<RE::NiAVObject>& a_rhs) const
	{
		return 
		(
			std::hash<RE::NiPointer<RE::NiAVObject>>()(a_lhs) < 
			std::hash<RE::NiPointer<RE::NiAVObject>>()(a_rhs)
		);
	}
};

// For hashing ActorHandles/ObjectRefHandles.
// Full credits again to ersh1:
// https://github.com/ersh1/Precision/blob/main/src/PCH.h#L39
template <class T>
struct std::hash<RE::BSPointerHandle<T>>
{
	uint32_t operator()(const RE::BSPointerHandle<T>& a_handle) const
	{
		uint32_t nativeHandle = const_cast<RE::BSPointerHandle<T>*>(&a_handle)->native_handle();  // ugh
		return nativeHandle;
	}
};

template <class T>
struct HandleComp
{
	bool operator()(const RE::BSPointerHandle<T>& a_lhs, const RE::BSPointerHandle<T>& a_rhs) const
	{
		return std::hash<RE::BSPointerHandle<T>>()(a_lhs) < std::hash<RE::BSPointerHandle<T>>()(a_rhs);
	}
};

template <>
struct std::hash<std::pair<RE::FormID, RE::FormID>>
{
	// Have to fit in two FIDs with one shifted over to the upper 32 bits of the size_t.
	static_assert(((1 << (sizeof(size_t) * 8 - 1))) >= (1 << (sizeof(RE::FormID) * 8 * 2 - 1)));
	std::size_t operator()(const std::pair<RE::FormID, RE::FormID>& a_fidPair) const
	{
		return (static_cast<size_t>(a_fidPair.first) << 32) + static_cast<size_t>(a_fidPair.second);
	}
};

struct FormIDPairComp
{
	bool operator()(const std::pair<RE::FormID, RE::FormID>& a_lhs, const std::pair<RE::FormID, RE::FormID>& a_rhs) const
	{
		return std::hash<std::pair<RE::FormID, RE::FormID>>()(a_lhs) < std::hash<std::pair<RE::FormID, RE::FormID>>()(a_rhs);
	}
};

template <>
struct std::hash<RE::BSFixedString>
{
	std::size_t operator()(const RE::BSFixedString& a_string) const
	{
		return Hash(a_string);
	}
};

struct BSFixedStringComp
{
	bool operator()(const RE::BSFixedString& a_lhs, const RE::BSFixedString& a_rhs) const
	{
		return std::hash<RE::BSFixedString>()(a_lhs) < std::hash<RE::BSFixedString>()(a_rhs);
	}
};


namespace ALYSLC
{
	using Skill = RE::PlayerCharacter::PlayerSkills::Data::Skill;
	using SkillList = std::array<float, (size_t)RE::PlayerCharacter::PlayerSkills::Data::Skill::kTotal>;
	using SteadyClock = std::chrono::steady_clock;

	//========
	//[Enums]:
	//========
	
	// '!' operator overloaded.
	// Convert integral enum types to underlying integral value.
	// Useful when trying to use an enum type to index into a container.
	template <typename T>
	inline constexpr std::underlying_type_t<T> operator!(const T& a_data)
	{
		static_assert(std::is_enum_v<T>, "ERR: Not an enum type. Cannot get underlying type.");
		return std::to_underlying(a_data);
	}

	//================================
	//[Point/vector type conversions]:
	//================================
	//
	// Option to normalize before returning as well.
	//

	inline RE::hkVector4 TohkVector4(const RE::NiPoint3& a_point, bool&& a_normalize = false)
	{
		RE::hkVector4 vec4{};
		vec4.quad = _mm_setr_ps(a_point.x, a_point.y, a_point.z, 0.0f);
		if (a_normalize)
		{
			if (float length = vec4.Length3(); length == 0.0f)
			{
				return vec4;
			}
			else
			{
				return vec4 / length;
			}
		}
		else
		{
			return vec4;
		}
	}

	inline RE::hkVector4 TohkVector4(const glm::vec3& a_vec, bool&& a_normalize = false)
	{
		RE::hkVector4 vec4{};
		vec4.quad = _mm_setr_ps(a_vec.x, a_vec.y, a_vec.z, 0.0f); 
		if (a_normalize) 
		{
			if (float length = vec4.Length3(); length == 0.0f) 
			{
				return vec4;
			}
			else
			{
				return vec4 / length;
			}
		}
		else
		{
			return vec4;
		}
	}

	inline RE::hkVector4 TohkVector4(const glm::vec4& a_vec, bool&& a_normalize = false)
	{
		return TohkVector4(glm::vec3(a_vec), std::move(a_normalize));
	}

	inline RE::NiPoint3 ToNiPoint3(const RE::hkVector4& a_vec, bool&& a_normalize = false)
	{
		if (a_normalize) 
		{
			if (float length = a_vec.Length3(); length == 0.0f) 
			{
				return RE::NiPoint3();
			}
			else
			{
				return RE::NiPoint3(a_vec.quad.m128_f32[0], a_vec.quad.m128_f32[1], a_vec.quad.m128_f32[2]) / length;
			}
		}
		else
		{
			return { a_vec.quad.m128_f32[0], a_vec.quad.m128_f32[1], a_vec.quad.m128_f32[2] };
		}
	}

	inline RE::NiPoint3 ToNiPoint3(const glm::vec3& a_vec, bool&& a_normalize = false)
	{
		if (a_normalize) 
		{
			if (glm::length(a_vec) == 0.0f) 
			{
				return RE::NiPoint3();
			}
			else
			{
				auto temp = glm::normalize(a_vec);
				return { temp.x, temp.y, temp.z };
			}
		}
		else
		{
			return { a_vec.x, a_vec.y, a_vec.z };
		}
	}

	inline RE::NiPoint3 ToNiPoint3(const glm::vec4& a_vec, bool&& a_normalize = false)
	{
		return ToNiPoint3(glm::vec3(a_vec), std::move(a_normalize));
	}

	inline glm::vec3 ToVec3(const RE::NiPoint3& a_point, bool&& a_normalize = false)
	{
		if (a_normalize) 
		{
			if (a_point.Length() == 0.0f) 
			{
				return glm::vec3(0.0f);
			}
			else
			{
				return glm::normalize(glm::vec3(a_point.x, a_point.y, a_point.z));
			}
		}
		else
		{
			return { a_point.x, a_point.y, a_point.z };
		}
	}

	inline glm::vec3 ToVec3(const RE::hkVector4& a_vec, bool&& a_normalize = false)
	{
		if (a_normalize) 
		{
			if (a_vec.Length3() == 0.0f) 
			{
				return glm::vec3(0.0f);
			}
			else
			{
				return glm::normalize
				(
					glm::vec3
					(
						a_vec.quad.m128_f32[0], 
						a_vec.quad.m128_f32[1], 
						a_vec.quad.m128_f32[2]
					)
				);
			}
		}
		else
		{
			return { a_vec.quad.m128_f32[0], a_vec.quad.m128_f32[1], a_vec.quad.m128_f32[2] };
		}
	}

	inline glm::vec4 ToVec4(const RE::NiPoint3& a_point, bool&& a_normalize = false)
	{
		if (a_normalize) 
		{
			if (a_point.Length() == 0.0f) 
			{
				return glm::vec4(0.0f);
			}
			else
			{
				return glm::normalize(glm::vec4(a_point.x, a_point.y, a_point.z, 0.0f));
			}
		}
		else
		{
			return { a_point.x, a_point.y, a_point.z, 0.0f };
		}
	}

	inline glm::vec4 ToVec4(const RE::hkVector4& a_vec, bool&& a_normalize = false)
	{
		if (a_normalize)
		{
			if (a_vec.Length3() == 0.0f)
			{
				return glm::vec4(0.0f);
			}
			else
			{
				return glm::normalize
				(
					glm::vec4
					(
						a_vec.quad.m128_f32[0],
						a_vec.quad.m128_f32[1],
						a_vec.quad.m128_f32[2], 0.0f
					)
				);
			}
		}
		else
		{
			return { a_vec.quad.m128_f32[0], a_vec.quad.m128_f32[1], a_vec.quad.m128_f32[2], 0.0f };
		}
	}

	//================
	//[Interpolation]:
	//================
	
	// Holds data and linear interpolation operations for floats and NiPoint3's.
	template <typename T>
	struct InterpolationData
	{
		InterpolationData()
		{
			if constexpr (std::is_same_v<T, RE::NiPoint3>)
			{
				prev = current = next = RE::NiPoint3();
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				prev = current = next = 0.0f;
			}

			secsSinceUpdate = 0.0f;
			secsUpdateInterval = 1.0f;
		}

		InterpolationData(T a_prev, T a_current, T a_next, const float& a_secsUpdateInterval) :
			prev(a_prev), current(a_current), next(a_next), 
			secsSinceUpdate(0.0f),
			secsUpdateInterval(a_secsUpdateInterval)
		{ }

		InterpolationData(const InterpolationData& lid) = delete;
		InterpolationData(InterpolationData&& lid) = delete;

		// Add time delta to duration since last update.
		inline void IncrementTimeSinceUpdate(const float& a_msSinceLastIteration)
		{
			secsSinceUpdate += a_msSinceLastIteration;
		}

		// Various interpolation types below.

		inline void InterpolateLinear(const float& a_ratio)
		{
			if (isinf(a_ratio) || isnan(a_ratio))
			{
				current = next;
			}
			else
			{
				if constexpr (std::is_same_v<T, RE::NiPoint3>)
				{
					current.x = std::lerp(prev.x, next.x, a_ratio);
					current.y = std::lerp(prev.y, next.y, a_ratio);
					current.z = std::lerp(prev.z, next.z, a_ratio);
				}
				else if constexpr (std::is_same_v<T, float>)
				{
					current = std::lerp(prev, next, a_ratio);
				}
			}
		}

		inline void InterpolateEaseIn(const float& a_ratio, const float& a_pow)
		{
			float powUpper = ceilf(a_pow);
			float powLower = floorf(a_pow);
			float upperPowerCoeff = fmodf(a_pow, powLower);
			float lowerPowCoeff = 1.0f - upperPowerCoeff;
			float newTRatio = 
			(
				lowerPowCoeff * powf(a_ratio, powLower) + 
				upperPowerCoeff * powf(a_ratio, powUpper)
			);
			InterpolateLinear(newTRatio);
		}

		inline void InterpolateEaseOut(const float& a_ratio, const float& a_pow)
		{
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
			InterpolateLinear(newTRatio);
		}

		inline void InterpolateEaseInEaseOut(const float& a_ratio, const float& a_pow)
		{
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
				) + 0.5f
			);
			float newTRatio = (a_ratio < 0.5f) ? easeInT : easeOutT;
			InterpolateLinear(newTRatio);
		}

		// Smoother step implementation: https://en.wikipedia.org/wiki/Smoothstep
		inline void InterpolateSmootherStep(const float& a_t)
		{
			InterpolateLinear(a_t * a_t * a_t * (6.0f * a_t * a_t - 15.0f * a_t + 10.0f));
		}

		// Reset all three cached values and last reported update time.
		inline void ResetData()
		{
			if constexpr (std::is_same_v<T, RE::NiPoint3>)
			{
				prev = current = next = RE::NiPoint3();
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				prev = current = next = 0.0f;
			}

			secsSinceUpdate = 0.0f;
		}

		// Set the three data points directly.
		inline void SetData(T a_prev, T a_current, T a_next)
		{
			prev = a_prev;
			current = a_current;
			next = a_next;
		}

		// Set the time since the last update directly.
		inline void SetTimeSinceUpdate(const float& a_secsSinceUpdate)
		{
			secsSinceUpdate = a_secsSinceUpdate;
		}

		// Interpolation endpoint reached,
		// so make sure the time since update
		// equals the update interval.
		inline void SetUpdateDurationAsComplete()
		{
			secsSinceUpdate = secsUpdateInterval;
		}

		// Shift over the current value into
		// the previous value, and the new
		// next value into the next value.
		inline void ShiftEndpoints(T a_newNext)
		{
			prev = current;
			next = a_newNext;
		}

		InterpolationData& operator=(const InterpolationData& a_lid)
		{
			prev = a_lid.prev;
			current = a_lid.current;
			next = a_lid.next;

			secsSinceUpdate = a_lid.secsSinceUpdate;
			secsUpdateInterval = a_lid.secsUpdateInterval;
			return *this;
		}

		InterpolationData& operator=(InterpolationData&& a_lid)
		{
			prev = std::move(a_lid.prev);
			current = std::move(a_lid.current);
			next = std::move(a_lid.next);

			secsSinceUpdate = std::move(a_lid.secsSinceUpdate);
			secsUpdateInterval = std::move(a_lid.secsUpdateInterval);
			return *this;
		}

		//
		// Members
		//

		// Starting interpolation endpoint.
		T prev;
		// Current interpolated value.
		T current;
		// Target interpolation endpoint.
		T next;
		// Seconds since the last data point shift.
		// Range: [0, 'secsUpdateInterval'].
		// Divided by 'secsUpdateInterval' to get
		// the time ratio for interpolation.
		float secsSinceUpdate;
		// Seconds after which to perform an update
		// by shifting the interpolation endpoints
		// before interpolating again over the next interval.
		float secsUpdateInterval;
	};
	
	// Holds interpolation data for two-way constant interpolation
	// from one endpoint to the other triggered by some state change
	// related to the data.
	struct TwoWayInterpData
	{
		TwoWayInterpData() :
			directionChangeTP(SteadyClock::now()),
			directionChangeFlag(false),
			interpToMax(false),
			interpToMin(true),
			secsInterpToMaxInterval(1.0f),
			secsInterpToMinInterval(1.0f),
			valueAtDirectionChange(0.0f),
			value(0.0f)
		{ }

		// Force interpolation to one endpoint or the other,
		// whichever serves as the starting endpoint (towards minimum by default).
		inline void Reset(bool&& a_towardsMin = true) 
		{
			if (a_towardsMin) 
			{
				interpToMin = true;
				interpToMax = false;
				directionChangeFlag = false;
			}
			else
			{
				interpToMin = false;
				interpToMax = true;
				directionChangeFlag = true;
			}

			directionChangeTP = SteadyClock::now();
			valueAtDirectionChange = value;
		}

		// Set interval to interpolate over to the minimum or maximum endpoint.
		inline void SetInterpInterval(const float& a_secsInterpInterval, const bool&& a_isToMaxInterval) 
		{
			if (a_isToMaxInterval) 
			{
				secsInterpToMaxInterval = a_secsInterpInterval; 
			}
			else
			{
				secsInterpToMinInterval = a_secsInterpInterval; 
			}
		}

		// Update the interpolated value after updating the cached
		// direction change flag with the given one.
		// A direction flag of true means interpolate to the maximum endpoint,
		// and a direction flag of false means interpolate to the minimum endpoint.
		// If the two flags are not equivalent, switch the direction of interpolation.
		float UpdateInterpolatedValue(const bool& a_directionChangeFlag);

		//
		// Members
		//
		
		// Max endpoint: 1.0
		// Min endpoint: 0.0
		
		// Time point indicating when the interpolation direction last changed.
		SteadyClock::time_point directionChangeTP;
		// Should continue interpolating to the maximum or minimum endpoints.
		bool interpToMax;
		bool interpToMin;
		// Gives the requested interpolation direction:
		// True		->		max endpoint.
		// False	->		min endpoint.
		bool directionChangeFlag;
		// Max number of seconds to reach the maximum endpoint value
		// starting from the minimum endpoint value.
		float secsInterpToMaxInterval;
		// Max number of seconds to reach the minimum endpoint value
		// starting from the maximum endpoint value.
		float secsInterpToMinInterval;
		// Saved interpolated value when the interpolation direction changes.
		float valueAtDirectionChange;
		// Range: [0.0, 1.0]
		float value;
	};

	namespace Util 
	{
		//==================================================
		//[Lambert W Function Solution Estimator Functions]:
		//==================================================
		// NOTE: Was previously used to set projectile release speeds 
		// when accounting for linear air drag.

		namespace LambertWFunc
		{
			// Three approximation methods.
			enum class ApproxMethod : uint8_t
			{
				kNewton,
				kHalley,
				kIaconoBoyd
			};

			// Holds two REAL values, one for each branch of the Lambert W function.
			using FuncRealSolnPair = std::pair<std::optional<double>, std::optional<double>>;
			// Returns approximations of the REAL value of the Lambert W function at the given z value,
			// approximated until the given precision is reached.
			FuncRealSolnPair ApproxRealSolutionBothBranches(const double& a_z, const double& a_precision);
			// Helper function which runs an approximation of the Lambert W function 
			// based on the given approximation type, the given z value,
			// and using the initial W0 and W-1 branch values and given precision to reach.
			FuncRealSolnPair RunApprox(ApproxMethod&& a_method, const double& a_init0, const double& a_initMin1, const double& a_z, const double& a_precision);
			// All approximation methods adapted from here:
			// https://en.wikipedia.org/wiki/Lambert_W_function#Numerical_evaluation
			double HalleyApprox(const double& a_wn, const double& a_z);
			double IaconoBoydApprox(const double& a_wn, const double& a_z);
			double NewtonApprox(const double& a_wn, const double& a_z);
		};

		//===================
		//[Native Functions]:
		//===================
		// Called directly from the game's code, not through the Papyrus VM.

		namespace NativeFunctions
		{
			// Credits to ersh1 once again for the havok and quaternion-related functions below:
			// https://github.com/ersh1/Precision/blob/main/src/Offsets.h#L77
			typedef void(_fastcall* thkpEntity_Activate)(RE::hkpEntity* a_this);
			static REL::Relocation<thkpEntity_Activate> hkpEntity_Activate{ RELOCATION_ID(60096, 60849) };	// A6C730, A90F40

			// https://github.com/ersh1/Precision/blob/main/src/Offsets.h#L107
			typedef bool (*thkpCollisionCallbackUtil_releaseCollisionCallbackUtil)(RE::hkpWorld* a_world);
			static REL::Relocation<thkpCollisionCallbackUtil_releaseCollisionCallbackUtil> hkpCollisionCallbackUtil_releaseCollisionCallbackUtil{ RELOCATION_ID(61800, 62715) };  // AC6230, AEAA40

			// https://github.com/ersh1/Precision/blob/main/src/Offsets.h#L104
			typedef bool (*thkpCollisionCallbackUtil_requireCollisionCallbackUtil)(RE::hkpWorld* a_world);
			static REL::Relocation<thkpCollisionCallbackUtil_requireCollisionCallbackUtil> hkpCollisionCallbackUtil_requireCollisionCallbackUtil{ RELOCATION_ID(60588, 61437) };  // A7DD00, AA2510

			// https://github.com/ersh1/Precision/blob/main/src/Offsets.h#L110
			typedef void* (*thkpWorld_addContactListener)(RE::hkpWorld* a_world, RE::hkpContactListener* a_worldListener);
			static REL::Relocation<thkpWorld_addContactListener> hkpWorld_addContactListener{ RELOCATION_ID(60543, 61383) };  // A7AB80, A9F390

			// https://github.com/ersh1/Precision/blob/main/src/Offsets.h#L113
			typedef void* (*thkpWorld_removeContactListener)(RE::hkpWorld* a_world, RE::hkpContactListener* a_worldListener);
			static REL::Relocation<thkpWorld_removeContactListener> hkpWorld_removeContactListener{ RELOCATION_ID(68974, 70327) };  // C59BD0, C814A0

			// https://github.com/ersh1/Precision/blob/main/src/Offsets.h#L98
			typedef void (*tNiMatrixToNiQuaternion)(RE::NiQuaternion& a_quatOut, const RE::NiMatrix3& a_matIn);
			static REL::Relocation<tNiMatrixToNiQuaternion> NiMatrixToNiQuaternion{ RELOCATION_ID(69467, 70844) };	// C6E2D0, C967D0

			// The next two are once again courtesy of SmoothCam:
			// SE: https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/include/offset_ids.h#L72-L73
			// AE: https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/include/offset_ids.h#L142-L143
			typedef bool(__fastcall* tPlayerCamera_LinearCast)
			(
				decltype(RE::PlayerCamera::unk120) a_physics, RE::bhkWorld* a_world, glm::vec4& a_rayStart,
				glm::vec4& a_rayEnd, uint32_t* a_rayResultInfo, RE::NiAVObject** a_object, float a_traceHullSize
			);
			static REL::Relocation<tPlayerCamera_LinearCast> PlayerCamera_LinearCast{ RELOCATION_ID(32270, 33007) };

			typedef RE::NiAVObject* (*thkpCdBody_GetUserData)(const RE::hkpCdBody*);
			static REL::Relocation<thkpCdBody_GetUserData> hkpCdBody_GetUserData{ RELOCATION_ID(76160, 77988) };

			// Credits to adamhynek for the VR offset from his awesome mod PLANCK:
			// https://github.com/adamhynek/activeragdoll/blob/master/src/RE/offsets.cpp#L304
			// and to alandtse for Skyrim VR Address Library:
			// https://github.com/alandtse/skyrim_vr_address_library

			// Clear movement offset for the given actor.
			inline void ClearKeepOffsetFromActor(RE::Actor* a_actor)
			{
				using func_t = decltype(&ClearKeepOffsetFromActor);
				REL::Relocation<func_t> func{ RELOCATION_ID(36871, 37895) };
				return func(a_actor);
			}

			// Credit to po3 for the Favorite/Unfavorite functions below.
			// The functions here are called without the restriction of the target form having
			// to be in P1's inventory, since I needed a way to un/favorite forms 
			// straight from a companion player's inventory.
			// https://github.com/powerof3/PapyrusExtenderSSE/blob/master/include/Papyrus/Functions/Form/Functions.h

			// Using the given inventory entry and the given extra data list,
			// favorite the associated bound object to the given inventory changes.
			inline void Favorite(RE::InventoryChanges* a_changes, RE::InventoryEntryData* a_entryData, RE::ExtraDataList* a_list)
			{
				using func_t = decltype(&Favorite);
				REL::Relocation<func_t> func{ RELOCATION_ID(15858, 16098) };
				return func(a_changes, a_entryData, a_list);
			}

			// Full credit goes to Exit-9B:
			// https://github.com/Exit-9B/Constellations/blob/main/src/Hooks/Athletics.cpp#L73
			inline void ForceUpdateCachedMovementType(RE::Actor* a_actor)
			{
				using func_t = decltype(&ForceUpdateCachedMovementType);
				REL::Relocation<func_t> func{ RELOCATION_ID(36916, 37941) };
				return func(a_actor);
			}

			// Credits to adamhynek for the VR offset from his awesome VR mods PLANCK and HIGGS:
			// https://github.com/adamhynek/activeragdoll/blob/master/src/RE/offsets.cpp#L303
			// and to alandtse for Skyrim VR Address Library:
			// https://github.com/alandtse/skyrim_vr_address_library

			// Keep a movement offset between the given actor and the given target actor.
			// Positional and angular offsets are given in local space relative
			// to the forward direction of the given actor.
			// Catch up radius determines the distance from the target actor
			// beyond which the actor should start running to catch up.
			// Follow radius determines the distance at which the actor
			// should stop completely when near the target actor.
			inline void KeepOffsetFromActor(RE::Actor* a_actor, const RE::ActorHandle& a_targetHandle, const RE::NiPoint3& a_posOffset, const RE::NiPoint3& a_angOffset, float a_catchUpRadius, float a_followRadius) 
			{
				using func_t = decltype(&KeepOffsetFromActor);
				REL::Relocation<func_t> func{ RELOCATION_ID(36870, 37894) };
				return func(a_actor, a_targetHandle, a_posOffset, a_angOffset, a_catchUpRadius, a_followRadius);
			}
			
			// Full credits to covey-j for their Actor Copy Lib: 
			// https://github.com/covey-j/ActorCopyLib

			// Remove the headpart corresponding to the given head part type from the given actor base.
			inline void RemoveHeadPart(RE::TESNPC* a_npc, RE::TESNPC::HeadPartType a_type)
			{
				using func_t = decltype(&RemoveHeadPart);
				REL::Relocation<func_t> func{ RELOCATION_ID(24247, 24751) };
				return func(a_npc, a_type);
			}

			// Credit to Sylennus:
			// https://github.com/Sylennus/PlayerMannequin/blob/master/src/MannequinInterface.cpp#L311

			// (Re)set the given actor base data flag for the given actor base data.
			inline void SetActorBaseDataFlag(RE::TESActorBaseData* a_actorBaseData, RE::ACTOR_BASE_DATA::Flag a_flag, bool a_enable)
			{
				using func_t = decltype(&SetActorBaseDataFlag);
				REL::Relocation<func_t> func{ RELOCATION_ID(14261, 14383) };
				return func(a_actorBaseData, a_flag, a_enable);
			}

			// All credits for the following function go to VersuchDrei:
			// https://github.com/VersuchDrei/OStimNG/blob/main/skse/src/GameAPI/GameActor.h#L90

			// Stop/allow the given actor to move.
			// Stops all translational and rotational movement instantly if set (animations still play though).
			inline bool SetDontMove(RE::Actor* a_actor, bool a_dontMove)
			{
				using func_t = decltype(SetDontMove);
				REL::Relocation<func_t> func{ RELOCATION_ID(36490, 37489) };
				return func(a_actor, a_dontMove);
			}

			// Credits to dTry for the two unequip funcs below:
			// https://github.com/D7ry/wheeler/blob/main/src/bin/Utilities/Utils.cpp#L53
			// Unequip the given shout for the given actor.
			inline void UnequipShout(RE::Actor* a_actor, RE::TESShout* a_shout)
			{
				auto aem = RE::ActorEquipManager::GetSingleton();
				if (!aem)
				{
					return;
				}

				using func_t = void(RE::ActorEquipManager::*)(RE::Actor*, RE::TESShout*);
				REL::Relocation<func_t> func{ RELOCATION_ID(37948, 38904) };
				return func(aem, a_actor, a_shout);
			}

			// https://github.com/D7ry/wheeler/blob/main/src/bin/Utilities/Utils.cpp#L37
			// Unequip the given spell from the given slot for the given actor.
			inline void UnequipSpell(RE::Actor* a_actor, RE::SpellItem* a_spell, const uint32_t a_slot)
			{
				auto aem = RE::ActorEquipManager::GetSingleton();
				if (!aem)
				{
					return;
				}
				
				using func_t = void(RE::ActorEquipManager::*)(RE::Actor*, RE::SpellItem*, const uint32_t);
				REL::Relocation<func_t> func{ RELOCATION_ID(37947, 38903) };
				return func(aem, a_actor, a_spell, a_slot);
			}

			// Using the given inventory entry and the given extra data list,
			// unfavorite the associated bound object from the given inventory changes.
			inline void Unfavorite(RE::InventoryChanges* a_changes, RE::InventoryEntryData* a_entryData, RE::ExtraDataList* a_list)
			{
				using func_t = decltype(&Unfavorite);
				REL::Relocation<func_t> func{ RELOCATION_ID(15859, 16099) };
				return func(a_changes, a_entryData, a_list);
			}

			// Credits to mwilsnd:
			// https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/include/offset_ids.h#L47

			// Update the given NiCamera's world-to-scaleform matrix.
			inline void UpdateWorldToScaleform(RE::NiCamera* a_niCam) 
			{
				using func_t = decltype(&UpdateWorldToScaleform);
				REL::Relocation<func_t> func{ RELOCATION_ID(69271, 70641) };
				return func(a_niCam);
			}
		};

		//====================
		//[Papyrus Functions]:
		//====================
		// Functions called using the Papyrus VM.

		namespace Papyrus
		{
			// Force open the given actor's inventory.
			inline void OpenInventory(RE::Actor* a_actor)
			{
				if (a_actor)
				{
					const auto policy = RE::BSScript::Internal::VirtualMachine::GetSingleton()->GetObjectHandlePolicy();
					if (policy)
					{
						auto handle = policy->GetHandleForObject(*a_actor->formType, a_actor);
						if (handle)
						{
							auto callback = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>();
							auto args = RE::MakeFunctionArguments(true);
							RE::BSScript::Internal::VirtualMachine::GetSingleton()->DispatchMethodCall
							(
								handle, 
								"Actor", 
								"OpenInventory",
								args, 
								callback
							);
						}
					}
				}
			}

			// Send an assault alarm directed at the given actor.
			inline void SendAssaultAlarm(RE::Actor* a_actor) 
			{
				if (a_actor)
				{
					const auto policy = RE::BSScript::Internal::VirtualMachine::GetSingleton()->GetObjectHandlePolicy();
					if (policy)
					{
						auto handle = policy->GetHandleForObject(*a_actor->formType, a_actor);
						if (handle)
						{
							auto callback = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>();
							auto args = RE::MakeFunctionArguments();
							RE::BSScript::Internal::VirtualMachine::GetSingleton()->DispatchMethodCall
							(
								handle, 
								"Actor",
								"SendAssaultAlarm",
								args, 
								callback
							);
						}
					}
				}
			}

			// Start combat between the given starting and target actors.
			inline void StartCombat(RE::Actor* a_starting, RE::Actor* a_target) 
			{
				if (a_starting && a_target)
				{
					const auto policy = RE::BSScript::Internal::VirtualMachine::GetSingleton()->GetObjectHandlePolicy();
					if (policy)
					{
						auto handle = policy->GetHandleForObject(*a_starting->formType, a_starting);
						if (handle)
						{
							auto callback = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>();
							auto args = RE::MakeFunctionArguments(std::move(a_target));
							RE::BSScript::Internal::VirtualMachine::GetSingleton()->DispatchMethodCall
							(
								handle, 
								"Actor", 
								"StartCombat", 
								args,
								callback
							);
						}
					}
				}
			}

			// Unequip all equipped forms for the given actor.
			inline void UnequipAll(RE::Actor* a_actor) 
			{
				if (a_actor)
				{
					const auto policy = RE::BSScript::Internal::VirtualMachine::GetSingleton()->GetObjectHandlePolicy();
					if (policy)
					{
						auto handle = policy->GetHandleForObject(*a_actor->formType, a_actor);
						if (handle)
						{
							auto callback = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>();
							auto args = RE::MakeFunctionArguments();
							RE::BSScript::Internal::VirtualMachine::GetSingleton()->DispatchMethodCall
							(
								handle, 
								"Actor",
								"UnequipAll",
								args, 
								callback
							);
						}
					}
				}
			}

			// Have the given actor unequip the given shout.
			inline void UnequipShout(RE::Actor* a_actor, RE::TESShout* a_shout)
			{
				if (a_actor && a_shout)
				{
					const auto policy = RE::BSScript::Internal::VirtualMachine::GetSingleton()->GetObjectHandlePolicy();
					if (policy)
					{
						auto handle = policy->GetHandleForObject(*a_actor->formType, a_actor);
						if (handle)
						{
							auto callback = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>();
							auto args = RE::MakeFunctionArguments(std::move(a_shout));
							RE::BSScript::Internal::VirtualMachine::GetSingleton()->DispatchMethodCall
							(
								handle, 
								"Actor", 
								"UnequipShout",
								args, 
								callback
							);
						}
					}
				}
			}
		};

		//===========================
		//[Inline Utility Functions]:
		//===========================

		// Have the given activator refr activate the given interaction target refr.
		// Can specify the refr's corresponding bound object,
		// the number to activate, and if only default processing should be used.
		inline void ActivateRef(RE::TESObjectREFR* a_interactionTarget, RE::TESObjectREFR* a_activator, uint8_t a_arg2, RE::TESBoundObject* a_object, int32_t a_count, bool a_defaultProcessingOnly) 
		{
			// Activator or interaction refr are invalid.
			if (!a_activator || !a_interactionTarget || !a_interactionTarget->loadedData || 
				a_interactionTarget->IsDisabled() || a_interactionTarget->IsDeleted() || !a_interactionTarget->IsHandleValid()) 
			{
				return;
			}

			// Interaction target calls activate on itself.
			a_interactionTarget->ActivateRef(a_activator, a_arg2, a_object, a_count, a_defaultProcessingOnly);
		}

		//===========================================================================================================================================
		//=======================
		// [Angles and Distance]:
		//=======================
		
		// Convert angle (in degrees) between the game's angular coordinate system 
		// and the Cartesian convention.
		// Conversion formula courtesy of the Creation Kit Wiki:
		// https://www.creationkit.com/index.php?title=GetAngleZ_-_ObjectReference
		inline float ConvertAngle(const float& a_angle)
		{
			if (a_angle <= PI / 2.0f)
			{
				return (PI / 2.0f - a_angle);
			}
			else
			{
				return (5.0f * PI / 2.0f - a_angle);
			}
		}

		// Adjust the given angle to lie in the range [0, 2PI].
		inline float NormalizeAng0To2Pi(const float a_angle)
		{
			if (a_angle > 2.0f * PI)
			{
				return (a_angle - 2.0f * PI);
			}
			else if (a_angle < 0.0f)
			{
				return (a_angle + 2.0f * PI);
			}

			return a_angle;
		}

		// Adjust the given angle to lie in the range [-2PI, 2PI].
		inline float NormalizeAngTo2Pi(const float a_angle)
		{
			if (a_angle > 2.0f * PI)
			{
				return (a_angle - 2.0f * PI);
			}
			else if (a_angle < -2.0f * PI)
			{
				return (a_angle + 2.0f * PI);
			}

			return a_angle;
		}

		// Adjust the given angle to lie in the range [-PI / 2, PI / 2].
		inline float NormalizeAngToHalfPi(const float a_angle)
		{
			if (a_angle > PI / 2.0f)
			{
				return (a_angle - PI / 2.0f);
			}
			else if (a_angle < -PI / 2.0f)
			{
				return (a_angle + PI / 2.0f);
			}

			return a_angle;
		}

		// Adjust the given angle to lie in the range [-PI, PI].
		inline float NormalizeAngToPi(const float a_angle)
		{
			if (a_angle > PI)
			{
				return (a_angle - 2.0f * PI);
			}
			else if (a_angle < -PI)
			{
				return (a_angle + 2.0f * PI);
			}

			return a_angle;
		}

		// Get the game pitch angle for the given direction.
		inline float DirectionToGameAngPitch(const RE::NiPoint3& a_dir)
		{
			return atan2f(-a_dir.z, sqrtf(a_dir.x * a_dir.x + a_dir.y * a_dir.y));
		}

		// Get the game yaw angle for the given direction.
		inline float DirectionToGameAngYaw(const RE::NiPoint3& a_dir)
		{
			return 
			(
				Util::ConvertAngle
				(
					Util::NormalizeAng0To2Pi
					(
						atan2f(a_dir.y, a_dir.x)
					)
				)
			);
		}
		
		// Get the game pitch between the two positions.
		inline float GetPitchBetweenPositions(RE::NiPoint3 a_sourcePos, RE::NiPoint3 a_targetPos)
		{
			// Sick formatting, bro (x2).
			return 
			(
				asinf
				(
					max
					(
						// Prevent div by 0.
						min
						(
							(a_sourcePos.z - a_targetPos.z) / (a_targetPos.GetDistance(a_sourcePos) + 0.00001f), 
							1.0f
						),
						-1.0f
					)
				)
			);
		}
		// Return the XY distance from the point (0, 0, 0) to the given point.
		inline float GetXYDistance(const RE::NiPoint3& a_pt)
		{
			return sqrtf(powf(a_pt.x, 2.0f) + powf(a_pt.y, 2.0f));
		}

		// Return the XY distance from the the first point to the second point.
		inline float GetXYDistance(RE::NiPoint3 a_pt1, RE::NiPoint3 a_pt2)
		{
			return sqrtf(powf(a_pt1.x - a_pt2.x, 2.0f) + powf(a_pt1.y - a_pt2.y, 2.0f));
		}

		// Return the XY distance from the the first point 
		// given by the X, Y coordinates pair to the second point.
		inline float GetXYDistance(float a_pt1X, float a_pt1Y, RE::NiPoint3 a_pt2)
		{
			return sqrtf(powf(a_pt1X - a_pt2.x, 2.0f) + powf(a_pt1Y - a_pt2.y, 2.0f));
		}

		// Return the XY distance from the the first point
		// to the second point given by the X, Y coordinates pair.
		inline float GetXYDistance(RE::NiPoint3 a_pt1, float a_pt2X, float a_pt2Y)
		{
			return sqrtf(powf(a_pt1.x - a_pt2X, 2.0f) + powf(a_pt1.y - a_pt2Y, 2.0f));
		}

		// Return the XY distance from the first X, Y coodinates pair to the second one.
		inline float GetXYDistance(float a_pt1X, float a_pt1Y, float a_pt2X, float a_pt2Y)
		{
			return sqrtf(powf(a_pt1X - a_pt2X, 2.0f) + powf(a_pt1Y - a_pt2Y, 2.0f));
		}

		// Get the game yaw angle from the given source position to the target position.
		inline float GetYawBetweenPositions(RE::NiPoint3 a_sourcePos, RE::NiPoint3 a_targetPos)
		{
			return 
			(
				Util::ConvertAngle
				(
					atan2f
					(
						a_targetPos.y - a_sourcePos.y, 
						a_targetPos.x - a_sourcePos.x
					)
				)
			);
		}

		// Get the game yaw angle from the given source refr to the target refr.
		inline float GetYawBetweenRefs(RE::TESObjectREFR* a_sourceRefr, RE::TESObjectREFR* a_targetRefr)
		{
			return 
			(
				Util::ConvertAngle
				(
					atan2f
					(
						a_targetRefr->data.location.y - a_sourceRefr->data.location.y,
						a_targetRefr->data.location.x - a_sourceRefr->data.location.x
					)
				)
			);
		}

		// Smoother step interpolation implementation: https://en.wikipedia.org/wiki/Smoothstep
		inline float InterpolateSmootherStep(const float& a_lower, const float& a_upper, const float& a_t)
		{
			return std::lerp
			(
				a_lower, 
				a_upper, 
				(a_t * a_t * a_t * (6.0f * a_t * a_t - 15.0f * a_t + 10.0f))
			);
		}

		//===========================================================================================================================================

		// Get the given refr's 3D center world position.
		// If the current 3D is unavailable, return a position 
		// halfway up the reference, given by its current position.
		// Possible TODO: 
		// If the current 3D is invalid, 
		// account for the refr's pitch angle when getting halfway point,
		inline RE::NiPoint3 Get3DCenterPos(RE::TESObjectREFR* a_refr)
		{
			if (!a_refr)
			{
				return RE::NiPoint3();
			}

			if (const auto refr3DPtr = RE::NiPointer<RE::NiAVObject>(a_refr->GetCurrent3D()); refr3DPtr && refr3DPtr.get())
			{
				return refr3DPtr->worldBound.center;
			}
			else
			{
				return a_refr->data.location + RE::NiPoint3(0.0f, 0.0f, a_refr->GetHeight() / 2.0f);
			}
		}

		// Get the given actor's character controller-reported linear velocity.
		inline RE::NiPoint3 GetActorLinearVelocity(RE::Actor* a_actor)
		{
			if (const auto charController = a_actor->GetCharController(); charController)
			{
				RE::hkVector4 linVel{ 0 };
				charController->GetLinearVelocityImpl(linVel);
				return RE::NiPoint3(linVel.quad.m128_f32[0], linVel.quad.m128_f32[1], linVel.quad.m128_f32[2]) * HAVOK_TO_GAME;
			}

			return RE::NiPoint3();
		}

		// Get the actor smart pointer from the given handle.
		// Return nullptr if the handle, its associated smart pointer, 
		// or its raw pointer are invalid.
		inline RE::ActorPtr GetActorPtrFromHandle(const RE::ActorHandle& a_handle) 
		{
			if (a_handle && a_handle.get() && a_handle.get().get()) 
			{
				return a_handle.get();
			}

			return nullptr;
		}

		// Get the enumeration name for the given actor value.
		inline RE::BSFixedString GetActorValueName(const RE::ActorValue& a_av) 
		{
			const auto avList = RE::ActorValueList::GetSingleton();
			if (!avList)
			{
				return ""sv;
			}

			const auto avInfo = avList->GetActorValue(a_av);
			if (!avInfo)
			{
				return ""sv;
			}

			return avInfo->enumName;
		}

		// Get the world position reported by the given actor's character controller.
		inline RE::NiPoint3 GetActorWorldPosition(RE::Actor* a_actor)
		{
			if (const auto charController = a_actor->GetCharController(); charController)
			{
				RE::hkVector4 pos{ 0 };
				charController->GetPositionImpl(pos, true);
				return RE::NiPoint3(pos.quad.m128_f32[0], pos.quad.m128_f32[1], pos.quad.m128_f32[2]) * HAVOK_TO_GAME;
			}

			return RE::NiPoint3();
		}

		// Get the bound weapon associated with the given spell, if any.
		inline RE::TESObjectWEAP* GetAssociatedBoundWeap(RE::SpellItem* a_spell) 
		{
			if (!a_spell) 
			{
				return nullptr;
			}

			if (auto avEffect = a_spell->GetAVEffect(); avEffect && avEffect->data.associatedForm) 
			{
				if (auto assocWeap = avEffect->data.associatedForm->As<RE::TESObjectWEAP>(); assocWeap && assocWeap->IsBound()) 
				{
					return assocWeap;
				}
			}

			return nullptr;
		}

		// Get the detection percent of the requesting actor by the detecting actor [0.0, 100.0].
		inline float GetDetectionPercent(RE::Actor* a_reqActor, RE::Actor* a_detectingActor) 
		{
			// Sick formatting, bro.
			return 
			(
				5.0f * 
				(
					20.0f + 
					std::clamp
					(
						static_cast<float>(a_detectingActor->RequestDetectionLevel(a_reqActor)), 
						-20.0f, 
						0.0f
					)
				)
			);
		}

		// Get number of seconds that have elapsed since the given time point.
		// 3 extra decimal places of precision if requested.
		inline float GetElapsedSeconds(const SteadyClock::time_point& a_timePoint, bool&& a_extraPrecision = false)
		{
			if (a_extraPrecision)
			{
				return std::chrono::duration_cast<std::chrono::microseconds>(SteadyClock::now() - a_timePoint).count() / 1000000.0f;
			}
			else
			{
				return std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - a_timePoint).count() / 1000.0f;
			}
		}

		// Get boolean game setting from the given setting name.
		// Nullopt if unable to get setting.
		inline std::optional<bool> GetGameSettingBool(RE::BSFixedString&& a_settingName)
		{
			if (a_settingName.empty())
			{
				return std::nullopt;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting)
				{
					return setting->data.b;
				}
			}

			return std::nullopt;
		}

		// Get float game setting from the given setting name.
		// Nullopt if unable to get setting.
		inline std::optional<float> GetGameSettingFloat(RE::BSFixedString&& a_settingName)
		{
			if (a_settingName.empty())
			{
				return std::nullopt;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting)
				{
					return setting->data.f;
				}
			}

			return std::nullopt;
		}

		// Get signed integer game setting from the given setting name.
		// Nullopt if unable to get setting.
		inline std::optional<int32_t> GetGameSettingInt(RE::BSFixedString&& a_settingName)
		{
			if (a_settingName.empty())
			{
				return std::nullopt;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting)
				{
					return setting->data.i;
				}
			}

			return std::nullopt;
		}

		
		// Get string game setting from the given setting name.
		// Nullopt if unable to get setting.
		inline std::optional<RE::BSFixedString> GetGameSettingString(RE::BSFixedString&& a_settingName)
		{
			if (a_settingName.empty())
			{
				return std::nullopt;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting)
				{
					return setting->data.s;
				}
			}

			return std::nullopt;
		}

		// Get unsigned integer game setting from the given setting name.
		// Nullopt if unable to get setting.
		inline std::optional<uint32_t> GetGameSettingUInt(RE::BSFixedString&& a_settingName)
		{
			if (a_settingName.empty())
			{
				return std::nullopt;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting)
				{
					return setting->data.u;
				}
			}

			return std::nullopt;
		}

		// Get the number of lockpicks the given actor possesses.
		// -1 if no inventory entry exits for lockpicks
		// or 0 if the actor previously had some but now has none.
		inline int32_t GetLockpicksCount(RE::Actor* a_actor) 
		{
			const auto defObjMgr = RE::BGSDefaultObjectManager::GetSingleton();
			if (!defObjMgr)
			{
				return -1;
			}

			const auto lockpickObj = defObjMgr->objects[RE::DEFAULT_OBJECT::kLockpick]->As<RE::TESObjectMISC>();
			const auto invCounts = a_actor->GetInventoryCounts();
			if (lockpickObj && invCounts.contains(lockpickObj))
			{
				return invCounts.at(lockpickObj); 
			}

			return -1;
		}

		// Get player LOS cast start/end position, a focus point of sorts.
		// The player focus point is returned as their refr position
		// offset by a fraction of their height.
		// This varies less than their looking-at position,
		// leading to more consistent raycast hit positions.
		// Also prevents players from clipping their head through walls, and using their looking at position,
		// which is now sticking through the wall, to interact with objects that should not be reachable.
		inline RE::NiPoint3 GetPlayerFocusPoint(RE::Actor* a_playerActor)
		{
			if (!a_playerActor)
			{
				return RE::NiPoint3();
			}

			return 
			(
				a_playerActor->data.location +
				RE::NiPoint3
				(
					0.0f,
					0.0f,
					a_playerActor->IsSneaking() ?
					0.5f * a_playerActor->GetHeight() :
					0.75f * a_playerActor->GetHeight()
				)
			);
		}

		// Get viewport dimensions.
		inline RE::GRect<float> GetPort()
		{
			if (const auto hud = DebugAPI::GetHUD(); hud)
			{
				return hud->uiMovie->GetVisibleFrameRect();
			}

			return RE::GRect<float>(0.0f, 0.0f, 0.0f, 0.0f);
		}

		// Get smart pointer to the given refr's currently loaded 3D,
		// or nullptr if the given refr's 3D is invalid.
		inline RE::NiPointer<RE::NiAVObject> GetRefr3D(RE::TESObjectREFR* a_refr)
		{
			if (!a_refr)
			{
				return nullptr;
			}

			const auto refr3DPtr = RE::NiPointer<RE::NiAVObject>(a_refr->GetCurrent3D());
			return refr3DPtr && refr3DPtr.get() ? refr3DPtr : nullptr;
		}

		// Get the object reference smart pointer from the given handle.
		// Return nullptr if the handle, its associated smart pointer,
		// or its raw pointer are invalid.
		inline RE::TESObjectREFRPtr GetRefrPtrFromHandle(const RE::ObjectRefHandle& a_handle)
		{
			if (a_handle && a_handle.get() && a_handle.get().get())
			{
				return a_handle.get();
			}

			return nullptr;
		}
		
		// Return true if the given actor handle and its managed
		// smart and raw pointers are all valid.
		inline bool HandleIsValid(const RE::ActorHandle& a_handle)
		{
			return a_handle && a_handle.get() && a_handle.get().get();
		}

		// Return true if the given object reference handle and its managed
		// smart and raw pointers are all valid.
		inline bool HandleIsValid(const RE::ObjectRefHandle& a_handle) 
		{
			return a_handle && a_handle.get() && a_handle.get().get();
		}

		// Return true if the given form is either a hostile spell,
		// or a weapon with a hostile spell enchantment.
		inline bool HasHostileSpell(RE::TESForm* a_form)
		{
			if (auto spell = a_form->As<RE::SpellItem>(); spell) 
			{
				return 
				(
					(spell->IsHostile()) || 
					(spell->GetAVEffect() && spell->GetAVEffect()->IsHostile())
				);
			}
			else if (auto weap = a_form->As<RE::TESObjectWEAP>(); weap && weap->IsStaff())
			{
				if (auto formEnchanting = weap->formEnchanting; formEnchanting) 
				{
					return 
					(
						(formEnchanting->IsHostile()) || 
						(formEnchanting->GetAVEffect() && formEnchanting->GetAVEffect()->IsHostile())
					);
				}
			}

			return false;
		}

		// Return true if the given form is a restoration spell,
		// or a weapon with a restoration skill enchantment.
		inline bool HasRestorationSpell(RE::TESForm* a_form)
		{
			auto spell = a_form->As<RE::SpellItem>();
			if (spell && spell->GetAVEffect())
			{
				return 
				(
					spell->GetAVEffect()->data.associatedSkill == RE::ActorValue::kRestoration
				);
			}
			else if (auto weap = a_form->As<RE::TESObjectWEAP>(); weap && weap->IsStaff())
			{
				if (auto formEnchanting = weap->formEnchanting; formEnchanting && formEnchanting->GetAVEffect()) 
				{
					return 
					(
						formEnchanting->GetAVEffect()->data.associatedSkill == RE::ActorValue::kRestoration
					);
				}
			}

			return false;
		}

		// Return true if the given spell fires a rune projectile when cast.
		inline bool HasRuneProjectile(RE::SpellItem* a_spell) 
		{
			return 
			(
				a_spell && 
				a_spell->GetDelivery() == RE::MagicSystem::Delivery::kTargetLocation &&
				a_spell->GetAVEffect() &&
				a_spell->GetAVEffect()->data.projectileBase &&
				a_spell->GetAVEffect()->data.projectileBase->data.types.all(RE::BGSProjectileData::Type::kGrenade)
			);
		}

		// Is the given bound object a lootable object for co-op players?
		// (can be moved into a player's inventory directly through activation).
		// NOTE: Reference instead of a pointer so that this function
		// can be used as an inventory filter function.
		inline bool IsLootableObject(RE::TESBoundObject& a_object)
		{
			// Must be playable, not deleted, not a static object or other object without a loaded name,
			// have a certain base type, have not been harvested (if a flora object),
			// have stopped flight (if a projectile), and be carryable (if a light).

			// Holy formatting, Batman!
			return 
			{
				(
					a_object.GetPlayable() && 
					!a_object.IsDeleted() && 
					strlen(a_object.GetName()) > 0
				) && 
				(
					a_object.Is
					(
						RE::FormType::AlchemyItem,
						RE::FormType::Apparatus,
						RE::FormType::ConstructibleObject,
						RE::FormType::Note,
						RE::FormType::Ingredient,
						RE::FormType::Scroll,
						RE::FormType::Ammo,
						RE::FormType::Projectile,
						RE::FormType::KeyMaster,
						RE::FormType::Armature,
						RE::FormType::Armor,
						RE::FormType::Book,
						RE::FormType::Misc,
						RE::FormType::Weapon,
						RE::FormType::SoulGem
					) ||
					(
						(a_object.Is(RE::FormType::Flora, RE::FormType::Tree)) && 
						(a_object.formFlags & RE::TESObjectREFR::RecordFlags::kHarvested) == 0
					) ||
					(
						a_object.As<RE::Projectile>() && a_object.As<RE::Projectile>()->ShouldBeLimited()
					) ||
					(
						a_object.As<RE::TESObjectLIGH>() && a_object.As<RE::TESObjectLIGH>()->CanBeCarried()
					)
				)
			};
		}

		// Is this actor friendly/neutral towards the co-op party?
		inline bool IsPartyFriendlyActor(RE::Actor* a_actor)
		{
			auto p1 = RE::PlayerCharacter::GetSingleton();
			// P1 better not hate themselves.
			if (p1 && a_actor == p1)
			{
				return true;
			}

			// Teammates are friendly, sometimes.
			bool isTeammate = a_actor->IsPlayerTeammate();
			if (isTeammate) 
			{
				return true;
			}

			auto commandingActorPtr = a_actor->GetCommandingActor();
			bool isPartyCommandedActor = 
			{
				(commandingActorPtr && commandingActorPtr.get()) &&
				((p1 && commandingActorPtr.get() == p1) ||
				(commandingActorPtr.get()->IsPlayerTeammate()))
			};
			// Commanded by a player (summons).
			if (isPartyCommandedActor)
			{
				return true;
			}

			// Does not care about friendly hits.
			bool ignoresFriendlyHits = 
			{
				(a_actor->formFlags & RE::TESObjectREFR::RecordFlags::kIgnoreFriendlyHits) &&
				(p1 && !a_actor->IsHostileToActor(p1))
			};
			if (ignoresFriendlyHits)
			{
				return true;
			}

			return false;
		}

		// Party-wide items (usable by any player through P1, or trigger quests).
		// Includes: gold, lockpicks, keys, non-skill/spell teaching books, and notes.
		inline bool IsPartyWideItem(RE::TESForm* a_form)
		{
			return 
			(
				(a_form) && 
				(
					(
						a_form->IsKey() || 
						a_form->IsGold() || 
						a_form->IsLockpick() || 
						a_form->IsNote()
					) ||
					(
						a_form->IsBook() && 
						a_form->As<RE::TESObjectBOOK>()->data.GetSanitizedType() == RE::OBJ_BOOK::Flag::kNone
					)
				)
			);
		}

		// Is the given race one that has an accompanying transformation?
		// Werewolf, vampire, and unplayable races.
		inline bool IsRaceWithTransformation(RE::TESRace* a_race)
		{
			if (!a_race)
			{
				return false;
			}

			return 
			(
				a_race->formEditorID == "WerewolfBeastRace"sv || 
				a_race->formEditorID == "DLC1VampireBeastRace"sv || 
				!a_race->GetPlayable()
			);
		}

		// Is the refr valid for targeting with the crosshair,
		// as an aim correction target, or for activation?
		// Baseline check for availability and handle + 3D validity. 
		inline bool IsValidRefrForTargeting(RE::TESObjectREFR* a_refr)
		{
			if (!a_refr)
			{
				return false;
			}

			return
			(
				a_refr->IsHandleValid() && 
				a_refr->loadedData &&
				a_refr->Is3DLoaded() && 
				!a_refr->IsMarkedForDeletion() && 
				!a_refr->IsDisabled() && 
				!a_refr->IsDeleted()
			);
		}

		// Is the given actor currently a Vampire Lord?
		inline bool IsVampireLord(RE::Actor* a_actor) 
		{
			if (!a_actor || !a_actor->race)
			{
				return false;
			}

			return a_actor->race->formEditorID == "DLC1VampireBeastRace"sv;
		}

		// Is the given actor currently a Werewolf?
		inline bool IsWerewolf(RE::Actor* a_actor) 
		{
			if (!a_actor || !a_actor->race)
			{
				return false;
			}

			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
			{
				return 
				(
					a_actor->race->formEditorID == "_00E_Theriantrophist_PlayerWerewolfRace"sv || 
					a_actor->race->formEditorID == "WerewolfBeastRace"sv
				);
			}
			else
			{
				return a_actor->race->formEditorID == "WerewolfBeastRace"sv;
			}
		}

		// Return true if a menu is open that stops the player from moving.
		inline bool OpenMenuStopsMovement()
		{
			auto ui = RE::UI::GetSingleton();
			return
			{
				ui &&
				(
					ui->GameIsPaused() ||
					ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::LevelUpMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::MapMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::StatsMenu::MENU_NAME)
				)
			};
		}

		// Have the given source actor perform the given idle directed at the given target actor.
		// If no target actor is specified, the source actor is used as the target actor.
		inline bool PlayIdle(RE::TESIdleForm* a_idle, RE::Actor* a_sourceActor, RE::Actor* a_targetActor = nullptr)
		{
			if (!a_idle || !a_sourceActor || !a_sourceActor->currentProcess)
			{
				return false;
			}

			if (!a_targetActor)
			{
				a_targetActor = a_sourceActor;
			}

			// Setting a_arg5 to false and a_arg6 to true seems 
			// to sometimes bypass certain idle conditions,
			// such as target HP level and random trigger chance.
			return 
			(
				a_sourceActor->currentProcess->SetupSpecialIdle
				(
					a_sourceActor, 
					RE::DEFAULT_OBJECT::kActionIdle, 
					a_idle, 
					false, 
					true, 
					a_targetActor
				)
			);
		}

		// Lookup the idle given by its name.
		// Then have the given source actor perform the given idle directed at the given target actor.
		// If no target actor is specified, the source actor is used as the target actor.
		inline bool PlayIdle(const RE::BSFixedString& a_idleName, RE::Actor* a_sourceActor, RE::Actor* a_targetActor = nullptr)
		{
			auto idle = RE::TESForm::LookupByEditorID<RE::TESIdleForm>(a_idleName);
			return PlayIdle(idle, a_sourceActor, a_targetActor);
		}

		// Convert the given pitch and yaw angles (Cartesian convention) to a 3D direction vector.
		// https://stackoverflow.com/questions/1568568/how-to-convert-euler-angles-to-directional-vector
		inline RE::NiPoint3 RotationToDirectionVect(const float& a_pitch, const float& a_yaw)
		{
			return 
			(
				RE::NiPoint3
				(
					cosf(a_pitch) * cosf(a_yaw), 
					cosf(a_pitch) * sinf(a_yaw), 
					sinf(a_pitch)
				)
			);
		}	

		// Set the boolean game setting, given by the setting name, to the given value.
		// Return true if request succeeded.
		inline bool SetGameSettingBool(const RE::BSFixedString&& a_settingName, const bool& a_value) 
		{
			if (a_settingName.empty())
			{
				return false;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting && setting->GetBool())
				{
					setting->data.b = a_value;
					gSettings->WriteSetting(setting);
					return true;
				}
			}

			return false;
		}

		// Set the float game setting, given by the setting name, to the given value.
		// Return true if request succeeded.
		inline bool SetGameSettingFloat(const RE::BSFixedString&& a_settingName, const float& a_value)
		{
			if (a_settingName.empty())
			{
				return false;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting && setting->GetFloat())
				{
					setting->data.f = a_value;
					gSettings->WriteSetting(setting);
					return true;
				}
			}

			return false;
		}

		// Set the signed integer game setting, given by the setting name, to the given value.
		// Return true if request succeeded.
		inline bool SetGameSettingInt(const RE::BSFixedString&& a_settingName, const int32_t& a_value)
		{
			if (a_settingName.empty())
			{
				return false;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting && setting->GetSInt())
				{
					setting->data.i = a_value;
					gSettings->WriteSetting(setting);
					return true;
				}
			}

			return false;
		}

		// Set the string game setting, given by the setting name, to the given value.
		// Return true if request succeeded.
		inline bool SetGameSettingString(const RE::BSFixedString&& a_settingName, std::string&& a_value)
		{
			if (a_settingName.empty())
			{
				return false;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting && setting->GetString())
				{
					setting->data.s = a_value.data();
					gSettings->WriteSetting(setting);
					return true;
				}
			}

			return false;
		}

		// Set the unsigned integer game setting, given by the setting name, to the given value.
		// Return true if request succeeded.
		inline bool SetGameSettingUInt(const RE::BSFixedString&& a_settingName, const uint32_t& a_value)
		{
			if (a_settingName.empty())
			{
				return false;
			}

			if (auto gSettings = RE::GameSettingCollection::GetSingleton(); gSettings)
			{
				if (auto setting = gSettings->GetSetting(a_settingName.c_str()); setting && setting->GetUInt())
				{
					setting->data.u = a_value;
					gSettings->WriteSetting(setting);
					return true;
				}
			}

			return false;
		}

		// Send a critical hit event constructed with the given aggressor actor,
		// source weapon, and sneak attack flag (if requested).
		inline void SendCriticalHitEvent(RE::TESObjectREFR* a_aggressor, RE::TESObjectWEAP* a_weapon, const bool& a_sneakHit)
		{
			if (auto critSource = RE::CriticalHit::GetEventSource(); critSource)
			{
				// Construct and send event.
				std::unique_ptr<RE::CriticalHit::Event> critEvent = std::make_unique<RE::CriticalHit::Event>();
				std::memset(critEvent.get(), 0, sizeof(RE::CriticalHit::Event));
				if (critEvent && critEvent.get())
				{
					critEvent->aggressor = a_aggressor;
					critEvent->weapon = a_weapon;
					critEvent->sneakHit = a_sneakHit;
					critSource->SendEvent(critEvent.get());
				}

				critEvent.reset();
			}
		}

		// Send a hit event constructed with the given cause refr,
		// target refr, source FID, projectile FID, and hit flags.
		inline void SendHitEvent(RE::TESObjectREFR* a_cause, RE::TESObjectREFR* a_target, const RE::FormID& a_sourceFID, const RE::FormID& a_projFID, RE::stl::enumeration<RE::TESHitEvent::Flag, std::uint8_t> a_flags)
		{
			if (auto sesh = RE::ScriptEventSourceHolder::GetSingleton(); sesh)
			{
				// Construct and send event.
				std::unique_ptr<RE::TESHitEvent> hitEvent = std::make_unique<RE::TESHitEvent>();
				std::memset(hitEvent.get(), 0, sizeof(RE::TESHitEvent));
				if (hitEvent && hitEvent.get())
				{
					hitEvent->cause = RE::TESObjectREFRPtr(a_cause);
					hitEvent->flags = a_flags;
					hitEvent->projectile = a_projFID;
					hitEvent->source = a_sourceFID;
					hitEvent->target = RE::TESObjectREFRPtr(a_target);
					sesh->SendEvent<RE::TESHitEvent>(hitEvent.get());
				}

				hitEvent.reset();
			}
		}

		// NOTE: 
		// Not sure if this sky mode change is necessary, 
		// since some interior cells still have camera proximity-based fog afterward.
		// May remove later.
		// Attempt to remove fog from interior cells by setting the cell sky mode to 'SkyDoneOnly'.
		inline void SetSkyboxOnlyForInteriorModeCell(const RE::TESObjectCELL* a_cell)
		{
			if (a_cell && a_cell->formID != 0)
			{
				if (auto tes = RE::TES::GetSingleton(); 
					tes && tes->sky && *tes->sky->mode != RE::Sky::Mode::kInterior)
				{
					tes->sky->mode.reset
					(
						RE::Sky::Mode::kFull,
						RE::Sky::Mode::kInterior, 
						RE::Sky::Mode::kNone, 
						RE::Sky::Mode::kSkyDomeOnly
					);
					if (a_cell->IsExteriorCell()) 
					{
						tes->sky->mode.set(RE::Sky::Mode::kFull);
					}
					else
					{
						tes->sky->mode.set(RE::Sky::Mode::kSkyDomeOnly);
					}
				}
			}
		}

		// Set all the characters in the given string to lowercase.
		inline void ToLowercase(std::string& a_stringOut) 
		{
			std::transform
			(
				a_stringOut.begin(), 
				a_stringOut.end(), 
				a_stringOut.begin(), 
				[](char& a_char) 
				{ 
					return std::tolower(a_char); 
				}
			);
		}

		//=============================================================================================================
	
		//====================
		//[Utility Functions]:
		//====================

		// Run task and wait until complete.
		// Can choose to send a UI task instead.
		// NOTE: DO NOT run on a main thread or the game will lock up.
		void AddSyncedTask(std::function<void()> a_func, bool a_isUITask = false);

		// Favorite/unfavorite the given form for the given actor.
		void ChangeFormFavoritesStatus(RE::Actor* a_actor, RE::TESForm* a_form, const bool& a_shouldFavorite);

		// Add/remove hotkey to/from the given form for the given actor.
		// Set -1 as the hotkey index to remove the hotkey.
		void ChangeFormHotkeyStatus(RE::Actor* a_actor, RE::TESForm* a_form, const int8_t& a_hotkeySlotToSet);

		// Add/remove given perk to/from P1.
		void ChangeP1Perk(RE::BGSPerk* a_perk, bool&& a_add);

		// Add/remove given perk to/from the given actor.
		bool ChangePerk(RE::Actor* a_actor, RE::BGSPerk* a_perk, bool&& a_add);

		// Creates a LS/RS thumbstick event using the provided user event name
		// and stick X, Y displacement values. 
		// Based on the Create() function for ButtonEvents.
		// NOTE: Must be free'd by the caller.
		RE::ThumbstickEvent* CreateThumbstickEvent(const RE::BSFixedString& a_userEvent, float a_xValue, float a_yValue, bool a_isLS);

		// Helper function that iterates through all refrs in the given cell
		// that are within the given radius from the provided origin position.
		// Can treat the given radius as a 3D or XY distance limit.
		// Runs the provided function on each iterated refr.
		void ForEachReferenceInCellWithinRange(RE::TESObjectCELL* a_cell, RE::NiPoint3 a_originPos, float a_radius, const bool& a_use3DDist, std::function<RE::BSContainer::ForEachResult(RE::TESObjectREFR* a_refr)> a_callback);

		// Credits to Ryan for the CommonLibSSE TES::ForEachReferenceInRange() function, and
		// Shrimperator for their adaptation of the function to use an origin position instead of an origin reference:
		// https://gitlab.com/Shrimperator/skyrim-mod-betterthirdpersonselection/-/blob/main/src/lib/Util.cpp#L969
		// NOTE: Cannot queue tasks in the passed-in filter function. 
		// Game will freeze.
		void ForEachReferenceInRange(RE::NiPoint3 a_originPos, float a_radius, const bool& a_use3DDist, std::function<RE::BSContainer::ForEachResult(RE::TESObjectREFR* a_refr)> a_callback);

		// Helper function that gets the approximated pixel height of the actor 
		// at the current camera position.
		// Actor's lower bound is their world position
		// and their upper bound is their head node or world position offset by their height.
		float GetActorPixelHeight(RE::Actor* a_actor, const RE::NiPoint3& a_headWorldPos, const RE::NiPoint3& a_centerWorldPos, const RE::NiPoint3& a_boundExtents);
		
		// Helper function that gets the approximated pixel width of the actor 
		// at the current camera position.
		// Actor's upper/lower bound is their center position offset by +- their X bound extent.
		float GetActorPixelWidth(RE::Actor* a_actor, const RE::NiPoint3& a_centerWorldPos, const RE::NiPoint3& a_boundExtents);

		// Get all skill levels for the given actor.
		SkillList GetActorSkillLevels(RE::Actor* a_actor);

		// Get an approximation of the refr's 3D bounds pixel height (vert axis) or width based on the current camera orientation.
		float GetBoundPixelDist(RE::TESObjectREFR* a_refr, bool&& a_vertAxis);

		// Get the X, Y, and Z euler angles from the given rotation matrix.
		// Pitch and yaw angles returned in the NiPoint follow the game's conventions for both.
		RE::NiPoint3 GetEulerAnglesFromRotMatrix(const RE::NiMatrix3& a_matrix);

		// Get the actor's eye position or approximate it as their looking at position
		// if they do not have a eye body part.
		// Credits to ersh1 for the method of getting the body part data:
		// https://github.com/ersh1/TrueDirectionalMovement/blob/master/src/Utils.cpp
		RE::NiPoint3 GetEyePosition(RE::Actor* a_actor);

		// Get the given 3D object or refr's associated rigid body.
		RE::hkRefPtr<RE::hkpRigidBody> GethkpRigidBody(RE::NiAVObject* a_node3D);

		RE::hkRefPtr<RE::hkpRigidBody> GethkpRigidBody(RE::TESObjectREFR* a_refr);

		// Get the world position for the given actor's head body part.
		RE::NiPoint3 GetHeadPosition(RE::Actor* a_actor);

		// Get the highest count ammo and its count in the given actor's inventory.
		// Can exclusively check arrows or check bolts.
		std::pair<RE::TESAmmo*, int32_t> GetHighestCountAmmo(RE::Actor* a_actor, const bool& a_forBows);

		// Get the highest damage ammo and its count in the given actor's inventory.
		// Can exclusively check arrows or check bolts.
		std::pair<RE::TESAmmo*, int32_t> GetHighestDamageAmmo(RE::Actor* a_actor, const bool& a_forBows);

		// Get the hotkey index, if any (-1 otherwise), for the given form.
		int32_t GetHotkeyForForm(RE::Actor* a_actor, RE::TESForm* a_form);

		// Get a smart pointer to the game's NiCamera.
		RE::NiPointer<RE::NiCamera> GetNiCamera();

		// Helper function that gets the approximated pixel height
		// of the given refr at the current camera orientation.
		float GetObjectPixelHeight(RE::TESObjectREFR* a_refr, const RE::NiPoint3& a_topExtentWorldPos, const RE::NiPoint3& a_centerWorldPos, const RE::NiPoint3& a_boundExtents);

		// Helper function that gets the approximated pixel width
		// of the given refr at the current camera orientation.
		float GetObjectPixelWidth(RE::TESObjectREFR* a_refr, const RE::NiPoint3& a_centerWorldPos, const RE::NiPoint3& a_boundExtents);

		// Get associated refr from the given 3D object.
		RE::TESObjectREFR* GetRefrFrom3D(RE::NiAVObject* a_obj3D);
		
		// Sets the shortened skeleton model name string for the given race
		// through the given outparam.
		void GetSkeletonModelNameForRace(RE::TESRace* a_race, std::string& a_skeletonNameOut);

		// Get the amount of stealth points that the given player actor 
		// has lost from the other given actor's combat group.
		// Used as the overall detection level while the player is in combat.
		float GetStealthPointsLost(RE::Actor* a_playerActor, RE::Actor* a_fromActor);

		// Get the actor's torso position 
		// or approximate as their refr position + half of the actors height 
		// or as their world bound center position,
		// if they do not have a torso body part.
		// Credits to ersh1 for the method of getting the body part data:
		// https://github.com/ersh1/TrueDirectionalMovement/blob/master/src/Utils.cpp
		RE::NiPoint3 GetTorsoPosition(RE::Actor* a_actor);

		// Using a couple raycasts, get collidable points (above, below) the given point.
		std::pair<float, float> GetVertCollPoints(const RE::NiPoint3& a_point);

		// Using a couple raycasts, get collidable points (above, below) 
		// the given point offset by the given hull size.
		std::pair<float, float> GetVertCollPoints(const RE::NiPoint3& a_point, const float& a_hullSize);

		// Get the weapon type that corresponds to the given keyword.
		RE::WEAPON_TYPE GetWeaponTypeFromKeyword(RE::BGSKeyword* a_keyword);

		// Check if the given observer has an LOS to the target refr.
		// The criteria for having LOS varies if the observer is attempting
		// select the target refr with their crosshair 
		// or if the LOS check was requested for refr activation.
		// Can also check/ignore the crosshair position.
		bool HasLOS(RE::TESObjectREFR* a_targetRefr, RE::Actor* a_observer, bool a_forCrosshairSelection, bool a_checkCrosshairPos, const RE::NiPoint3& a_crosshairWorldPos);
		
		// Helper function which performs a series of raycasts
		// that all start along a vertical ray segment that is bounded 
		// along the observer's vertical axis.
		// Can exclude certain 3D objects from consideration,
		// and choose to raycast at the crosshair position.
		// If any raycast hits the target refr, the observer has LOS on the target.
		bool HasRaycastLOSAlongObserverAxis(RE::TESObjectREFR* a_observer, RE::TESObjectREFR* a_targetRefr, const std::vector<RE::NiAVObject*>& a_excluded3DObjects, bool a_checkCrosshairPos, const RE::NiPoint3& a_crosshairWorldPos, bool&& a_showDebugDraws = false);

		// Helper function which performs a series of raycasts
		// from the given start position to the target's
		// refr data position, 3D world position,
		// 3D center position, and potentially the crosshair position, if requested.
		// Can exclude certain 3D objects from consideration.
		// If any raycast hits the target refr, the observer has LOS on the target.
		bool HasRaycastLOSFromPos(const RE::NiPoint3& a_startPos, RE::TESObjectREFR* a_targetRefr, const std::vector<RE::NiAVObject*>& a_excluded3DObjects, bool a_checkCrosshairPos, const RE::NiPoint3& a_crosshairWorldPos, bool&& a_showDebugDraws = false);

		// Get interpolated values using various interpolation functions.
		// - Previous and next set the interpolation bounds.
		// - Ratio gives the proportion of the range 
		// between the previous and next endpoints
		// to use for the base interpolated value,
		// - Power modifies the characteristics of the interpolation curve.
		// For example, a power of 1 is linear and is the same as using std::lerp.
		float InterpolateEaseIn(const float& a_prev, const float& a_next, float a_ratio, const float& a_pow);

		float InterpolateEaseOut(const float& a_prev, const float& a_next, float a_ratio, const float& a_pow);

		float InterpolateEaseInEaseOut(const float& a_prev, const float& a_next, float a_ratio, const float& a_pow);

		// Interpolate one rotation matrix to another.
		// Full credits once again go to ersh1 for Precision:
		// https://github.com/ersh1/Precision/blob/main/
		// NOTE: Unused for now until I get back to improving node rotation blending.
		RE::NiMatrix3 InterpolateRotMatrix(const RE::NiMatrix3& a_matA, const RE::NiMatrix3& a_matB, const float& a_ratio);

		// Is the given form favorited by the given actor?
		bool IsFavorited(RE::Actor* a_actor, RE::TESForm* a_form);

		// Is the given form hotkeyed in the given slot for the given actor?
		bool IsHotkeyed(RE::Actor* a_actor, RE::TESForm* a_form);

		// Is the given point in front of the camera's collision position?
		bool IsInFrontOfCam(const RE::NiPoint3& a_point);

		// Is the given refr lootable by co-op players
		// (can be moved into a player's inventory directly through activation)?
		bool IsLootableRefr(RE::TESObjectREFR* a_refr);

		// Is the given refr usable for target selection or activation?
		bool IsSelectableRefr(RE::TESObjectREFR* a_refr);

		// Construct a rotation matrix from the given axis 
		// and rotation angle about that axis.
		RE::NiMatrix3 MatrixFromAxisAndAngle(RE::NiPoint3 a_axis, const float& a_angle);

		// Return true if no temporary menus are open.
		// Temporary menus are considered as menus that add a non-gameplay/TFC context 
		// onto the menu context stack, plus the 'LootMenu' from the QuickLoot mod.
		bool MenusOnlyAlwaysOpen();

		// Returns true if only 'always open' menus, 
		// the Dialogue Menu, or the LootMenu are open.
		// All such menus are unpaused.
		bool MenusOnlyAlwaysUnpaused();

		// Add the given perk to P1.
		// Return true if successful.
		bool Player1AddPerk(RE::BGSPerk* a_perk);

		// Return true if P1 has the given perk
		// in the P1 singleton perk list.
		bool Player1PerkListHasPerk(RE::BGSPerk* a_perk);
		
		// Remove the given perk from P1.
		// Return true if successful.
		bool Player1RemovePerk(RE::BGSPerk* a_perk);

		// Check if the given point is within the camera's frustum.
		// Can set a pixel margin around the screen beyond which
		// points are considered off-screen.
		bool PointIsOnScreen(const RE::NiPoint3& a_point, float&& a_marginPixels = 0.0f);

		// Check if the given point is within the camera's frustum.
		// Also set the 2D screen position that corresponds to the given
		// world position through the outparam.
		// Can set a pixel margin around the screen beyond which
		// points are considered off-screen, and also clamp
		// the outparam 2D position to the screen's X and Y bounds.
		bool PointIsOnScreen(const RE::NiPoint3& a_point, RE::NiPoint3& a_screenPointOut, float&& a_marginPixels = 0.0f, bool&& a_shouldClamp = true);

		// Ragdoll the given actor by triggering a knock explosion 
		// of the given force at the given position.
		// If triggering a downed ragdoll, paralyze the essential actor first
		// to prevent them from getting up.
		// Used for setting players as 'downed' if the revive system is enabled.
		void PushActorAway(RE::Actor* a_actorToPush, const RE::NiPoint3& a_contactPos, const float& a_force, bool&& a_downedRagdoll = false);

		// Full credits to ersh1:
		// https://github.com/ersh1/Precision/blob/main/src/Utils.cpp#L139
		// Slerp quaternion A to B.
		RE::NiQuaternion QuaternionSlerp(const RE::NiQuaternion& a_quatA, const RE::NiQuaternion& a_quatB, double a_t);

		// Full credits to ersh1:
		// https://github.com/ersh1/Precision/blob/main/src/Utils.cpp#L201
		// Convert a quaternion to a rotation matrix.
		RE::NiMatrix3 QuaternionToRotationMatrix(const RE::NiQuaternion& a_quat);

		// Helper function which searches for an associated refr
		// by first checking the current node,
		// and then recursing up the node tree if nothing is found, 
		// checking one direct parent node at a time.
		// Return early if max recursion depth is reached.
		RE::TESObjectREFR* RecurseForRefr(RE::NiNode* a_parentNode, uint8_t a_recursionDepth = 0);

		// Reset object fade value(s) for all refrs in the given cell.
		void ResetFadeOnAllObjectsInCell(RE::TESObjectCELL* a_cell);

		// Reset the third person camera's orientation.
		void ResetTPCamOrientation();

		// Rotate the given vector about the given axis by the given angle.
		// Given given given. I like givin'.
		// Vector is unitized and set through the outparam.
		// Precondition: Angle must be in radians.
		// How to rotate a vector in 3d space around arbitrary axis, 
		// URLs: https://math.stackexchange.com/q/4034978,
		// https://en.wikipedia.org/wiki/Rotation_matrix#cite_note-5
		void RotateVectorAboutAxis(RE::NiPoint3& a_vectorOut, RE::NiPoint3 a_axis, float a_angle);

		// Return the quaternion that corresponds to the given rotation matrix.
		// https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
		RE::NiQuaternion RotationMatrixToQuaternion(const RE::NiMatrix3& a_matrix);

		// Run player action console command that corresponds to 
		// the given default object applied to the given actor.
		void RunPlayerActionCommand(RE::DEFAULT_OBJECT&& a_defObj, RE::Actor* a_actor);

		// Instantly construct and send a button event using the given device,
		// user event name, button input mask, pressed value, and held time.
		// Can also toggle AI driven to false temporarily for P1
		// while sending and handling the event.
		// Can also set a flag via the InputEvent's pad24 member
		// to signal the event as proxied by P1, meaning it will be allowed 
		// through when input event filtering is done via the MenuControls hook.
		void SendButtonEvent(RE::INPUT_DEVICE a_inputDevice, RE::BSFixedString a_ueString, uint32_t a_buttonMask, float a_pressedValue, float a_heldTimeSecs, bool a_toggleAIDriven, bool a_setPadProxiedFlag = false);
		
		// Send a crosshair event with the given crosshair refr to set.
		// Can be used to open/close the QuickLoot LootMenu.
		void SendCrosshairEvent(RE::TESObjectREFR* a_crosshairRefrToSet);

		// Send the given input event.
		void SendInputEvent(std::unique_ptr<RE::InputEvent* const>& a_inputEvent);

		// Construct and send a LS/RS thumbstick event using the given
		// user event name and analog stick X, Y displacements.
		void SendThumbstickEvent(const RE::BSFixedString& a_ueString, float a_xValue, float a_yValue, bool a_isLS);

		// Set the game's current camera position to the given one.
		void SetCameraPosition(RE::TESCamera* a_cam, const RE::NiPoint3& a_position);

		// Set the game's current camera rotation to the given pitch and yaw.
		void SetCameraRotation(RE::TESCamera* a_cam, const float& a_pitch, const float& a_yaw);

		// Set the given actor's linear velocity to the given velocity.
		void SetLinearVelocity(RE::Actor* a_actor, RE::NiPoint3 a_velocity);

		// Change P1's AI driven flag to the requested value.
		// Return true if the request produced the desired change.
		bool SetPlayerAIDriven(const bool&& a_shouldSet);

		// Set the actor's position within the actor's 
		// current worldspace and parent cell.
		// Precondition: 
		// Position is in game coordinates, not havok coordinates.
		// NOTE: Use MoveTo instead if desiring to 
		// move the actor from one cell/worldspace to another.
		void SetPosition(RE::Actor* a_actor, RE::NiPoint3 a_position);

		// Set the rotation matrix outparam to a rotation matrix 
		// constructed from the given game pitch and yaw angles (game's convention).
		void SetRotationMatrixPY(RE::NiMatrix3& a_rotMatrix, const float& a_pitch, const float& a_yaw);

		// Set the rotation matrix outparam to a rotation matrix
		// constructed from the given pitch, roll, and yaw angles (game's convention).
		void SetRotationMatrixPYR(RE::NiMatrix3& a_rotMatrix, const float& a_pitch, const float& a_yaw, const float& a_roll);

		// Set the rotation matrix outparam to a rotation matrix
		// constructed from the given axis unit vectors
		// rotated about the given pitch, roll, and yaw angles (game's convention).
		void SetRotationMatrixPYRAndAxes(RE::NiMatrix3& a_rotMatrix, const RE::NiPoint3& a_xAxis, const RE::NiPoint3& a_yAxis, const RE::NiPoint3& a_zAxis, const float& a_pitch, const float& a_yaw, const float& a_roll);

		// If the spell has an image space modifier 
		// and is a self-targeted concentration spell, return true.
		// The spell should then be cast by one of P1's magic casters.
		bool ShouldCastWithP1(RE::SpellItem* a_spell);

		// Effect shader and hit art code:
		// Credits to po3:
		// Adapted from their papyrus extender code found here:
		// https://github.com/powerof3/PapyrusExtenderSSE/blob/master/src/Papyrus/Util/Graphics.cpp#L42
		
		// Start playing the given effect shader on the given refr.
		// Can set a duration to play the effect for (-1 to play indefinitely).
		void StartEffectShader(RE::TESObjectREFR* a_refr, RE::TESEffectShader* a_shader, const float& a_timeSecs = -1.0f);
		
		// Start playing the given hit art between the given refrs.
		// Can set a duration to play the effect for (-1 to play indefinitely).
		void StartHitArt(RE::TESObjectREFR* a_refr, RE::BGSArtObject* a_artObj, RE::TESObjectREFR* a_facingRefr, const float& a_timeSecs = -1.0f, bool&& a_faceTarget = false, bool&& a_attachToCamera = false);

		// Stop all effect shaders currently playing on the given refr.
		void StopAllEffectShaders(RE::TESObjectREFR* a_refr);

		// Stop all hit art effects currently playing on the given refr.
		void StopAllHitArtEffects(RE::TESObjectREFR* a_refr);

		// Stop the given effect shader playing on the given refr.
		// Can delay removal of the effect by a certain number of seconds 
		// (-1 to stop instantly).
		void StopEffectShader(RE::TESObjectREFR* a_refr, RE::TESEffectShader* a_shader, float&& a_delayedStopSecs = -1.0f);

		// Stop the given hit art playing on the given refr.
		// Can delay removal of the effect by a certain number of seconds
		// (-1 to stop instantly).
		void StopHitArt(RE::TESObjectREFR* a_refr, RE::BGSArtObject* a_artObj, float&& a_delayedStopSecs = -1.0f);

		// Teleport the the requesting actor to the target actor.
		// Drop an entry and exit portal down during the process.
		void TeleportToActor(RE::Actor* a_teleportingActor, RE::Actor* a_target);

		// Toggle all of P1's controls on or off.
		void ToggleAllControls(bool a_shouldEnable);

		// Traverse P1's entire perk tree with modifications in mind for the given player actor.
		// Run the given visitor function on each perk tree node.
		void TraverseAllPerks(RE::Actor* a_actor, std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> a_treeVisitor);

		// Run a visitor function on each child node 
		// while recursively moving down the node tree from the given starting 3D object's node.
		void TraverseChildNodesDFS(RE::NiAVObject* a_current3D, std::function<void(RE::NiAVObject* a_node)> a_visitor);

		// Traverse a single skill's perk tree starting at the given node,
		// with modifications in mind for the given player actor.
		// Run the given visitor function on each perk tree node.
		// NOTE: The first node passed in should always be the root node
		// for the perk tree, which does not have an associated perk
		// and simply points to the first perk node in the tree
		// (its first child node).
		void TraversePerkTree(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor, std::function<void(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor)> a_visitor);

		// Trigger a skill level up message by spoofing a level up 
		// to the given level for the given skill.
		// Returns true if successful.
		bool TriggerFalseSkillLevelUp(const RE::ActorValue& a_avSkill, const Skill& a_skill, const std::string& a_skillName, const float& a_newLevel);

		// Get the screen point (Z component used for the position's depth value)
		// that corresponds to the given world position.
		// Can clamp the returned screen position to the screen's X, Y bounds if requested.
		RE::NiPoint3 WorldToScreenPoint3(const RE::NiPoint3& a_worldPos, bool&& a_shouldClamp = true);
	};
}

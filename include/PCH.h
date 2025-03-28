#pragma once

// IMPORTANT DEV NOTE: 
// Ensure 'ALYSLC_DEBUG_MODE' is not defined before submitting a PR or committing changes.
#define ALYSLC_DEBUG_MODE
#ifdef ALYSLC_DEBUG_MODE
	#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else 
	#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

#pragma warning(push)
#if defined(FALLOUT4)
#include "RE/Fallout.h"
#include "F4SE/F4SE.h"
#define SKSE F4SE
#define SKSEAPI F4SEAPI
#define SKSEPlugin_Load F4SEPlugin_Load
#define SKSEPlugin_Query F4SEPlugin_Query
#define RUNTIME RUNTIME_1_10_163
#else
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#if defined(SKYRIMAE)
#define RUNTIME 0
#elif defined(SKYRIMVR)
#define RUNTIME SKSE::RUNTIME_VR_1_4_15_1
#else
#define RUNTIME SKSE::RUNTIME_1_5_97
#endif
#endif

#include <format>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <SimpleIni.h>

#ifdef NDEBUG
#include <spdlog/sinks/basic_file_sink.h>
#else
#include <spdlog/sinks/msvc_sink.h>
#endif
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

#pragma warning(pop)

using namespace std::literals;

namespace logger = SKSE::log;

namespace util
{
	using SKSE::stl::report_and_fail;
}

#define DLLEXPORT __declspec(dllexport)

#ifdef SKYRIM_AE
#define OFFSET(se, ae) ae
#define OFFSET_3(se, ae, vr) ae
#elif SKYRIMVR
#define OFFSET(se, ae) se
#define OFFSET_3(se, ae, vr) vr
#else
#define OFFSET(se, ae) se
#define OFFSET_3(se, ae, vr) se
#endif

#include "Version.h"

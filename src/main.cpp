
#include "Hooks.h" 
#include <CameraManager.h>
#include <Compatibility.h>
#include <DebugAPI.h>
#include <Events.h>
#include <MenuInputManager.h>
#include <Proxy.h>
#include <Serialization.h>
#include <Util.h>
#include "../extern/CommonLibSSE/src/SKSE/API.cpp"

const SKSE::LoadInterface* g_loadInterface = nullptr;

void SKSEMessageHandler(SKSE::MessagingInterface::Message* msg)
{
	switch (msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
	{
		logger::info("[MAIN] Data loaded.");
		// Install all hooks.
		ALYSLC::Hooks::Install();
		// Add event sinks for all necessary events.
		ALYSLC::Events::RegisterEvents();
		// Register debug overlay menu.
		ALYSLC::DebugOverlayMenu::Register();
		break;
	}
	case SKSE::MessagingInterface::kNewGame:
	{
		logger::info("[MAIN] New game.");
		// Set default serialization data through the Load() function.
		if (SKSE::SerializationInterface* intfc = SKSE::detail::APIStorage::get().serializationInterface; intfc)
		{
			logger::info("[MAIN] New game. Setting default serialization data on load.");
			ALYSLC::SerializationCallbacks::Load(intfc);
		}

		// Attempt to load the debug overlay.
		ALYSLC::DebugOverlayMenu::Load();
		break;
	}
	case SKSE::MessagingInterface::kPostLoad:
	{
		logger::info("[MAIN] Post load.");
		break;
	}
	case SKSE::MessagingInterface::kPostLoadGame:
	{
		logger::info("[MAIN] Post load game.");
		// Run compatibility checks and initialization.
		ALYSLC::EnderalCompat::CheckForEnderalSSE();
		ALYSLC::MCOCompat::CheckForMCO(g_loadInterface);
		ALYSLC::MiniMapCompat::CheckForMiniMap();
		ALYSLC::PersistentFavoritesCompat::CheckForPersistentFavorites();
		ALYSLC::PrecisionCompat::RequestPrecisionAPIs(g_loadInterface);
		ALYSLC::QuickLootCompat::CheckForQuickLoot(g_loadInterface);
		ALYSLC::RequiemCompat::CheckForRequiem(g_loadInterface);
		ALYSLC::SkyrimsParagliderCompat::CheckForParaglider();
		ALYSLC::TKDodgeCompat::CheckForTKDodge();
		ALYSLC::TrueDirectionalMovementCompat::CheckForTrueDirectionalMovement(g_loadInterface);
		ALYSLC::TrueHUDCompat::RequestTrueHUDAPIs(g_loadInterface);
		// Attempt to load the debug overlay.
		ALYSLC::DebugOverlayMenu::Load();
		break;
	}
	case SKSE::MessagingInterface::kPostPostLoad:
	{
		logger::info("[MAIN] Post-post load.");
		break;
	}
	case SKSE::MessagingInterface::kPreLoadGame:
	{
		logger::info("[MAIN] Pre load game.");
		// Register for P1 positioning events.
		ALYSLC::CoopPositionPlayerEventHandler::Register();
		break;
	}
	default:
	{
		break;
	}
	}
}

void InitializeLog()
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		util::report_and_fail("[MAIN] Failed to find standard logging directory"sv);
	}

	*path /= fmt::format("{}.log"sv, Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
	const auto level = spdlog::level::trace;
#else
	const auto level = spdlog::level::debug;
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(level);
	log->flush_on(level);

	spdlog::set_default_logger(std::move(log));
	// spdlog::set_pattern("[%l] %v"s);
	// spdlog::set_pattern("[%H:%M:%S:%e] %v"s);
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info("[MAIN] Initialized logger for {} v{}", Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {};
#endif

	logger::info("[MAIN] Adventurers Like You: Skyrim Local Co-op Mod loaded!");
	// Create global data singleton before doing anything else.
	ALYSLC::GlobalCoopData::GetSingleton();

	g_loadInterface = a_skse;
	SKSE::Init(a_skse);
	InitializeLog();
	SKSE::AllocTrampoline(1 << 8);

	if (auto messaging = SKSE::GetMessagingInterface(); !messaging->RegisterListener("SKSE", SKSEMessageHandler))
	{
		logger::error("[MAIN] ERR: Could not register messaging interface listener.");
		return false;
	}

	if (auto papyrus = SKSE::GetPapyrusInterface(); !papyrus || !papyrus->Register(ALYSLC::CoopLib::RegisterFuncs))
	{
		logger::error("[MAIN] ERR: Could not get Papyrus interface or register Papyrus functions.");
		return false;
	}

	if (auto serialization = SKSE::GetSerializationInterface(); !serialization) 
	{
		logger::error("[MAIN] ERR: Could not get serialization interface.");
		return false;
	}
	else
	{
		logger::info("[MAIN] Setting serialization callbacks.");
		// Set serialization ID and callbacks.
		serialization->SetUniqueID(ALYSLC::Hash("ALYSLC"));
		serialization->SetLoadCallback(ALYSLC::SerializationCallbacks::Load);
		serialization->SetRevertCallback(ALYSLC::SerializationCallbacks::Revert);
		serialization->SetSaveCallback(ALYSLC::SerializationCallbacks::Save);
	}

	return true;
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("ALYSLC");
	v.AuthorName("Jossarian");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "ALYSLC";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor())
	{
		logger::error("[MAIN] Loaded in editor, marking as incompatible."sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver
#ifndef SKYRIMVR
		< SKSE::RUNTIME_1_5_39
#else
		> SKSE::RUNTIME_VR_1_4_15_1
#endif
	)
	{
		logger::error(FMT_STRING("[MAIN] Unsupported runtime version {}."sv), ver.string());
		return false;
	}

	return true;
}
#endif

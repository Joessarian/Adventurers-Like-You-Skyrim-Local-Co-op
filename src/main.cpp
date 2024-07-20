
#include "Hooks.h" 
#include <CameraManager.h>
#include <Compatibility.h>
#include <DebugAPI.h>
#include <Events.h>
#include <MenuInputManager.h>
#include <Proxy.h>
#include <Serialization.h>
#include <Util.h>
#include "../external/CommonLibSSE/src/SKSE/API.cpp"

const SKSE::LoadInterface* g_loadInterface = nullptr;

void SKSEMessageHandler(SKSE::MessagingInterface::Message* msg)
{
	switch (msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
	{
		logger::info("[Main] Data loaded.");
		// Install all hooks.
		ALYSLC::Hooks::Install();
		// Add event sinks for all necessary events.
		ALYSLC::Events::RegisterEvents();
		// Register debug overlay menu.
		ALYSLC::DebugOverlayMenu::Register();
		// Set logger level for the mod.
		ALYSLC::Settings::SetLoggerLevel();
		break;
	}
	case SKSE::MessagingInterface::kNewGame:
	{
		logger::info("[Main] New game.");
		// Set default serialization data through the Load() function.
		if (SKSE::SerializationInterface* intfc = SKSE::detail::APIStorage::get().serializationInterface; intfc)
		{
			logger::info("[Main] New game. Setting default serialization data on load.");
			ALYSLC::SerializationCallbacks::Load(intfc);
		}

		// Attempt to load the debug overlay.
		ALYSLC::DebugOverlayMenu::Load();
		break;
	}
	case SKSE::MessagingInterface::kPostLoad:
	{
		logger::info("[Main] Post load.");
		// Run compatibility checks and initialization.
		ALYSLC::EnderalCompat::CheckForEnderalSSE();
		ALYSLC::MiniMapCompat::CheckForMiniMap();
		ALYSLC::PrecisionCompat::RequestPrecisionAPIs(g_loadInterface);
		ALYSLC::QuickLootCompat::CheckForQuickLoot(g_loadInterface);
		ALYSLC::SkyrimsParagliderCompat::CheckForParaglider();
		ALYSLC::TrueDirectionalMovementCompat::CheckForTrueDirectionalMovement(g_loadInterface);
		ALYSLC::TrueHUDCompat::RequestTrueHUDAPIs(g_loadInterface);
		break;
	}
	case SKSE::MessagingInterface::kPostLoadGame:
	{
		logger::info("[Main] Post load game.");
		// Attempt to load the debug overlay.
		ALYSLC::DebugOverlayMenu::Load();
		break;
	}
	case SKSE::MessagingInterface::kPostPostLoad:
	{
		logger::info("[Main] Post-post load.");
		break;
	}
	case SKSE::MessagingInterface::kPreLoadGame:
	{
		logger::info("[Main] Pre load game.");
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

	*path /= fmt::format("{}.log"sv, Plugin::NAME);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
	const auto level = spdlog::level::trace;
#else
	const auto level = spdlog::level::info;
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(level);
	log->flush_on(level);

	spdlog::set_default_logger(std::move(log));
	//spdlog::set_pattern("[%l] %v"s);
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {};
#endif

#ifdef SKYRIMAE
	InitializeLog();
#endif

	logger::info("[MAIN] Adventurers Like You: Skyrim Local Co-op Mod loaded!");
	// Create global data singleton before doing anything else.
	ALYSLC::GlobalCoopData::GetSingleton();

	g_loadInterface = a_skse;
	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 8);

	if (auto messaging = SKSE::GetMessagingInterface(); !messaging->RegisterListener("SKSE", SKSEMessageHandler))
	{
		logger::critical("[MAIN] ERR: Could not register messaging interface listener.");
		return false;
	}

	if (auto papyrus = SKSE::GetPapyrusInterface(); !papyrus || !papyrus->Register(ALYSLC::CoopLib::RegisterFuncs))
	{
		logger::critical("[MAIN] ERR: Could not get Papyrus interface or register Papyrus functions.");
		return false;
	}

	if (auto serialization = SKSE::GetSerializationInterface(); !serialization) 
	{
		logger::critical("[MAIN] ERR: Could not get serialization interface.");
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

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	InitializeLog();

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION.pack();

	if (a_skse->IsEditor()) {
		logger::critical("[MAIN] Loaded in editor, marking as incompatible."sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver != RUNTIME) {
		logger::critical(FMT_STRING("[MAIN] Unsupported runtime version {}."sv), ver.string());
		return false;
	}

	return true;
}

#ifdef SKYRIMAE
extern "C" DLLEXPORT constinit SKSE::PluginVersionData SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
	v.UsesAddressLibrary(true);

	return v;
}();
#endif

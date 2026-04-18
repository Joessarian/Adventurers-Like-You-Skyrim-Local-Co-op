#include <CameraManager.h>
#include <Compatibility.h>
#include <DebugAPI.h>
#include <Events.h>
#include <Hooks.h> 
#include <MenuInputManager.h>
#include <ModAPI.h>
#include <Proxy.h>
#include <Serialization.h>
#include <Util.h>
#include "../extern/CommonLibSSE/src/SKSE/API.cpp"

const SKSE::LoadInterface* g_loadInterface = nullptr;

void SKSEMessageHandler(SKSE::MessagingInterface::Message* msg)
{
	switch (msg->type) 
	{
	case SKSE::MessagingInterface::kDataLoaded:
	{
		SPDLOG_INFO("Data loaded.");
		// Install all hooks.
		ALYSLC::Hooks::Install();
		// Add event sinks for all necessary events.
		ALYSLC::Events::RegisterEvents();
		// Register debug overlay menu.
		ALYSLC::DebugOverlayMenu::Register();
		// Run compatibility checks and initialization.
		ALYSLC::AlternateConversationCameraCompat::CheckForAlternateConversationCamera
		(
			g_loadInterface
		);
		ALYSLC::EldenSprintCompat::CheckForEldenSprint(g_loadInterface);
		ALYSLC::MCOCompat::CheckForMCO(g_loadInterface);
		ALYSLC::NFFCompat::CheckForNFF(g_loadInterface);
		ALYSLC::PersistentFavoritesCompat::CheckForPersistentFavorites();
		ALYSLC::PrecisionCompat::RequestPrecisionAPIs(g_loadInterface);
		ALYSLC::QuickLootCompat::CheckForQuickLoot(g_loadInterface);
		ALYSLC::RaceMenuCompat::CheckForRaceMenu(g_loadInterface);
		ALYSLC::RequiemCompat::CheckForRequiem(g_loadInterface);
		ALYSLC::SkyrimsParagliderCompat::CheckForParaglider();
		ALYSLC::TKDodgeCompat::CheckForTKDodge();
		ALYSLC::TrueDirectionalMovementCompat::CheckForTrueDirectionalMovement(g_loadInterface);
		ALYSLC::TrueHUDCompat::RequestTrueHUDAPIs(g_loadInterface);
		// Import all settings after setting the plugin name to use
		// in ALYSLC::EnderalCompat::CheckForEnderalSSE().
		ALYSLC::EnderalCompat::CheckForEnderalSSE();
		ALYSLC::Settings::ImportAllSettings();
		break;
	}
	case SKSE::MessagingInterface::kNewGame:
	{
		SPDLOG_INFO("New game.");
		// Set default serialization data through the Load() function.
		SKSE::SerializationInterface* intfc = 
		(
			SKSE::detail::APIStorage::get().serializationInterface
		); 
		if (intfc)
		{
			SPDLOG_INFO("New game. Setting default serialization data on load.");
			ALYSLC::Serialization::Load(intfc);
		}

		// Attempt to load the debug overlay.
		ALYSLC::DebugOverlayMenu::Load();
		break;
	}
	case SKSE::MessagingInterface::kPostLoad:
	{
		SPDLOG_INFO("Post load.");
		break;
	}
	case SKSE::MessagingInterface::kPostLoadGame:
	{
		SPDLOG_INFO("Post load game.");
		// Attempt to load the debug overlay.
		ALYSLC::DebugOverlayMenu::Load();
		// Despawn any lingering summons.
		ALYSLC::Util::DespawnLingeringSummons();

		// No longer loading a save.
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		glob.loadingASave = false;
		break;
	}
	case SKSE::MessagingInterface::kPostPostLoad:
	{
		SPDLOG_INFO("Post-post load.");
		break;
	}
	case SKSE::MessagingInterface::kPreLoadGame:
	{
		SPDLOG_INFO("Pre load game.");
		// Register for P1 positioning events.
		ALYSLC::CoopPositionPlayerEventHandler::Register();
		// Stop any active co-op session and indicate that the game is loading.
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		glob.loadingASave = true;
		break;
	}
	case SKSE::MessagingInterface::kSaveGame:
	{
		SPDLOG_INFO("Save game.");
		auto& glob = ALYSLC::GlobalCoopData::GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		// Ensure P1 is not essential before the game saves, since this state will carry over
		// when the save is loaded and become problematic 
		// if P1 is not in co-op and falls below 0 health.
		// Guarantees a clean slate upon loading a save and allows death alternative mods
		// to re-apply their changes to P1's essential status.
		if (glob.coopSessionActive && !glob.p1IsEssential && p1 && p1->IsEssential())
		{
			SPDLOG_INFO("Clear essential flag for P1 before the game saves.");
			ALYSLC::Util::ChangeEssentialStatus(p1, false);
		}

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
		util::report_and_fail("Failed to find standard logging directory"sv);
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
	// Changed to not include the directory.
	spdlog::set_pattern("| %^%l%$ | %c | %s (%#) | [%!] | >> %v"s);

	SPDLOG_INFO("Initialized logger for {} v{}", Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {};
#endif

	// Create global data singleton before doing anything else.
	ALYSLC::GlobalCoopData::GetSingleton();
	g_loadInterface = a_skse;
	SKSE::Init(a_skse);
	InitializeLog();
	SKSE::AllocTrampoline((1 << 7) + (1 << 5));

	auto messaging = SKSE::GetMessagingInterface(); 
	if (!messaging->RegisterListener("SKSE", SKSEMessageHandler))
	{
		SPDLOG_ERROR("Could not register messaging interface listener.");
		return false;
	}

	auto papyrus = SKSE::GetPapyrusInterface(); 
	if (!papyrus || !papyrus->Register(ALYSLC::CoopLib::RegisterFuncs))
	{
		SPDLOG_ERROR("Could not get Papyrus interface or register Papyrus functions.");
		return false;
	}

	if (auto serialization = SKSE::GetSerializationInterface(); !serialization) 
	{
		SPDLOG_ERROR("Could not get serialization interface.");
		return false;
	}
	else
	{
		SPDLOG_INFO("Setting serialization callbacks.");
		// Set serialization ID and callbacks.
		serialization->SetUniqueID(Hash("ALYSLC"));
		serialization->SetLoadCallback(ALYSLC::Serialization::Load);
		serialization->SetRevertCallback(ALYSLC::Serialization::Revert);
		serialization->SetSaveCallback(ALYSLC::Serialization::Save);
	}
	
	SPDLOG_INFO("Adventurers Like You: Skyrim Local Co-op Mod loaded!");
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
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query
(
	const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info
)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "ALYSLC";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor())
	{
		SPDLOG_ERROR("Loaded in editor, marking as incompatible."sv);
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
		SPDLOG_ERROR(FMT_STRING("Unsupported runtime version {}."sv), ver.string());
		return false;
	}

	return true;
}
#endif

extern "C" DLLEXPORT void* SKSEAPI RequestPluginAPI
(
	const ALYSLC_API::InterfaceVersion a_interfaceVersion
)
{
	auto api = ALYSLC_API::ALYSLCInterface::GetSingleton();
	SPDLOG_INFO
	(
		"RequestPluginAPI called, InterfaceVersion {}.", 
		static_cast<uint8_t>(a_interfaceVersion)
	);

	switch (a_interfaceVersion) 
	{
	case ALYSLC_API::InterfaceVersion::V1:
	case ALYSLC_API::InterfaceVersion::V2:
		SPDLOG_INFO("RequestPluginAPI returned the API singleton.");
		return static_cast<void*>(api);
	}

	SPDLOG_INFO("RequestPluginAPI requested the wrong interface version.");
	return nullptr;
}

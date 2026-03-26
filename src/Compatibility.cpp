#include "Compatibility.h"
#include "GlobalCoopData.h"

namespace ALYSLC
{
	PRECISION_API::IVPrecision4* PrecisionCompat::g_precisionAPI4{ nullptr };
	TRUEHUD_API::IVTrueHUD3* TrueHUDCompat::g_trueHUDAPI3{ nullptr };
	bool AlternateConversationCameraCompat::g_installed{ false };
	bool EldenSprintCompat::g_installed{ false };
	bool EnderalCompat::g_installed{ false };
	bool MCOCompat::g_installed{ false };
	bool PersistentFavoritesCompat::g_installed{ false };
	bool PrecisionCompat::g_installed{ false };
	bool QuickLootCompat::g_installed{ false };
	bool QuickLootCompat::g_isQuickLootIE{ false };
	bool RaceMenuCompat::g_installed{ false };
	bool RequiemCompat::g_installed{ false };
	bool SkyrimSoulsCompat::g_installed{ false };
	bool SkyrimsParagliderCompat::g_installed{ false };
	bool SkyrimsParagliderCompat::g_p1HasParaglider{ false };
	bool TKDodgeCompat::g_installed{ false };
	bool TrueDirectionalMovementCompat::g_installed{ false };
	bool TrueHUDCompat::g_installed{ false };
	
	void AlternateConversationCameraCompat::CheckForAlternateConversationCamera
	(
		const SKSE::LoadInterface* a_loadInterface
	)
	{
		g_installed = 
		(
			static_cast<bool>(GetModuleHandleA("AlternateConversationCamera.dll"))
		);
		if (g_installed)
		{
			SPDLOG_INFO("AlternateConversationCamera installed!");
		}
	}

	void EldenSprintCompat::CheckForEldenSprint(const SKSE::LoadInterface* a_loadInterface)
	{
		g_installed = 
		(
			a_loadInterface->GetPluginInfo("EldenSprint") ||
			a_loadInterface->GetPluginInfo("LoreRim - Inifinte Stamina Out of Combat")
		);
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_installed && dataHandler) 
		{
			g_installed = static_cast<bool>
			(
				dataHandler->LookupModByName("EldenSprint.esl") ||
				dataHandler->LookupModByName("LoreRim - Inifinte Stamina Out of Combat.esp")
			);
		}

		if (g_installed)
		{
			SPDLOG_INFO("Elden Sprint installed!");
		}
	}

	void EnderalCompat::CheckForEnderalSSE()
	{
		g_installed = static_cast<bool>(GetModuleHandleA("EnderalSE.dll"));
		if (g_installed)
		{
			ALYSLC::GlobalCoopData::PLUGIN_NAME = "ALYSLC Enderal.esp"sv;
			SPDLOG_INFO("Enderal SSE installed! Plugin name to use: '{}'.",
				ALYSLC::GlobalCoopData::PLUGIN_NAME);
		}
		else
		{
			ALYSLC::GlobalCoopData::PLUGIN_NAME = "ALYSLC.esp"sv;
			SPDLOG_INFO("Enderal SSE is not installed. Plugin name to use: '{}'.",
				ALYSLC::GlobalCoopData::PLUGIN_NAME);
		}
		
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (dataHandler && 
			dataHandler->LookupLoadedLightModByName("ALYSLC Enderal.esp"sv) &&
			dataHandler->LookupLoadedLightModByName("ALYSLC.esp"sv))
		{
			RE::DebugMessageBox
			(
				"[ALYSLC]\nERROR: "
				"More than one ALYSLC '.esp' file is currently loaded.\n"
				"To avoid issues, please make sure only the '.esp' that matches your game "
				"(Skyrim or Enderal) is enabled and then restart the game."
			);
		}
	}
	
	void MCOCompat::CheckForMCO(const SKSE::LoadInterface* a_loadInterface)
	{
		g_installed = 
		(
			a_loadInterface->GetPluginInfo("Attack_DXP") || 
			static_cast<bool>(GetModuleHandleA("MCO.dll"))
		);
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_installed && dataHandler) 
		{
			g_installed = dataHandler->LookupModByName("Attack_DXP.esp") != nullptr;
			if (!g_installed) 
			{
				g_installed = dataHandler->LookupModByName("Attack_MCO.esp") != nullptr;
			}
		}

		if (g_installed)
		{
			SPDLOG_INFO("MCO installed!");
		}
	}

	void PersistentFavoritesCompat::CheckForPersistentFavorites()
	{
		g_installed = 
		(
			static_cast<bool>(GetModuleHandleA("PersistentFavorites.dll"))
		);
		if (g_installed)
		{
			SPDLOG_INFO("PersistentFavorites installed!");
		}
	}

	void PrecisionCompat::RequestPrecisionAPIs(const SKSE::LoadInterface* a_loadInterface)
	{
		g_precisionAPI4 = nullptr;
		const auto pluginInfo = a_loadInterface->GetPluginInfo(PRECISION_API::PrecisionPluginName); 
		if (pluginInfo)
		{
			g_installed = true;
			SPDLOG_INFO("Prerequisite mod {} is installed!", 
				PRECISION_API::PrecisionPluginName);

			g_precisionAPI4 = reinterpret_cast<PRECISION_API::IVPrecision4*>
			(
				PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V4)
			);
			if (g_precisionAPI4)
			{
				SPDLOG_INFO("Received access to Precision API V4.");

				// Register havok callback after obtaining the API.
				g_precisionAPI4->AddPrePhysicsStepCallback
				(
					SKSE::GetPluginHandle(), 
					[](RE::bhkWorld* a_world) 
					{
						GlobalCoopData::PrecisionPrePhysicsStepCallback(a_world); 
					}
				);
				SPDLOG_INFO("Registered Precision pre-physics step callback.");

				g_precisionAPI4->AddPreHitCallback
				(
					SKSE::GetPluginHandle(),
					[](const PRECISION_API::PrecisionHitData& a_data)
					{
						return GlobalCoopData::PrecisionPreHitCallback(a_data);
					}
				);
				SPDLOG_INFO("Registered Precision pre-hit callback.");
			}
			else
			{
				SPDLOG_ERROR("Could not get access to Precision API V4.");
				return;
			}

			SPDLOG_INFO("Gained access to all required Precision APIs.");
		}
		else
		{
			g_installed = false;
			SPDLOG_ERROR
			(
				"Could not find prerequisite mod 'Precision'. Please ensure it is installed."
			);
		}
	}

	void QuickLootCompat::CheckForQuickLoot(const SKSE::LoadInterface* a_loadInterface)
	{
		g_isQuickLootIE = a_loadInterface->GetPluginInfo("QuickLootIE");
		g_installed = 
		{
			a_loadInterface->GetPluginInfo("QuickLootRE") ||
			a_loadInterface->GetPluginInfo("QuickLootEE") ||
			g_isQuickLootIE
		};

		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_installed && dataHandler)
		{
			g_isQuickLootIE = dataHandler->LookupModByName("QuickLootIE.esp") != nullptr;
			g_installed = 
			{
				dataHandler->LookupModByName("QuickLootRE.esp") != nullptr ||
				dataHandler->LookupModByName("QuickLootEE.esp") != nullptr ||
				g_isQuickLootIE
			};
		}

		if (g_installed) 
		{
			SPDLOG_INFO("{} installed!", g_isQuickLootIE ? "QuickLootIE" : "QuickLootRE/EE");
		}
	}

	void RaceMenuCompat::CheckForRaceMenu(const SKSE::LoadInterface * a_loadInterface)
	{
		g_installed = 
		{
			a_loadInterface->GetPluginInfo("RaceMenu") || 
			static_cast<bool>(GetModuleHandleA("skee64.dll"))
		};

		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_installed && dataHandler)
		{
			g_installed = 
			(
				dataHandler->LookupModByName("RaceMenu.esp") != nullptr
			);
		}

		if (g_installed)
		{
			SPDLOG_INFO("RaceMenu installed!");
		}
	}

	void RequiemCompat::CheckForRequiem(const SKSE::LoadInterface* a_loadInterface)
	{
		g_installed = a_loadInterface->GetPluginInfo("Requiem");
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_installed && dataHandler)
		{
			g_installed = dataHandler->LookupModByName("Requiem.esp") != nullptr;
		}

		if (g_installed)
		{
			SPDLOG_INFO("Requiem - The Roleplaying Overhaul installed!");
		}
	}

	void SkyrimSoulsCompat::CheckForSkyrimSouls()
	{
		g_installed = 
		{
			static_cast<bool>(GetModuleHandleA("SkyrimSoulsRE.dll"))
		};
		if (g_installed)
		{
			SPDLOG_INFO("SkyrimSoulsRE installed!");
		}
	}

	void SkyrimsParagliderCompat::CheckForParaglider()
	{
		// Paraglider ownership check is done before P1 manager construction; 
		// init to false for now.
		g_p1HasParaglider = false;
		g_installed = static_cast<bool>(GetModuleHandleA("Paraglider.dll"));
		if (g_installed)
		{
			SPDLOG_INFO("Skyrim's Paraglider installed!");
		}
	}

	void TKDodgeCompat::CheckForTKDodge()
	{
		g_installed = 
		{
			static_cast<bool>(GetModuleHandleA("TKPlugin.dll")) ||
			static_cast<bool>(GetModuleHandleA("TK_Dodge_RE.dll"))
		};
		if (g_installed)
		{
			SPDLOG_INFO("TKDodge installed!");
		}
	}

	void TrueDirectionalMovementCompat::CheckForTrueDirectionalMovement
	(
		const SKSE::LoadInterface* a_loadInterface
	)
	{
		g_installed = 
		{
			a_loadInterface->GetPluginInfo("TrueDirectionalMovement") || 
			static_cast<bool>(GetModuleHandleA("TrueDirectionalMovement.dll"))
		};

		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_installed && dataHandler)
		{
			g_installed = 
			(
				dataHandler->LookupModByName("TrueDirectionalMovement.esp") != nullptr
			);
		}

		if (g_installed)
		{
			SPDLOG_INFO("True Directional Movement installed!");
		}
	}

	void TrueHUDCompat::RequestTrueHUDAPIs(const SKSE::LoadInterface* a_loadInterface) 
	{
		g_trueHUDAPI3 = nullptr;
		const auto pluginInfo = a_loadInterface->GetPluginInfo(TRUEHUD_API::TrueHUDPluginName); 
		if (pluginInfo) 
		{
			g_installed = true;
			SPDLOG_INFO("{} is installed!", TRUEHUD_API::TrueHUDPluginName);

			g_trueHUDAPI3 = reinterpret_cast<TRUEHUD_API::IVTrueHUD3*>
			(
				TRUEHUD_API::RequestPluginAPI(TRUEHUD_API::InterfaceVersion::V3)
			);
			if (g_trueHUDAPI3)
			{
				SPDLOG_INFO("Received access to TrueHUD API V3.");
			}
			else
			{
				SPDLOG_ERROR("Could not get access to TrueHUD API V3.");
				return;
			}

			SPDLOG_INFO("Gained access to all required TrueHUD APIs.");
		}
	}
};

#include "Compatibility.h"
#include "GlobalCoopData.h"

namespace ALYSLC
{
	PRECISION_API::IVPrecision1* PrecisionCompat::g_precisionAPI1;
	PRECISION_API::IVPrecision3* PrecisionCompat::g_precisionAPI3;
	PRECISION_API::IVPrecision4* PrecisionCompat::g_precisionAPI4;
	TRUEHUD_API::IVTrueHUD3* TrueHUDCompat::g_trueHUDAPI3;
	bool EnderalCompat::g_enderalSSEInstalled;
	bool MCOCompat::g_mcoInstalled;
	bool MiniMapCompat::g_miniMapInstalled;
	bool MiniMapCompat::g_shouldApplyCullingWorkaround;
	bool PrecisionCompat::g_precisionInstalled;
	bool QuickLootCompat::g_quickLootInstalled;
	bool RequiemCompat::g_requiemInstalled;
	bool SkyrimsParagliderCompat::g_paragliderInstalled;
	bool SkyrimsParagliderCompat::g_p1HasParaglider;
	bool TKDodgeCompat::g_tkDodgeInstalled;
	bool TrueDirectionalMovementCompat::g_trueDirectionalMovementInstalled;
	bool TrueHUDCompat::g_trueHUDInstalled;

	void EnderalCompat::CheckForEnderalSSE()
	{
		g_enderalSSEInstalled = static_cast<bool>(GetModuleHandleA("EnderalSE.dll"));
		if (g_enderalSSEInstalled)
		{
			ALYSLC::GlobalCoopData::PLUGIN_NAME = "ALYSLC Enderal.esp"sv;
			logger::info("[Compatibility] Enderal SSE installed! Plugin name to use: '{}'.", ALYSLC::GlobalCoopData::PLUGIN_NAME);
		}
		else
		{
			ALYSLC::GlobalCoopData::PLUGIN_NAME = "ALYSLC.esp"sv;
			logger::info("[Compatibility] Enderal SSE is not installed. Plugin name to use: '{}'.", ALYSLC::GlobalCoopData::PLUGIN_NAME);
		}
	}
	
	void MCOCompat::CheckForMCO(const SKSE::LoadInterface* a_loadInterface)
	{
		g_mcoInstalled = a_loadInterface->GetPluginInfo("Attack_DXP") || static_cast<bool>(GetModuleHandleA("MCO.dll"));
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_mcoInstalled && dataHandler) 
		{
			g_mcoInstalled = dataHandler->LookupModByName("Attack_DXP.esp") != nullptr;
			if (!g_mcoInstalled) 
			{
				g_mcoInstalled = dataHandler->LookupModByName("Attack_MCO.esp") != nullptr;
			}
		}

		if (g_mcoInstalled)
		{
			logger::info("[Compatibility] MCO installed!");
		}
	}

	void MiniMapCompat::CheckForMiniMap()
	{
		g_miniMapInstalled = static_cast<bool>(GetModuleHandleA("MiniMap.dll"));
		if (g_miniMapInstalled)
		{
			logger::info("[Compatibility] Felisky384's MiniMap installed! Will setup culling hooks to minimize freezes in interior cells.");
		}

		g_shouldApplyCullingWorkaround = false;
	}

	void PrecisionCompat::RequestPrecisionAPIs(const SKSE::LoadInterface* a_loadInterface)
	{
		g_precisionAPI1 = nullptr;
		g_precisionAPI3 = nullptr;
		g_precisionAPI4 = nullptr;
		if (const auto pluginInfo = a_loadInterface->GetPluginInfo(PRECISION_API::PrecisionPluginName); pluginInfo)
		{
			g_precisionInstalled = true;
			logger::info("[Compatibility] Prerequisite mod {} is installed.", PRECISION_API::PrecisionPluginName);
			g_precisionAPI3 = reinterpret_cast<PRECISION_API::IVPrecision3*>(PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V3));
			if (g_precisionAPI3)
			{
				logger::info("[Compatibility] Received access to Precision API V3.");
			}
			else
			{
				logger::error("[Compatibility] Could not get access to Precision API V3.");
				return;
			}

			g_precisionAPI1 = reinterpret_cast<PRECISION_API::IVPrecision1*>(PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V1));
			if (g_precisionAPI1)
			{
				logger::info("[Compatibility] Received access to Precision API V1.");
			}
			else
			{
				logger::error("[Compatibility] Could not get access to Precision API V1.");
				return;
			}

			g_precisionAPI4 = reinterpret_cast<PRECISION_API::IVPrecision4*>(PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V4));
			if (g_precisionAPI4)
			{
				logger::info("[Compatibility] Received access to Precision API V4.");
			}
			else
			{
				logger::error("[Compatibility] Could not get access to Precision API V4.");
				return;
			}

			logger::info("[Compatibility] Gained access to all required Precision APIs.");
		}
		else
		{
			g_precisionInstalled = false;
			logger::error("[Compatibility] Could not find prerequisite mod 'Precision'. Please ensure it is installed.");
		}
	}

	void QuickLootCompat::CheckForQuickLoot(const SKSE::LoadInterface* a_loadInterface)
	{
		g_quickLootInstalled = 
		{
			a_loadInterface->GetPluginInfo("QuickLootRE") ||
			a_loadInterface->GetPluginInfo("QuickLootEE")
		};

		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_quickLootInstalled && dataHandler)
		{
			g_quickLootInstalled = 
			{
				dataHandler->LookupModByName("QuickLootRE.esp") != nullptr ||
				dataHandler->LookupModByName("QuickLootEE.esp") != nullptr
			};
		}

		if (g_quickLootInstalled) 
		{
			logger::info("[Compatibility] QuickLootRE/EE installed!");
		}
	}

	void RequiemCompat::CheckForRequiem(const SKSE::LoadInterface* a_loadInterface)
	{
		g_requiemInstalled = a_loadInterface->GetPluginInfo("Requiem");
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_requiemInstalled && dataHandler)
		{
			g_requiemInstalled = dataHandler->LookupModByName("Requiem.esp") != nullptr;
		}

		if (g_requiemInstalled)
		{
			logger::info("[Compatibility] Requiem - The Roleplaying Overhaul installed!");
		}
	}

	void SkyrimsParagliderCompat::CheckForParaglider()
	{
		// Paraglider check is done before P1 manager construction; init to false for now.
		g_p1HasParaglider = false;
		g_paragliderInstalled = static_cast<bool>(GetModuleHandleA("Paraglider.dll"));
		if (g_paragliderInstalled)
		{
			logger::info("[Compatibility] Skyrim's Paraglider installed!");
		}
	}

	void TKDodgeCompat::CheckForTKDodge()
	{
		g_tkDodgeInstalled = 
		{
			static_cast<bool>(GetModuleHandleA("TKPlugin.dll")) ||
			static_cast<bool>(GetModuleHandleA("TK_Dodge_RE.dll"))
		};
		if (g_tkDodgeInstalled)
		{
			logger::info("[Compatibility] TKDodge installed!");
		}
	}

	void TrueDirectionalMovementCompat::CheckForTrueDirectionalMovement(const SKSE::LoadInterface* a_loadInterface)
	{
		g_trueDirectionalMovementInstalled = 
		{
			a_loadInterface->GetPluginInfo("TrueDirectionalMovement") || 
			static_cast<bool>(GetModuleHandleA("TrueDirectionalMovement.dll"))
		};

		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!g_trueDirectionalMovementInstalled && dataHandler)
		{
			g_trueDirectionalMovementInstalled = dataHandler->LookupModByName("TrueDirectionalMovement.esp") != nullptr;
		}

		if (g_trueDirectionalMovementInstalled)
		{
			logger::info("[Compatibility] True Directional Movement installed!");
		}
	}

	void TrueHUDCompat::RequestTrueHUDAPIs(const SKSE::LoadInterface* a_loadInterface) 
	{
		g_trueHUDAPI3 = nullptr;
		if (const auto pluginInfo = a_loadInterface->GetPluginInfo(TRUEHUD_API::TrueHUDPluginName); pluginInfo) 
		{
			g_trueHUDInstalled = true;
			logger::info("[Compatibility] Prerequisite mod {} is installed.", TRUEHUD_API::TrueHUDPluginName);
			g_trueHUDAPI3 = reinterpret_cast<TRUEHUD_API::IVTrueHUD3*>(TRUEHUD_API::RequestPluginAPI(TRUEHUD_API::InterfaceVersion::V3));
			if (g_trueHUDAPI3)
			{
				logger::info("[Compatibility] Received access to TrueHUD API V3.");
			}
			else
			{
				logger::error("[Compatibility] Could not get access to TrueHUD API V3.");
				return;
			}

			logger::info("[Compatibility] Gained access to all required TrueHUD APIs.");
		}
		else
		{
			g_trueHUDInstalled = false;
			logger::error("[Compatibility] Could not find prerequisite mod 'TrueHUD'. Please ensure it is installed.");
		}
	}
};

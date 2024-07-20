#pragma once
#include <PrecisionAPI.h>
#include <TrueHUDAPI.h>

namespace ALYSLC
{
	struct EnderalCompat
	{
		static void CheckForEnderalSSE();
		static bool g_enderalSSEInstalled;
	};

	struct MiniMapCompat
	{
		static void CheckForMiniMap();
		static bool g_miniMapInstalled;
		static bool g_shouldApplyCullingWorkaround;
	};

	struct PrecisionCompat
	{
		static void RequestPrecisionAPIs(const SKSE::LoadInterface* a_loadInterface);
		static bool g_precisionInstalled;
		static PRECISION_API::IVPrecision1* g_precisionAPI1;
		static PRECISION_API::IVPrecision3* g_precisionAPI3;
		static PRECISION_API::IVPrecision4* g_precisionAPI4;
	};

	struct QuickLootCompat
	{
		static void CheckForQuickLoot(const SKSE::LoadInterface* a_loadInterface);
		static bool g_quickLootInstalled;
	};

	struct SkyrimsParagliderCompat
	{
		static void CheckForParaglider();
		static bool g_paragliderInstalled;
		static bool g_p1HasParaglider;
	};

	struct TrueDirectionalMovementCompat
	{
		static void CheckForTrueDirectionalMovement(const SKSE::LoadInterface* a_loadInterface);
		static bool g_trueDirectionalMovementInstalled;
	};

	struct TrueHUDCompat
	{
		static void RequestTrueHUDAPIs(const SKSE::LoadInterface* a_loadInterface);
		static bool g_trueHUDInstalled;
		static TRUEHUD_API::IVTrueHUD3* g_trueHUDAPI3;
	};
};

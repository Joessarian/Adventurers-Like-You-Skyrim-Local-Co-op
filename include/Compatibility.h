#pragma once
#include <PrecisionAPI.h>
#include <TrueHUDAPI.h>

namespace ALYSLC
{
	// Saved flags and other data which indicate if certain supported mods are installed.

	struct EnderalCompat
	{
		static void CheckForEnderalSSE();
		static bool g_enderalSSEInstalled;

	};

	struct MCOCompat
	{
		static void CheckForMCO(const SKSE::LoadInterface* a_loadInterface);
		static bool g_mcoInstalled;
	};

	struct PersistentFavoritesCompat
	{
		static void CheckForPersistentFavorites();
		static bool g_persistentFavoritesInstalled;
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

	struct RequiemCompat
	{
		static void CheckForRequiem(const SKSE::LoadInterface* a_loadInterface);
		static bool g_requiemInstalled;
	};

	struct SkyrimsParagliderCompat
	{
		static void CheckForParaglider();
		static bool g_paragliderInstalled;
		static bool g_p1HasParaglider;
	};

	struct TKDodgeCompat
	{
		static void CheckForTKDodge();
		static bool g_tkDodgeInstalled;
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
		static TRUEHUD_API::IVTrueHUD1* g_trueHUDAPI1;
		static TRUEHUD_API::IVTrueHUD3* g_trueHUDAPI3;
	};
};

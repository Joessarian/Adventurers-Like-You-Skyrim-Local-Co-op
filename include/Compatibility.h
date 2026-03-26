#pragma once
#include <PrecisionAPI.h>
#include <TrueHUDAPI.h>

namespace ALYSLC
{
	// Saved flags and other data that indicate if certain supported mods are installed.

	struct AlternateConversationCameraCompat
	{
		static void CheckForAlternateConversationCamera(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
	};

	struct EldenSprintCompat
	{
		static void CheckForEldenSprint(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
	};

	struct EnderalCompat
	{
		static void CheckForEnderalSSE();
		static bool g_installed;
	};

	struct MCOCompat
	{
		static void CheckForMCO(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
	};

	struct PersistentFavoritesCompat
	{
		static void CheckForPersistentFavorites();
		static bool g_installed;
	};

	struct PrecisionCompat
	{
		static void RequestPrecisionAPIs(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
		static PRECISION_API::IVPrecision4* g_precisionAPI4;
	};

	struct QuickLootCompat
	{
		static void CheckForQuickLoot(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
		static bool g_isQuickLootIE;
	};

	struct RaceMenuCompat
	{
		static void CheckForRaceMenu(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
	};

	struct RequiemCompat
	{
		static void CheckForRequiem(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
	};

	struct SkyrimSoulsCompat
	{
		static void CheckForSkyrimSouls();
		static bool g_installed;
	};

	struct SkyrimsParagliderCompat
	{
		static void CheckForParaglider();
		static bool g_installed;
		static bool g_p1HasParaglider;
	};

	struct TKDodgeCompat
	{
		static void CheckForTKDodge();
		static bool g_installed;
	};

	struct TrueDirectionalMovementCompat
	{
		static void CheckForTrueDirectionalMovement(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
	};

	struct TrueHUDCompat
	{
		static void RequestTrueHUDAPIs(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
		static TRUEHUD_API::IVTrueHUD3* g_trueHUDAPI3;
	};
};

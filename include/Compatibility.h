#pragma once
#include <PrecisionAPI.h>
#include <TrueDirectionalMovementAPI.h>
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

	struct ExtendedUICompat
	{
		static void CheckForExtendedUI(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
	};

	struct MCOCompat
	{
		static void CheckForMCO(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
	};

	struct NFFCompat
	{
		static void CheckForNFF(const SKSE::LoadInterface* a_loadInterface);
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
		static double g_originalScaleX;
		static double g_originalScaleY;
		static double g_originalX;
		static double g_originalY;
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
	
	struct SandboxWhenIdleCompat
	{
		static void CheckForSandboxWhenIdle(const SKSE::LoadInterface* a_loadInterface);
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
		static void RequestTrueDirectionalMovementAPIs(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
		static TDM_API::IVTDM2* g_tdmAPI2;
	};

	struct TrueHUDCompat
	{
		static void RequestTrueHUDAPIs(const SKSE::LoadInterface* a_loadInterface);
		static bool g_installed;
		static TRUEHUD_API::IVTrueHUD3* g_trueHUDAPI3;
	};

	struct UseOrTakeCompat
	{
		static void CheckForUseOrTake();
		static bool g_installed;
	};
};

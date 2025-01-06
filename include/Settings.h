#pragma once
#include <Enums.h>
#include <Util.h>

namespace ALYSLC
{
	struct Settings
	{
		//========================
		//========================
		// [Customizable options]:
		//========================
		//========================

		//---------
		//[Camera]:
		//---------
		// Adjust the camera pitch depending on the current walkable terrain's pitch,
		// and adjust the camera yaw to better follow 
		// the collective heading direction of all players.
		static inline bool bAutoRotateCamPitch = true;
		static inline bool bAutoRotateCamYaw = true;
		// Experimental. Do not use if you suffer from motion sickness, as the camera
		// will jump through obstacles regularly to keep as many players in frame as possible.
		// The camera may also catch on a surface and stutter or cause the screen to flicker.
		// Use if you can't stand seeing the skybox/void outside the traversable worldspace.
		static inline bool bCamCollisions = true;
		// Also fade larger statics, like houses and walls.
		static inline bool bFadeLargerObstructions = false;
		// Fade objects that are blocking the visibility of players.
		static inline bool bFadeObstructions = true;
		// When an off-screen player is also not visible to other players
		// and is trying to adjust the camera, focus the camera on them
		// until they get near the rest of the party.
		// Useful when the camera is stuck in a bad spot
		// or for assisting straggling players in finding other players
		// without resorting to teleportation.
		static inline bool bFocalPlayerMode = false;
		// Use interpolation to smooth the camera's origin point path 
		// (the path the point equidistant to all players takes).
		static inline bool bOriginPointSmoothing = true;
		// Only fade objects that are a certain distance from the camera.
		static inline bool bProximityFadeOnly = false;
		// IMPORTANT: Disabling all occlusion markers in an exterior/interior cell, 
		// whether through the CK or through a plugin at runtime,
		// will lead to worse performance, as more objects must be rendered each frame.
		// Only enable if you have the FPS headroom, want to have max visibility, 
		// and don't mind the jank of seeing the "void" 
		// outside the walkable exterior/interior terrain.
		static inline bool bRemoveExteriorOcclusion = false;
		static inline bool bRemoveInteriorOcclusion = false;
		// Use interpolation to smooth the camera's target point path.
		static inline bool bTargetPosSmoothing = true;
		// Camera FOV to set for exterior and interior cells.
		static inline float fCamExteriorFOV = 75.0f;
		static inline float fCamInteriorFOV = 75.0f;
		
		// Lock on mode assistance type.
		// 0: Full (WIP): 
		// Auto zoom, rotate, and position the camera 
		// to keep all players and the lock on target on screen.
		// Zoom, position, and rotation controls are disabled.
		// 
		// 1: Rotation: 
		// Auto rotate the camera to track the lock on target 
		// and zoom out to keep the party in view as much as possible. 
		// Can still zoom in/out and change the camera's positional offset.
		// 
		// 2: Zoom: 
		// Auto zoom out the camera to keep the players and lock on target on screen. 
		// Can still rotate the camera normally.
		static inline uint32_t uLockOnAssistance = !ALYSLC::CamLockOnAssistanceLevel::kRotation;

		//-------------------
		//[Cheats and Debug]:
		//-------------------
		// NOTE: Not for Enderal.
		// Add all perks that have corresponding animation events, 
		// such as shield charge and silent roll.
		static inline bool bAddAnimEventSkillPerks = false;
		// Attract nearby clutter while holding grab 
		// and not targeting any object with the crosshair.
		static inline bool bAutoGrabClutterOnHold = true;
		// Can players grab actors (other players not included)?
		static inline bool bCanGrabActors = false;
		// Can players grab other players?
		static inline bool bCanGrabOtherPlayers = true;
		// Infinite carryweight for all players while in co-op.
		static inline bool bInfiniteCarryweight = false;
		// Can grab actors indefinitely.
		static inline bool bRemoveGrabbedActorAutoGetUp = true;
		// Only one collision raycast per ragdolling actor per frame if true.
		// Otherwise, one collision raycast 
		// for each ragdolling actor's main skeleton nodes each frame.
		static inline bool bSimpleActorCollisionRaycast = false;
		// WIP: Speed up supported dodge animations. 
		// Makes dodging more responsive but lowers dodge distance.
		static inline bool bSpeedUpDodgeAnimations = true;
		// Speed up equip/unequip animations. 
		// Makes sheathing/drawing and cycling through gear faster.
		static inline bool bSpeedUpEquipAnimations = true;
		// Max number of grabbable objects at once.
		// Additional grabbed objects are suspended in a ring around the first grabbed object,
		// hence 1 is added to a number that divides 360 without a remainder.
		static inline uint32_t uMaxGrabbedReferences = 41;

		//--------------
		//[Emote Idles]:
		//--------------
		// All default assignable and cyclable emote idle events.
		static inline std::vector<RE::BSFixedString> sEmoteIdlesList = {
			"IdleApplaud2",
			"IdleApplaud3",
			"IdleApplaud4",
			"IdleApplaudSarcastic",
			"IdleSilentBow",
			"IdleCivilWarCheer",
			"IdleUncontrollableCough",
			"IdleCowering",
			"IdleCiceroDance1",
			"IdleCiceroDance2",
			"IdleCiceroDance3",
			"idleDrinkingStandingStart",
			"idleEatingStandingStart",
			"IdleFluteStart",
			"IdleGetAttention",
			"IdleCiceroHappy",
			"IdleHandsBehindBack",
			"pa_HugA",
			"IdleBlowHornImperial",
			"IdleLaugh",
			"IdleLuteStart",
			"IdlePointClose",
			"IdlePointFar_01",
			"IdlePointFar_02",
			"IdlePray",
			"IdleSalute",
			"IdleBlowHornStormcloak",
			"IdleWarmArms",
			"IdleWarmHands",
			"IdleWarmHandsCrouched",
			"IdleWave",
			"IdleWipeBrow"
		};

		//--------
		//[Menus]:
		//--------
		// Lockpicking is a team effort when enabled.
		// The lock-activating companion player rotates the pick, and P1 rotates the lock.
		static inline bool bTwoPlayerLockpicking = false;
		// Closest player to the speaker gets control of the dialogue menu 
		// if no player triggered the conversation.
		static inline bool bUninitializedDialogueWithClosestPlayer = true;
		// Player distance from dialogue target (in-game units) 
		// at which to automatically exit dialogue.
		static inline float fAutoEndDialogueRadius = 500.0f;

		//-----------
		//[Movement]:
		//-----------
		// Movement/rotation multipliers.
		// Multiplier applied to all rotation.
		static inline float fBaseRotationMult = 1.0f;
		// Base speedmult value, which is then modified by the player's height (inv. proportional).
		// Game's default speedmult is 100.
		// Players are much faster with the modified movement types'
		// increased rotation speeds, so the base speed is set lower to compensate.
		static inline float fBaseSpeed = 85.0f;
		// Bashing rotation multiplier.
		static inline float fBashingRotMult = 0.5f;
		// Blocking rotation multiplier.
		static inline float fBlockingRotMult = 1.0f;
		// Speedmult multiplier while casting spells.
		static inline float fCastingMovMult = 1.0f;
		// Casting rotation multiplier.
		static inline float fCastingRotMult = 0.75f;
		// Jump base initial speed in LS direction.
		// In game units / second.
		static inline float fJumpBaseLSDirSpeed = 500.0f;
		// Jump rotation multiplier.
		static inline float fJumpingRotMult = 0.3f;
		// Gravity multiplier that affects how fast the player rises 
		// to the apex and falls during the descent of a jump.
		// Lower the value for a floatier jump. Raise the value for a faster ascent and descent.
		static inline float fJumpingGravityMult = 2.0f;
		// Maximum support surface slope angle from which a player can start jumping.
		// Set to PI / 2.0 radians to jump off any support surface without restriction.
		// See that mountain? You can climb it. Or maybe not.
		static inline float fJumpingMaxSlopeAngle = PI / 2.0f;
		// Speedmult actor value multiplier while attacking with a melee weapon.
		static inline float fMeleeAttackMovMult = 1.0f;
		// Rotation multiplier while attacking with a melee weapon.
		static inline float fMeleeAttackRotMult = 0.5f;
		// Speedmult multiplier while attacking with a ranged weapon.
		static inline float fRangedAttackMovMult = 1.0f;
		// Rotation multiplier while attacking with a ranged weapon.
		static inline float fRangedAttackRotMult = 0.5f;
		// Mounted rotation multiplier.
		static inline float fRidingRotMult = 0.3f;
		// Seconds before reverting gravity and playing fall
		// animation for a jumping player. Directly influences jump height.
		// Increase to jump higher.
		static inline float fSecsAfterGatherToFall = 0.07f;
		// Sneak rotation multiplier.
		static inline float fSneakRotMult = 1.0f;
		// Speedmult actor value multiplier when sprinting.
		// Setting to 1 means jogging and sprinting are the same speed.
		static inline float fSprintingMovMult = 1.5f;
		// Sprint rotation multiplier.
		static inline float fSprintingRotMult = 0.67f;

		//-----------------
		//[Player Actions]:
		//-----------------
		// Aim pitch angle affects flop trajectory. Check it out.
		static inline bool bAimPitchAffectsFlopTrajectory = true;
		// Enable flopping (double tap special action bind when weapons are sheathed).
		static inline bool bAllowFlopping = true;
		// Can perform killmove executions on other players.
		static inline bool bCanKillmoveOtherPlayers = true;
		// Enable reviving of P1 if using the revive system.
		// Will NOT be compatible with any mods that change the death system for P1,
		// so keep disabled if you have any of those mods.
		static inline bool bCanRevivePlayer1 = true;
		// Hold a cycling bind to cycle through equipable items.
		// Emote idles are cycled on hold by default.
		static inline bool bHoldToCycle = false;
		// Rotate arms when holding the corresponding bind(s) 
		// and moving the right stick while weapons are sheathed.
		static inline bool bRotateArmsWhenSheathed = true;
		// Use velocity-based dash dodge instead of animations from any installed dodge mods.
		static inline bool bUseDashDodgeSystem = true;
		// Perform killmoves using this mod's system.
		static inline bool bUseKillmovesSystem = true;
		// Players are downed when reaching 0 health and remain in a pseudo-death state until
		// revived by another player through health transfer 
		// or the pre-determined revive window elapses (entire party is wiped).
		static inline bool bUseReviveSystem = true;
		// Use unarmed killmoves when casting spells at nearby low-health enemies.
		// Add a mean streak to your mage in close quarters combat.
		static inline bool bUseUnarmedKillmovesForSpellcasting = false;
		// Killmove chance per attack when target's health is below the killmove health threshold 
		// (fKillmoveHealthFraction).
		static inline float fKillmoveChance = 0.333333f;
		// Max health fraction below which killmoves can trigger.
		static inline float fKillmoveHealthFraction = 0.1f;
		// Max dash dodge movement type speed mult to apply.
		static inline float fMaxDashDodgeSpeedmult = 600.0f;
		// Min dash dodge movement type speed mult to apply.
		static inline float fMinDashDodgeSpeedmult = 300.0f;
		// Cycle through nearby references after holding the activate bind for this many seconds.
		static inline float fSecsBeforeActivationCycling = 1.0f;
		// Seconds between activation checks 
		// (time between highlighting objects once cycling starts).
		static inline float fSecsBetweenActivationChecks = 0.7f;
		// Seconds between cycling favorited items/emote idles while holding a cycling bind.
		static inline float fSecsCyclingInterval = 0.8f;
		// Default input hold time threshold for triggering on-hold player actions, 
		// such as power attacks.
		static inline float fSecsDefMinHoldTime = 0.15f;
		// Seconds to fully revive a player when downed, if uninterrupted.
		static inline float fSecsReviveTime = 5.0f;
		// Seconds until an unrevived downed player, and all other active players, die.
		static inline float fSecsUntilDownedDeath = 30.0f;
		// Automatically equip weapon-matching highest count/damage ammo when the player
		// equips a ranged weapon and has no ammo or mismatching ammo equipped.
		// [0] for no auto-equip
		// [1] for highest count.
		// [2] for highest damage.
		static inline uint32_t uAmmoAutoEquipMode = !ALYSLC::AmmoAutoEquipMode::kHighestCount;
		// The number of frames the dash dodge lasts at 60 FPS.
		// NOTE: This is also the number of I-frames for the dodge, 
		// since the player is invulnerable throughout the duration of the dodge. 
		// Decreasing/increasing the frame count also decreases/increases 
		// the distance of the dodge because the player spends less/more time 
		// at their max speedmult if the dodge lasts shorter/longer.
		// The dodge also always has an additional 6 startup frames.
		static inline uint32_t uDashDodgeBaseAnimFrameCount = 24;

		//--------------
		//[Progression]:
		//--------------
		// Add auto-scaling to co-op player's own skill progression 
		// when setting skill AVs on level up.
		// Gives co-op players class-dependent extra levels to skills on character level up.
		// NOTE: Base skill AVs are solely dependent on race at level 1, 
		// so no scaling occurs when changing class until leveling up after level 1.
		static inline bool bStackCoopPlayerSkillAVAutoScaling = false;
		// Multiplier for the XP required to level up.
		static inline float fLevelUpXPThresholdMult = 1.0f;

		// [Enderal-specific settings]
		// Please note that the options below will in no way balance progression completely
		// for multiple players and can easily be exploited should players desire to do so.
		// Nonetheless, the options are offered to present players 
		// with tools to individualize their progression 
		// in the absence of separate leveling per player.
		// For this reason, it is highly recommended to spread leveling points 
		// evenly among active players and elect for a varied party composition.
		//
		// When a skillbook is looted, add a random skillbook
		// of the same tier to other active players.
		static inline bool bEveryoneGetsALootedEnderalSkillbook = true;
		// Scale crafting/learning/memory points with the number of active players.
		// Meaning each time P1 gains one of these points, 
		// P1 instead gains points equal to the number of active players.
		// Left to the players' discretions to (not) abuse this as they see fit.
		// Reading crafting/learning books will only level up the player reading the books.
		static inline bool bScaleCraftingPointsWithNumPlayers = true;
		static inline bool bScaleLearningPointsWithNumPlayers = true;
		// Please note that perks are still shared among all players.
		static inline bool bScaleMemoryPointsWithNumPlayers = true;
		// Additional gold multiplier per active player.
		// Set to 1 to scale directly with the number of active players, 
		// or 0 for no additional gold at all.
		// Exists because Enderal skill progression is linked to buying crafting/learning books.
		static inline float fAdditionalGoldPerPlayerMult = 0.0f;

		//-----
		//[UI]:
		//-----
		// Camera lock on indicator side length (pixels).
		static inline float fCamLockOnIndicatorLength = 15.0f;
		// Camera lock on indicator side thickness (pixels).
		static inline float fCamLockOnIndicatorThickness = 3.0f;
		// Maximum amount of pixels the crosshair can move across per second.
		// Used as the base when scaling player crosshair sensitivities.
		static inline float fCrosshairMaxTraversablePixelsPerSec = 1920.0f;
		// Player menu control overlay (colored border) outline thickness (pixels).
		static inline float fPlayerMenuControlOverlayOutlineThickness = 6.0f;

		//============================
		//============================
		// [Player-specific settings]:
		//============================
		//============================
		// 
		// NOTE: All should be indexed via player ID, not controller ID.
		//
		//-----------------
		//[Stat Modifiers]:
		//-----------------
		// Multipliers applied to a co-op actor's health, magicka, and stamina cost/regen values.
		static inline std::vector<float> vfHealthRegenMult = 
		{
			1.0f, 1.0f, 1.0f, 1.0f 
		};
		static inline std::vector<float> vfMagickaCostMult = 
		{
			1.0f, 1.0f, 1.0f, 1.0f 
		};
		static inline std::vector<float> vfMagickaRegenMult = 
		{
			1.0f, 1.0f, 1.0f, 1.0f 
		};
		static inline std::vector<float> vfStaminaCostMult = 
		{
			1.0f, 1.0f, 1.0f, 1.0f 
		};
		static inline std::vector<float> vfStaminaRegenMult = 
		{ 
			1.0f, 1.0f, 1.0f, 1.0f 
		};
		// Ratio of health regen rate in combat to health regen rate out of combat.
		static inline std::vector<float> vfHealthRegenCombatRatio = 
		{
			0.70f, 0.70f, 0.70f, 0.70f 
		};
		// Ratio of magicka regen rate in combat to magicka regen rate out of combat.
		static inline std::vector<float> vfMagickaRegenCombatRatio = 
		{
			0.33f, 0.33f, 0.33f, 0.33f
		};
		// Ratio of stamina regen rate in combat to stamina regen rate out of combat.
		static inline std::vector<float> vfStaminaRegenCombatRatio = 
		{
			0.35f, 0.35f, 0.35f, 0.35f
		};

		//-------------------
		//[Damage Modifiers]:
		//-------------------
		// Multiplier applied to each player's dealt damage.
		// Applies to most sources of damage, 
		// especially direct sources such as melee attacks or non-explosive hits.
		static inline std::vector<float> vfDamageDealtMult = 
		{
			1.0f, 1.0f, 1.0f, 1.0f 
		};
		// Multiplier applied to each player's received damage.
		// Applies to most sources of damage, 
		// especially direct sources such as melee attacks or non-explosive hits.
		static inline std::vector<float> vfDamageReceivedMult =
		{
			1.0f, 1.0f, 1.0f, 1.0f 
		};
		// Flop damage mult. For no reason at all.
		static inline std::vector<float> vfFlopDamageMult = 
		{
			1.0f, 1.0f, 1.0f, 1.0f 
		};
		// Damage mult for thrown objects.
		static inline std::vector<float> vfThrownObjectDamageMult = 
		{
			1.0f, 1.0f, 1.0f, 1.0f 
		};

		//-------------
		//[Controller]:
		//-------------
		// Proportion of full displacement from center (32768) to serve as analog stick deadzone.
		static inline std::vector<float> vfAnalogDeadzoneRatio = 
		{
			0.25f, 0.25f, 0.25f, 0.25f 
		};
		// Proportion of full trigger activation (255) to serve as trigger deadzone.
		static inline std::vector<float> vfTriggerDeadzoneRatio =
		{
			0.25f, 0.25f, 0.25f, 0.25f
		};

		//--------------
		//[Progression]:
		//--------------
		// Skill XP multiplier applied whenever this player receives XP for using a skill.
		static inline std::vector<float> vfSkillXPMult = 
		{
			1.0f, 1.0f, 1.0f, 1.0f 
		};

		//------------
		//[Targeting]:
		//------------
		// Can this player select other players with their crosshair?
		static inline std::vector<bool> vbCanTargetOtherPlayers = 
		{
			true, true, true, true 
		};
		// Crosshair magnetism. Crosshair slows down when over a target.
		// Use crosshair magnetism when selecting Actors.
		static inline std::vector<bool> vbCrosshairMagnetismForActors = 
		{
			true, true, true, true 
		};
		// Use crosshair magnetism when selecting Object References.
		static inline std::vector<bool> vbCrosshairMagnetismForObjRefs = 
		{
			true, true, true, true 
		};
		// Is friendly fire enabled towards actors friendly to the party ?
		// (followers, other players, summons, etc.)
		static inline std::vector<bool> vbFriendlyFire = 
		{
			false, false, false, false
		};
		// Use aim correction to automatically target a nearby hostile actor 
		// in the player's facing direction when no object is selected by the crosshair.
		static inline std::vector<bool> vbUseAimCorrection = 
		{
			true, true, true, true 
		};
		// Angular window (radians) centered at the player's current left stick/heading angle
		// within which any actors will be considered as targets for attack aim correction.
		// Only used when the player is not targeting an actor with their crosshair.
		static inline std::vector<float> vfAimCorrectionFOV = 
		{
			PI / 4.0f, PI / 4.0f, PI / 4.0f, PI / 4.0f 
		};
		// Max angular adjustment speed when adjusting the player's aim pitch.
		// Radians per second.
		static inline std::vector<float> vfMaxAimPitchAdjustmentRate = 
		{
			PI, PI, PI, PI 
		};
		// Projectile trajectory adjustment types:
		// kAimDirection: No projectile trajectory adjustment. 
		// Projectile is fired in the player's aiming direction.
		// kHoming: Homing projectiles that follow the crosshair or last selected target actor.
		// kPrediction: Projectiles move to the predicted target intercept position.
		// Note for aim prediction (WIP): 
		// Tends to be pretty inaccurate when targets change direction quickly
		// or when shooting a lobbed projectile.
		static inline std::vector<uint32_t> vuProjectileTrajectoryType = 
		{ 
			!ALYSLC::ProjectileTrajType::kHoming, 
			!ALYSLC::ProjectileTrajType::kHoming, 
			!ALYSLC::ProjectileTrajType::kHoming, 
			!ALYSLC::ProjectileTrajType::kHoming 
		};

		//-----
		//[UI]:
		//-----
		// Animated crosshair (animated rotation and oscillation about center).
		static inline std::vector<bool> vbAnimatedCrosshair = 
		{
			true, true, true, true 
		};
		// Fade inactive crosshair.
		static inline std::vector<bool> vbFadeInactiveCrosshair = 
		{
			true, true, true, true 
		};
		// Recenter inactive crosshair.
		static inline std::vector<bool> vbRecenterInactiveCrosshair = 
		{
			true, true, true, true
		};
		// [True]: Use Skyrim-style crosshair.
		// [False]: Use generic four-prongs style.
		static inline std::vector<bool> vbSkyrimStyleCrosshair = 
		{
			true, true, true, true 
		};
		// Crosshair gap radius (pixels).
		static inline std::vector<float> vfCrosshairGapRadius =
		{
			3.0f, 3.0f, 3.0f, 3.0f 
		};
		// Crosshair side length (pixels).
		static inline std::vector<float> vfCrosshairLength = 
		{
			7.0f, 7.0f, 7.0f, 7.0f
		};
		// Crosshair thickness (pixels).
		static inline std::vector<float> vfCrosshairThickness = 
		{
			4.0f, 4.0f, 4.0f, 4.0f 
		};
		// Crosshair horizontal movement sensitivities for each player.
		// Fraction of max pixels per second (fCrosshairMaxTraversablePixelsPerSec).
		// For example, a value of '0.5' means the crosshair will move
		// across a distance equal to half the max pixels traversable per second.
		static inline std::vector<float> vfCrosshairHorizontalSensitivity = 
		{
			0.5f, 0.5f, 0.5f, 0.5f 
		};
		// Crosshair vertical movement sensitivities for each player.
		// Fraction of max pixels per second.
		// For example, a value of '0.5' means the crosshair will move
		// across a distance equal to half the max pixels traversable per second.
		static inline std::vector<float> vfCrosshairVerticalSensitivity = 
		{
			0.5f, 0.5f, 0.5f, 0.5f 
		};
		// Player indicator side length (pixels).
		static inline std::vector<float> vfPlayerIndicatorLength = 
		{
			30.0f, 30.0f, 30.0f, 30.0f 
		};
		// Player indicator side thickness (pixels).
		static inline std::vector<float> vfPlayerIndicatorThickness = 
		{
			5.0f, 5.0f, 5.0f, 5.0f
		};
		// Crosshair body is the same color as the overlay.
		// Red, Green, Cyan, Yellow by default.
		static inline std::vector<uint32_t> vuOverlayRGBAValues = 
		{
			0xFF0000AA, 0x00FF00AA, 0x00FFFFAA, 0xFF00FFAA 
		};
		// Inner outline is always drawn.
		// Black by default.
		static inline std::vector<uint32_t> vuCrosshairInnerOutlineRGBAValues = 
		{
			0x000000AA, 0x000000AA, 0x000000AA, 0x000000AA 
		};
		// Outer outline is only drawn when the crosshair is over a selectable target.
		// White by default.
		static inline std::vector<uint32_t> vuCrosshairOuterOutlineRGBAValues = 
		{
			0xFFFFFFAA, 0xFFFFFFAA, 0xFFFFFFAA, 0xFFFFFFAA 
		};
		// Player indicator visibility modes:
		// kDisabled: Never show the player indicator.
		// kLowVisibility: Only show when the player is obscured 
		// or below a certain pixel height on the screen.
		// kAlways: Show the player indicator at all times.
		static inline std::vector<uint32_t> vuPlayerIndicatorVisibilityType = 
		{ 
			!ALYSLC::PlayerIndicatorVisibilityType::kLowVisibility, 
			!ALYSLC::PlayerIndicatorVisibilityType::kLowVisibility, 
			!ALYSLC::PlayerIndicatorVisibilityType::kLowVisibility, 
			!ALYSLC::PlayerIndicatorVisibilityType::kLowVisibility 
		};

		//------------------
		//[Timer Intervals]:
		//------------------
		// Minimum number of seconds to traverse highlighted target 
		// at max right stick displacement.
		static inline std::vector<float> vfMinSecsCrosshairTargetTraversal = 
		{
			0.2f, 0.2f, 0.2f, 0.2f 
		};
		// Seconds before fading/re-centering crosshair when it is not over a target, 
		// not moving, and not in 'face target' mode.
		static inline std::vector<float> vfSecsBeforeRemovingInactiveCrosshair = 
		{ 
			2.0f, 2.0f, 2.0f, 2.0f 
		};
		// Seconds to oscillate the animated crosshair from fully contracted to fully expanded.
		static inline std::vector<float> vfSecsToOscillateCrosshair = 
		{
			1.5f, 1.5f, 1.5f, 1.5f 
		};
		// Seconds to rotate the crosshair from its default '+' orientation 
		// to its 'x' (face target) configuration or vice versa.
		static inline std::vector<float> vfSecsToRotateCrosshair = 
		{
			0.5f, 0.5f, 0.5f, 0.5f 
		};

		//==========================================================
		//==========================================================
		// [Constant, non-MCM-customizable global options and data]:
		//==========================================================
		//==========================================================

		//-------------------
		//[Camera Constants]:
		//-------------------
		// Interpolation factors [0, 1].
		// Cam interp factor is lower if camera collisions are enabled to smooth out 
		// jumps past obstacles and movement above the ground.
		// May also be compensating for some sub-par positioning code. Heh.
		static inline const float fCamCollisionInterpFactor = 0.2f;
		static inline const float fCamManualPosInterpFactor = 0.5f;
		static inline const float fCamNoCollisionInterpFactor = 0.266667f;
		// Max raycast distance for calculating the camera target position.
		// Also max calculated auto zoom delta at which to bail out of the loop 
		// when attempting to keep all players in frame.
		static inline const float fMaxRaycastAndZoomOutDistance = 16384.0f;
		// Constants used as sentinel values when calculating the auto zoom radial distance
		// at which all players are within the camera's frustum.
		// Min calculated auto zoom delta at which to bail out of loop 
		// when attempting to keep all players in frame.
		static inline const float fMinAutoZoomDelta = 1.0f / 128.0f;
		// Interpolation interval when oscillating the lock on target's indicator.
		static inline const float fSecsCamLockOnIndicatorOscillationUpdate = 0.5f;
		// Interpolation interval when calculating the average of player movement pitch angles.
		static inline const float fSecsCamMovementPitchUpdate = 2.0f;
		// Interpolation interval when calculating the average of player movement yaw angles 
		// relative to the camera's yaw.
		static inline const float fSecsCamMovementYawUpdate = 0.05f;
		// Interpolation interval when fading obstructions in/out.
		static inline const float fSecsObjectFadeInterval = 0.75f;

		//-------------------
		//[Cheats and Debug]:
		//-------------------
		// Speeding up dodge animations also shortens dodge distance.
		// NOTE: Does not affect dash dodge.
		static inline const float fDodgeAnimSpeedFactor = 1.5f;
		// Animation play speed factor for equip/unequip/draw/sheathe and dodge animations.
		// Have had crashes with Precision when this value is too high.
		// 10.0 seems to rarely cause crashes, if ever. Can set lower just to be safe.
		// 5.0 seems to be completely stable.
		static inline const float fEquipAnimSpeedFactor = 5.0f;

		//-------------
		//[Controller]:
		//-------------
		// Max number of base frames between tapping (pressing + releasing) an input 
		// that can be considered a consecutive press.
		// ~0.2 seconds at 60 fps.
		static inline const float fConsecTapsFrameCountWindow = 13;
		// Max number of controllers supported.
		static inline const std::uint8_t fMaxNumControllers = 4;

		//-----------
		//[Movement]:
		//-----------
		// Multiplier to apply to rotation speed for each movement type.
		// NOTE: Setting this value above 8 leads to choppier rotation,
		// especially when facing a target.
		static inline const float fBaseMTRotationMult = 8.0f;

		//----------
		//[Physics]:
		//----------
		// Gravitational acceleration constant (units: meters / second^2).
		static inline const float fG = 9.80665f;
		// Air resistance constant (units: 1 / second).
		static inline const float fMu = 0.099609375f;

		//-----------------
		//[Player Actions]:
		//-----------------
		// Attempt to perform generic 'character' skeleton killmoves on NPCs 
		// with skeletons that have no assigned killmoves.
		// WARNING: Will likely look odd or completely bug out when performed. 
		// Disabling will prevent such NPCs from being killmove'd.
		static inline const bool bUseGenericKillmovesOnUnsupportedNPCs = false;
		// Absolute max thrown object speed (in-game units / second).
		static inline const float fAbsoluteMaxThrownRefrReleaseSpeed = 15000.0f;
		// Base max speed at which a grabbed reference can move (ingame units / second).
		static inline const float fBaseGrabbedRefrMaxSpeed = 131072.0f;
		// Base max thrown object speed (in-game units / second).
		static inline const float fBaseMaxThrownRefrReleaseSpeed = 1500.0f;
		// Speed up the player's animations with this factor while dash dodging.
		static inline const float fDashDodgeAnimSpeedFactor = 2.0f;
		// Max grabbed player speed multiplier.
		static inline const float fGrabbedPlayerMaxSpeedMult = 0.5f;
		// Max speed at which a grabbed reference can rotate (radians / second).
		static inline const float fGrabbedRefrBaseRotSpeed = PI;
		// Seconds to hold the grab bind in order to throw the grabbed form at max speed.
		static inline const float fGrabHoldSecsToMaxReleaseSpeed = 0.5f;
		// Max radial distance from a revive target to start reviving.
		static inline const float fMaxDistToRevive = 150.0f;
		// Min health reached while reviving another player.
		static inline const float fMinHealthWhileReviving = 1.0f;
		// Seconds before attracting another nearby clutter refr while holding grab.
		static inline const float fSecsBeforeAutoGrabbingNextRefr = 0.25f;
		// Max number of seconds to block all input actions 
		// after the player action manager resumes.
		static inline const float fSecsToBlockAllInputActions = 1.0f;
		// Max recursion depth when getting composing inputs by breaking down player actions 
		// into their own composing inputs.
		static inline const uint8_t uMaxComposingInputsCheckRecursionDepth = 10;
		// Dash dodge setup frame count (before leaning in movement direction at 60 FPS).
		static inline const uint32_t uDashDodgeSetupFrameCount = 6;

		//----------
		//[Stealth]:
		//----------
		// Radial distance from hostile actor to start performing detection checks.
		// 45 feet from leveling page converted into in-game units:
		// https://en.uesp.net/wiki/Skyrim:Leveling
		// https://www.creationkit.com/index.php?title=Unit
		static inline const float fHostileTargetStealthRadius = 960.0f;

		//------------
		//[Targeting]:
		//------------
		// Max time to continue adjusting the path of managed projectiles.
		static inline const float fMaxProjAirborneSecsToTarget = 60.0f;
		// Maximum number of seconds after releasing an object before it is no longer handled.
		static inline const float fMaxSecsBeforeClearingReleasedRefr = 10.0f;
		// Minimum distance (in-game units) to move 
		// before automatically refreshing nearby references when activation cycling.
		static inline const float fMinMoveDistToRefreshRefrs = 10.0f;
		// Minimum angle to turn before automatically refreshing nearby references 
		// when activation cycling.
		static inline const float fMinTurnAngToRefreshRefrs = 5.0f * PI / 180.0f;
		// Max activation reach multiplier when mounted.
		static inline const float fMountedActivationReachMult = 2.0f;
		// Max number of seconds over which the last released refr is handled 
		// following a collision.
		static inline const float fSecsAfterCollisionBeforeClearingReleasedRefr = 5.0f;
		// If the distance from the player's attack source to the target position
		// is less than or equal to this value, begin slowing the player's rotation.
		static inline const float fTargetAttackSourceDistToSlowRotation = 200.0f;
		// Number of unique projectiles managed before checking for expired projectiles 
		// to remove from the managed queue.
		static inline const uint8_t uManagedPlayerProjectilesBeforeRemoval = 50;

		//------------------
		//[Timer Intervals]:
		//------------------
		// Seconds between updating crosshair text if the previous message's type
		// is not of the same type as the current one.
		static inline const float fSecsBetweenDiffCrosshairMsgs = 2.0f;
		// Seconds between attempting to move invalid players to player 1.
		static inline const float fSecsBetweenInvalidPlayerMoveRequests = 2.0f;
		// Seconds between stealth state checks while sneaking.
		static inline const float fSecsBetweenStealthStateChecks = 0.25f;
		// Seconds between crosshair target visibility checks.
		static inline const float fSecsBetweenTargetVisibilityChecks = 1.0f;
		// Seconds without LOS on a crosshair target after which to clear the target.
		static inline const float fSecsWithoutLOSToInvalidateTarget = 7.0f;
		// Framecount to fully blend in/out arm and torso node rotations
		// to the default or custom values.
		static inline const uint32_t uBlendPlayerNodeRotationsFrameCount = 12;

		
		//==========================
		//==========================
		// [Importing MCM Settings]:
		//==========================
		//==========================
		// Much of the settings import code is modified from ersh1's TrueHUD.
		// Full credits go to him:
		// https://github.com/ersh1/TrueHUD/blob/master/src/Settings.h
		// https://github.com/ersh1/TrueHUD/blob/master/src/Settings.cpp

		// Paths to settings files.
		static inline const std::filesystem::path defaultFilePathSSE = 
		(
			"Data/MCM/Config/ALYSLC/settings.ini"
		);
		static inline const std::filesystem::path defaultFilePathEnderal = 
		(
			"Data/MCM/Config/ALYSLC Enderal/settings.ini"
		);
		static inline const std::filesystem::path userSettingsFilePathSSE = 
		(
			"Data/MCM/Settings/ALYSLC.ini"
		);
		static inline const std::filesystem::path userSettingsFilePathEnderal =
		(
			"Data/MCM/Settings/ALYSLC Enderal.ini"
		);
		
		// Check if the given setting key exists in the given ini section.
		static inline bool SettingExists
		(
			CSimpleIniA& a_ini, const char* a_section, const char* a_settingKey
		)
		{
			return a_ini.GetValue(a_section, a_settingKey);
		}

		// Import all MCM settings.
		static void ImportAllSettings();

		// Import each player's binds.
		// Returns true if all binds are valid.
		static bool ImportBinds(CSimpleIniA& a_ini);

		// Import settings that pertain to all players.
		static void ImportGeneralSettings(CSimpleIniA& a_ini);

		// Import each player's specific settings.
		static void ImportPlayerSpecificSettings(CSimpleIniA& a_ini);

		// Read a boolean setting with the given key from the given section into the outparam.
		// Returns true if successful.
		static bool ReadBoolSetting
		(
			CSimpleIniA& a_ini, const char* a_section, const char* a_settingKey, bool& a_settingOut
		);

		// Read a boolean setting with the given key from the given section 
		// into the given index in the outparam list.
		// Returns true if successful.
		static bool ReadBoolSettingToIndex
		(
			CSimpleIniA& a_ini,
			const char* a_section,
			const char* a_settingKey, 
			std::vector<bool>& a_settingsList, 
			uint8_t&& a_index
		);

		// Read a float setting with the given key from the given section into the outparam.
		// Also clamp the result to the given lower and upper bounds, 
		// and/or convert the result to radians.
		// Returns true if successful.
		static bool ReadFloatSetting
		(
			CSimpleIniA& a_ini, 
			const char* a_section, 
			const char* a_settingKey, 
			float& a_settingOut, 
			float&& a_lowerLimit, 
			float&& a_upperLimit,
			bool&& a_convertToRadians = false
		);

		// Read a signed 32-bit integer setting with the given key 
		// from the given section into the outparam.
		// Also clamp the result to the given lower and upper bounds.
		// Returns true if successful.
		static bool ReadInt32Setting
		(
			CSimpleIniA& a_ini, 
			const char* a_section, 
			const char* a_settingKey,
			int32_t& a_settingOut,
			int32_t&& a_lowerLimit,
			int32_t&& a_upperLimit
		);

		// Read an unsigned 32-bit integer setting with the given key 
		// from the given section and parse an RGB value from it into the outparam.
		// Returns true if successful.
		static bool ReadRGBStringSetting
		(
			CSimpleIniA& a_ini,
			const char* a_section, 
			const char* a_settingKey, 
			uint32_t& a_settingOut
		);

		// Read a string setting with the given key from the given section into the outparam.
		// Returns true if successful.
		static bool ReadStringSetting
		(
			CSimpleIniA& a_ini, 
			const char* a_section,
			const char* a_settingKey, 
			RE::BSFixedString& a_settingOut
		);

		// Read an unsigned 32-bit integer setting with the given key 
		// from the given section into the outparam.
		// Also clamp the result to the given lower and upper bounds.
		// Returns true if successful.
		static bool ReadUInt32Setting
		(
			CSimpleIniA& a_ini, 
			const char* a_section, 
			const char* a_settingKey, 
			uint32_t& a_settingOut, 
			uint32_t&& a_lowerLimit, 
			uint32_t&& a_upperLimit
		);
	};
}

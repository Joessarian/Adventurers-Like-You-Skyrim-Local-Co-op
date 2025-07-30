# Adventurers Like You: Skyrim Local Co-op ALPHA

![Banner](https://i.imgur.com/VhQyyN0.png)

> What's better than experiencing the boundless magic of Skyrim?  
> ***Experiencing Skyrim with friends and family — with adventurers like you!***

## [Features]
- ***Local co-op with controllers*** for 2-4 players.
- ***Fully adjustable co-op camera*** that is controllable by any player, with toggleable camera collisions, object fade, and additional lock on and free cam modes (no split screen).
- ***Character customization options for companion players***. Includes options to change the player's name, gender, race, class, NPC preset appearance, weight, and height.
- ***Crosshairs for each player*** to target and interact with objects and NPCs, complete with adjustable appearance, sensitivity, fade options, and more.
- ***New expansive control scheme*** built with co-op in mind that features 70 different customizable binds, 4 assignable inputs or player actions per bind, and different trigger options (on press, on release, on press and release, on hold, and on consecutive tap). Enable/disable any bind or even assign recursive binds composed of other binds.
- ***Any player can control menus***. All vanilla menus are supported and players can switch which player is in control of dialogue. Players can also freely trade items by switching tabs from a companion player's inventory or by using the Gift Menu bind.
- ***Different inventories, favorited items, and hotkeys for each player.*** Quest items, books, notes, learned skillbooks, keys, gold, and lockpicks are shared and given to player 1 for ease of access, but all other items are lootable by companion players.
- ***Independent skill leveling and perk selection***. While all players share the same character level, each player will level their skills separately using the same level-on-use system as the vanilla game. On character level-up, each player can increase their Health, Magicka, or Stamina, and select different perks.
- ***Balancing/difficulty options*** featuring per-player damage dealt and received multipliers, per-player health/magicka/stamina regeneration and cost multipliers, per-player skill XP gain multipliers, a character level XP threshold multiplier for all players, and more.
- ***Papyrus and SKSE APIs*** to facilitate mod compatibility efforts and allow for development of additional features in the future. More functions to come!
- ***New mechanics and systems:***
	- Assignable emote idle events for player expression.
	- Arm adjustment (if enabled) with collisions to allow for more interactivity with the world.
	- Dash dodge with a customizable I-frame count and speed/distance.
	- Killmoves system with customizable triggering health threshold and chance.
	- Lob-able projectiles and adjustable attack pitch with new spinal rotation system.
	- Revive system that allows living players to revive their fallen comrades within a certain time interval.
	- Tactical ragdolling. Yes.
	- Telekinetically grab and throw stuff. Clutter is now your best friend!
	- Two player co-operative lockpicking, with one player rotating the pick and the other rotating the lock.

... and more secret mechanics just waiting to be discovered!
- ***Quality of life features:***  
	- Aim assist and projectile trajectory options per-player.
	- Basic cheats for players who wish to use them.
	- Crosshair sensitivity and stickiness options to allow for easier object selection.
	- Customizable player movement and rotation speeds.
	- Debug menu and dedicated debug binds to resolve issues on-the-fly.
	- Equip favorited items/spells via cycling or through radial selection of hotkeyed items with the right stick.
	- Exchange items between player 1 and companion players.
	- Friendly fire toggleable per-player.
	- Loot all grabbed items, all items from a targeted container or corpse, or all nearby loose items or items in containers.
	- Menu border overlay color-coded to show which player is in control of open menus.
	- Special action bind with a varying effect based on what weapons/magic the player has equipped.
	- Teleport to other players.

... and more!

## [Build Steps and Tips]
### Setting Up the Build Environment and Dependencies.
- Set up Visual Studio 2022 and install desktop development with C++.
- This project uses [po3’s fork of CommonLib](https://github.com/powerof3/CommonLibSSE). 
Ensure you have all of CommonLibSSE’s dependencies set up.
- Clone ALYSLC to the directory of your choice by opening a command prompt in that directory and typing: 
  ```
  git clone https://github.com/Joessarian/Adventurers-Like-You-Skyrim-Local-Co-op
  ```
- Then move into the new repository folder by typing:
  ```
  cd Adventurers-Like-You-Skyrim-Local-Co-op
  ```
- Update the CommonLib submodule with: 
  ```
  git submodule init
  git submodule update
  ```
- Move all files/folders in the respository's `Data` subfolder to either a new MO2 mod folder:
  - `<MO2 folder>/<MO2 mods folder>/Adventurers Like You - Skyrim Local Co-op/`
- Or to your Skyrim installation’s `Data` folder:
  - `<Skyrim Installation Folder>/Data”`

### Pre-Project Generation
- Search `environment variables` in the Windows search bar and choose `Edit the system environment variables`.
- Click on `Environment Variables`.
- Click on `New` under the `User variables for <your user name>` list.
- If you’re building for Skyrim SE (versions including 1.5.97 and below):
  - Create a variable with the name `Skyrim64Path` and have it point to your Skyrim SE installation folder.
    - For example: `<Drive Letter>:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`
  - Create a variable with the name `ALYSLCPluginPathSE` and enter where you’d like to copy the compiled .dll and .pdb built from the project we’re about to generate. Examples: 
    - `<MO2 folder>/mods/Adventurers Like You - Skyrim Local Co-op/SKSE/Plugins`
    - `<Skyrim Installation Folder>/Data/SKSE/Plugins`
- If you’re building for Skyrim AE (versions after 1.5.97):
  - Create a variable with the name `SkyrimAEPath` and have it point to your Skyrim AE installation folder.
  - Create a variable with the name `ALYSLCPluginPathAE` to point to where the output .dll and .pdb will be exported to.

### Generating the Project
- If not open already, open a command prompt where you cloned the respository.
- To generate a project for SE, type:
  ```
  cmake --preset vs2022-windows-vcpkg-se
  ```
- To generate a project for AE, type:
  ```
  cmake --preset vs2022-windows-vcpkg-ae
  ```
- If successful, open the newly-generated solution folder `build` for SE, or `buildae` for AE.

### Post-Project Generation
Build via the command line:
- If not open already, open a command prompt where you cloned the respository.
- For SE:
  ```
  cmake --build build --config Release
  ```
- For AE:
  ```
  cmake --build buildae --config Release
  ```

Build via Visual Studio 2022:
- Open up the `ALYSLC.sln` inside your newly-generated `build` or `buildae` folder.
- Switch the active solution configuration from `Debug` to `Release`. - ***Note***: Building in debug currently does not work, so ensure the project's build configuration is set to release.
- If you wish to enable all debug prints (`SPDLOG_DEBUG()` calls), uncomment ```#define ALYSLC_DEBUG``` in `PCH.h`. ***Always comment out the line before making a PR***. This is basically a reminder for myself, as I tend to forget.
- Build the solution with `Ctrl + Shift + B` or by clicking on `Build` and then `Build Solution`.
- If the build succeeded and you previously specified an output path for the compiled plugin, you’re all set to test out your changes in-game!
- Special notes on configuring for Skyrim/Enderal before launching the game:
   - If you are playing Enderal, delete or deactivate the `ALYSLC.esp` in your mod manager.
      - Modify the mod's settings via the `/Data/MCM/Settings/ALYSLC Enderal.ini` file.
   - If you are playing Skyrim, delete or deactivate the `ALYSLC Enderal.esp` in your mod manager.
      - Modify the mod's settings via the `/Data/MCM/Settings/ALYSLC.ini` file. 
- Note for subsequent builds:
   - If the generated `ALYSLC.pdb` file is growing in size rapidly with each subsequent build:
      - From the main VS 2022 window, click on `Project` then `ALYSLC Properties` and in the `Configuration Properties` pane, click on `Build Events`. Then choose `Pre-Build`, click on the `Command Line` entry, and type in:
        ```
        del /s /q $(TargetDir)*.pdb
        ```
      - Click `Apply` and then `Ok`.
	  - This command will delete the .pdb and force the linker to regenerate it every time the project is built.

Editing Papyrus scripts:
- If you're using Visual Studio Code, check out [Joel Day's 'Papyrus for Visual Studio Code' extension](https://marketplace.visualstudio.com/items?itemName=joelday.papyrus-lang-vscode) to set up a build environment for editing Papyrus scripts.
- If you wish to edit and build any of the Papyrus scripts packaged with this mod (`.psc` files in the mod's`/Data/Scripts/Source` folder), ensure you've already extracted all script source files from the following mods into your Skyrim install directory's `/Data/Scripts/Source` or `/Data/Source/Scripts`folder:
	- [SKSE](https://skse.silverlock.org/)
	- [Papyrus Extender](https://www.nexusmods.com/skyrimspecialedition/mods/22854)
	- [Papyrus Util SE](https://www.nexusmods.com/skyrimspecialedition/mods/13048)
	- [UIExtensions](https://www.nexusmods.com/skyrimspecialedition/mods/17561)
- Functions from the Papyrus files provided by these mods are utilized in ALYSLC's Papyrus source code.

## [Coding Style]
- The C++ codebase uses a custom style that emphasizes code-blocking of complex boolean conditions, assignments, and long parameter/argument lists. At the moment, ClangFormat does not seem to offer the style customization options that would automate formatting the codebase in this way, so if possible, please keep the following style guidelines in mind:
	- All function parameters or arguments should have the ```a_``` prefix.
	- Commonly used members can have abbreviated names:
		- Examples:
			- Players: (CoopPlayer -> p) 
			- Managers: (EquipManager -> em, PlayerActionManager -> pam, etc.)
			- Handlers: (ControllerDataHandler -> cdh, etc.) 
	- Limit lines to 100 characters. Highly recommend installing this Visual Studio extension to provide a visual guideline for the column limit: [Editor Guidelines Extension for VS 2022](https://marketplace.visualstudio.com/items?itemName=PaulHarrington.EditorGuidelinesPreview&ssr=false#overview).
		- Some exceptions (flexible, use best judgement):
			- Debug prints
			- Declarations
			- Embedded URLs
			- Long typenames
			- Long names for functions/members that are part of a namespace
	- Brackets should adhere to the ['Allman' style](https://en.wikipedia.org/wiki/Indentation_style#Allman_style).
	- Wrap parentheses/braces around longer assignments, chained function calls, function argument lists, etc. to create a code block around them. The resulting code block can be packed or further broken up with line breaks as the developer sees fit.
	- In short, when a line of code exceeds the column limit, feel free to use more parentheses to order operations explicitly and drop down another line to make the code more readable.
	
### Examples:
```
static void GlobalCoopData::SomeFunction(const std::shared_ptr<CoopPlayer>& a_p, float a_1, float a_2, float a_3, float a_4, float a_5);

// Becomes:

static void GlobalCoopData::SomeFunction
(
	const std::shared_ptr<CoopPlayer>& a_p,
	float a_1,
	float a_2,
	float a_3,
	float a_4,
	float a_5
);
```
```
bool something = (bool1) && ((bool2 || bool3) && ((operand1 * operand2 == 0) || longFunc1(func2(arg1, arg2))));

// Becomes:

bool something = 
(
	(bool1) &&
	(
		(bool2 || bool3) &&
		(
			(operand1 * operand2 == 0) ||
			longFunc1
			(
				func2(arg1, arg2)
			)
		)
	)
);
```

## [For Users]
### Getting Started and Important Notes
- First and foremost, ***expect jankiness/clunkiness***, simply by virtue of having to hack in controllable NPCs to use as players and from attempting to apply player 1 features and player agency to these companion players. I've tried to make the mod's mechanics and features as modular as possible, with mod compatibility in mind, and will continue to offer additional customizability in the future, so check out the mod's MCM if there's a feature you'd like to adjust or disable entirely.
- The mod is meant to be played locally and ***is NOT a fully-fledged multiplayer experience*** like the incredible [Skyrim Together Reborn](https://www.nexusmods.com/skyrimspecialedition/mods/69993) mod. Online play is still possible via leveraging Steam's [Remote Play Together](https://store.steampowered.com/remoteplay) with [Remote Play Whatever](https://github.com/m4dEngi/RemotePlayWhatever) or via streaming with [Parsec](https://parsec.app/). Your mileage may vary.
- This mod requires plugging ***at least two XInput-compatible controllers*** into your computer before starting a co-op session.
- Almost all of the mod's testing was done on ***Skyrim version 1.5.97, so for the most stable experience (and for the best mod compatibility), your best bet is to downgrade your Skyrim installation to this version***.
- If you are experiencing issues with player 1 using ALYSLC's co-op binds and have installed a mod that modifies ```controlmap.txt```, (local path: ```Data/Interface/Controls/PC/controlmap.txt```) ***backup and then revert to the default controls through the settings menu in-game, remove the custom controlmap temporarily, or disable the mod.***
- If you are playing ***Skyrim***, enable 'ALYSLC.esp' and disable 'ALYSLC Enderal.esp' in your load order. Otherwise, if you are playing ***Enderal***, enable 'ALYSLC Enderal.esp' and disable 'ALYSLC.esp'.
- After installing the mod for the first time, always ***start a new playthrough and dedicate the playthrough to co-op***. Minimize progressing player 1's character outside of co-op.
- ***Important note on starting a new game***: If sticking with the ***vanilla start*** sequence, summoning other players right from the get-go causes a slew of bugs (and horrifying glitches) that must be fixed with debug options + console commands and is probably not worth the effort. ***Try waiting until player 1 has their shackles removed, before summoning other players***. Otherwise, have fun tipping over the carriage, getting player 1 stuck under the driver's seat, and feverishly trying out every console command and debug option available to remedy a hilarious situation that is quickly getting out of hand.
- As of this time, ***removing the mod mid-playthrough is not supported*** and will lead to freezes when loading saves.
- To summon other players, after loading a save, first ***ensure player 1 is not in combat***. Then hold the `Back` button and press the `Start` button on ***player 1's*** controller. This will establish which player is recognized as player 1 by the mod and open the Summoning Menu.
- A tri-colored border overlay will appear around any menu opened during co-op. This ***player menu control (PMC) border's colors identify which player is in control of menus*** and correspond to the player's chosen main UI overlay color, crosshair inner outline, and crosshair outer outline colors.
- ***Save frequently*** while playing and ***disable any sources of autosaving or only load manual saves*** to minimize issues where a companion player's data might've been imported onto player 1 and saved.
- ***Stick together*** when possible to ensure all players are on-screen and easy to see. There are also some options to improve player visibility, such as player indicators, camera object fade options, and player-focal camera positioning when players are far apart. And finally, if seeing "the void" outside of the traversable map does not bother you, disabling camera collisions altogether provides the smoothest experience.
- ***Bugs are inevitable, and in some cases, correctable***. Before reporting an issue, please use the ***debug binds*** or ***Debug Menu options*** to troubleshoot issues as they arise. If the bug recurs frequently even after using said options, please report the issue and include what debug options you've used and/or a crash log if you've also installed a crash logger.
- Have a peek at the mod's ***MCM*** once it fully loads to ***learn about and customize the mod's extensive settings***.  Settings include:
  - Camera options
  - Cheats
  - Character and balancing options for all players
  - Emote idle assignment
  - Extra mechanics for all players
  - Menu and user interface options
  - Movement speed/rotation options for all players
  - Progression options
  - Unique controls customization for each player
### Binds
![](https://i.imgur.com/f6itQHk.png)
### Performance
- Expect a ***loss of, on average, at least 7-15% of your base framerate with some framerate spikes***, especially when camera collisions are enabled or when opening menus, such as a large player inventory. The performance hit also depends on how many players are summoned, what actions players are performing, and what camera options are active.
- ***Turning off camera collisions, obstruction fading, and not removing occlusion*** produces the best performance when using the co-op camera. ***See the MCM for other performance-impacting options.***
- Highly recommend ***capping your framerate to 60 FPS if possible***, since some of the physics-related code in this mod can act up at higher framerates.
- I realize that the code is inefficient and sub-optimal, especially with all the workarounds in place to make certain features work. However, as the project, and my programming skills, are still very much in alpha, ***expect additional performance optimizations down the line once the mod becomes more stable***.

### Installation

#### Prerequisite Mods
- Install the following mods + all their necessary prerequisites. Also have a look at the recommended mods below, as quite a few are almost essential.
	- [SKSE64](https://skse.silverlock.org/)
	- [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604)
	- [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
	- [MCM Helper](https://www.nexusmods.com/skyrimspecialedition/mods/53000)
	- [PapyrusExtender](https://www.nexusmods.com/skyrimspecialedition/mods/22854)
	- [PapyrusUtil SE](https://www.nexusmods.com/skyrimspecialedition/mods/13048)
	- [Party Combat Parameters (SE Only)](https://www.nexusmods.com/skyrimspecialedition/mods/57127) and/or [TrueHUD](https://www.nexusmods.com/skyrimspecialedition/mods/62775)
	   - Party Combat Parameters provides essential functionality by showing Health/Magicka/Stamina and other useful stats for companion players via a customizable widget. Unfortunately, it is not compatible with AE, so installing TrueHUD is the best alternative if running AE. 
	   - TrueHUD also provides super convenient and customizable floating health bars for companion players and enemies while in combat. 
	   - Recommend installing both and playing around with their customization options for the best experience.
	- [Precision](https://www.nexusmods.com/skyrimspecialedition/mods/72347)
	- [UIExtensions](https://www.nexusmods.com/skyrimspecialedition/mods/17561)
	   - Note: Already comes packaged with Enderal, so no need to install separately unless you’re only playing Skyrim.

#### Recommended Mods (Not Strictly Required)
- Additional mods which are highly recommended, but not required for this mod to function:
	- [Alt-Tab Stuck Key Fix NG](https://www.nexusmods.com/skyrimspecialedition/mods/148466) 
		- Fixes some issues with inputs 'sticking' after alt-tabbing and controller inputs registering as keyboard inputs, overriding or preventing some co-op controls from functioning. Recommended if you encounter issues with co-op controls when tabbing out and back into Skyrim.
	- [Alternate Start](https://www.nexusmods.com/skyrimspecialedition/mods/272) or another mod that alters the opening sequence.
		- Allows players to skip everyone’s favorite intro sequence and to summon companion players right away. ***Highly recommended***
	- [Auto Input Switch](https://www.nexusmods.com/skyrimspecialedition/mods/54309)
		- Seamlessly switch between controller and keyboard + mouse inputs.
	- [Detection Meter SE](https://www.nexusmods.com/skyrimspecialedition/mods/63057), [Detection Meter AE](https://www.nexusmods.com/skyrimspecialedition/mods/77350)
		- Properly show detection levels for player 1 while sneaking when the co-op camera is active.
	- [Face Discoloration Fix](https://www.nexusmods.com/skyrimspecialedition/mods/42441)
		- Removes the face and body tone mismatch and neck seam issues when customizing companion players' appearances. ***Highly recommended***.
	- [Motion Sensitive Fix SE](https://www.nexusmods.com/skyrimspecialedition/mods/35833)
		- Removes almost every source of camera shake. Use if you're still experiencing camera shake while the co-op camera is active. ***Highly recommended***.
	- [No Silly Physics Damage - Carts Pots Bones etc](https://www.nexusmods.com/skyrimspecialedition/mods/36132)
		- Removes excessive physics damage when a havok-enabled object is squeezed against a character, such as when walking into a cart or cauldron. ***Highly recommended***.
	- [Quick Loot RE/EE](https://www.nexusmods.com/skyrimspecialedition/mods/21085)
		- Players can hover their crosshairs over a container to trigger and use the QuickLoot menu.
	- [Skyrim Souls RE](https://www.nexusmods.com/skyrimspecialedition/mods/27859)
		- Unpause most menus, drastically improving the flow of co-op, since other players that are not in control of menus can still move and interact with certain objects. This mod has been developed, for the most part, with Skyrim Souls active. ***Highly recommended***.
		- ***Important***: To prevent a companion player's inventory (Container Menu) from closing when they are far away from player 1, open up the configuration file located locally at `SKSE/Plugins/SkyrimSoulsRE.ini` and change `bAutoCloseMenus` to ***false*** or adjust the other auto-close settings to your liking.
	- [SSE Engine Fixes](https://www.nexusmods.com/skyrimspecialedition/mods/17230)
		- Fixes many bugs present in vanilla Skyrim. ***Highly recommended***.
	- [Super Fast Get Up Animation](https://www.nexusmods.com/skyrimspecialedition/mods/46714)
		- Regain control of your player characters faster after ragdolling. ***Highly recommended***.
- If you are playing Skyrim, ***activate*** `ALYSLC.esp` and ***delete or deactivate*** `ALYSLC Enderal.esp` in your mod manager.
- If you are playing Enderal, ***activate*** `ALYSLC Enderal.esp` and ***delete or deactivate*** `ALYSLC.esp` in your mod manager.
- ***Run LOOT or modify your plugin load order manually***. Ensure ALYSLC's .esp files are placed near the bottom of the load order.
- ***Run Nemesis or Pandora***. Make sure the `Precision` checkbox is ticked at the minimum before running either engine.
#### Example Mod Lists (AE/SE) In MO2
##### Anniversary Edition (1.6.1170)
![](https://i.imgur.com/OhuoUgJ.png)
##### Special Edition (1.5.97)

![](https://i.imgur.com/cYwQo8m.png)
### Known Issues and Tips and Tricks
Certain systems were built to work around player 1 exclusivity or around restrictions that the game places on NPCs. Using the ***debug binds (see the mod's MCM for details)*** and additional options in the ***Debug Menu*** (opened by holding the `Start` button and then pressing the `Back` button by default) is highly recommended for fixing issues that arise during gameplay.

#### The following features are more likely to bug out at times and require user intervention to fix:
- Equipping/unequipping gear and companion player equipment selection.
    - Player can lock up or have the wrong gear equipped. The game will periodically force-equip what gear it thinks is best. Until a proper solution is found, the only recourse is to manually re-equip the unequipped items.
    - The Favorites and Magic Menu selected entries reset when a companion player equips an item and must be refreshed by moving one category over and back again, which will reset the selected entry. Still looking for a seamless solution.
    - Ammo must be equipped 1 unit at a time for companion players to avoid creation of new inventory entries each time the ammo is equipped and to avoid issues with unequipping a large quantity of ammo at once.
	- Equipped bound weapons will sometimes bug out, become invisible, or get unequipped by the game when picking up an object of the same type from the overworld. Must sheathe, draw, and cast the spell again to re-equip the weapon.
- Player revive system.
    - 'All or nothing' system to incentivize team play: if one player dies, all players die.
    - Refrain from saving while a player is downed.
    - The game may sometimes consider player 1 as dead after they are downed, which can bug out certain spells (such as Vigilant's weapon arts) or interactions (such as interacting with Myrad Keepers in Enderal). Must load a save made before the bug appeared to fix.
- Menu control assignment.
    - Since the mod cannot always determine the triggering source of certain menus, such as Message Box menus triggered externally by scripts, the wrong player can gain control of menus at times. A colored border that matches the menu-controlling player's crosshair color is drawn around the screen to indicate which player is in charge of menus.
    - Refrain from opening load doors, fast-traveling, or saving while menus are open, especially if a companion player is controlling menus. Data from the companion player (ex. name, race name, skill levels, or inventory) may still be copied to player 1 when the game saves, which can only be undone by reloading an older save.
- Spellcasting for companion players:
	- Left and right hand casting can feel 'sticky' or unresponsive at times. Will require sheathing and drawing the spells and potentially trying some of the debug options to reset the hand casters.
	- Moving further away from the camera can lead to a slower casting charge speed and less responsive casting overall. Haven't found the cause for this yet.
	- Fire-and-forget spellcasts can fail if a bound weapon is equipped in the other hand.
	
#### The following systems are not fully implemented or have some issues:
- Crosshair magnetism:
	- Certain objects' apparent widths/heights on the screen may not match the game's reported bounding box dimensions.
	- The in-place rudimentary bounding box approximation leads to situations where the object's true width/height is much lower or higher than its corresponding calculated dimension, and moving the crosshair across the object is a lot faster or slower than it should be.
	- Will look to cover edge cases eventually, such as oddly shaped activators or objects rotated significantly with respect to their default orientation.
- Crosshair selection:
	- Certain objects are not targetable with the player crosshairs and will require player 1 to switch back to the default camera and select the object.
		- Examples include tankards and some plates, or any small objects within the collision box of a larger object.
	- Certain objects are targetable with player crosshairs even though they probably shouldn't be. While the co-op camera is active, the game's line of sight check originates from the camera position, which can move into unreachable areas if camera collisions are off, and thus, on its own, can result in activation attempts of objects that should not be targetable.
		- Example: Items within reach distance but also visually blocked from access by a wall or another obstacle.
- Player Progression:
	- Some disparity in XP triggers between player 1 and other players.
	- Player 1's progression system remains untouched.
	- Companion players gain XP for a school of magic by expending magicka (even when magicka cost is adjusted or removed), healing an ally or themselves with Restoration magic, or by doing damage with Destruction magic. XP gain scales off the base magicka cost for spells, which is **not** the displayed cost in the Magic Menu.
	- No additional XP gain for spells that have unique conditions in the vanilla game. Some examples (on the long term to-do list):
		- Magelight awarding XP based on the distance the projectile travels before collision.
		- Receiving XP when summons deal damage in combat.
		- Detect Life scaling its granted XP based on the number of detected targets.
	- Otherwise, XP gain should be nearly identical for player 1 and other players. Let me know if there are other discrepancies or special cases.
- Projectile trajectory adjustments:
    - Predictive projectiles are not accurate when targeting fast-moving targets, such as dragons, because angular momentum is not taken into account as of now and NPC targets accelerate erratically. Consider switching to homing projectiles instead.
    - Certain enemies are difficult to hit, such as mudcrabs, because ~~they are the strongest beings in Tamriel~~ some of their model's targetable nodes protrude from their collision capsule and do not collide with projectiles.
    - Beam projectiles, such as Lightning Bolt, fail to hit targets at times, especially when fired along sloped terrain or pitched upward or downward too much.
- Spellcasting for companion players:
	- Not every spell will work properly for companion players as they do for player 1. Example: ```Raise Zombie``` glitches out upon re-animating the targeted corpse.
	- Some spells only apply effects if the target or caster is player 1. Example: ```Vigilant's weapon art powers```.
- Staff usage for companion players:
    - No animations when casting.
    - Enchantment charging not implemented.
- Torso and arm node adjustments:
    - Motion can spazz out at times.
    - Node positions do not match their associated collision capsules when spinal rotation is modified, which can lead to inaccurate collisions. Looking into a fix on my end.
    - Attacking a target when looking straight up or down leads to some pretty odd looking animations at times.
    - Archery aim direction may not perfectly match the release direction of arrows/bolts.

#### The following systems do not support fundamental or sweeping changes from other mods:
- Player progression:
    - By default, per-player progression only supports 1 perk point gained and a health/magicka/stamina levelup per level. Any mod which changes these allotted points on level up will likely break the per-player progression system of this mod. An MCM option is provided to change the number of perk points awarded per level up while in co-op.
    - Player 1-specific sources of XP are not synchronized with other players.
    - Not all perks in Skyrim's perk tree will apply to other players when unlocked. Example: the "Eagle Eye" Archery perk.
    - Vampire/werewolf transformation perk trees only apply to player 1 and not all powers are supported yet for companion players.
    - Any custom skill trees will likely only work for P1.
- Player 1's controlmap and the camera.
    - This mod has its own binds system to ensure that all players are working with the same base controls while the co-op camera is active. In order to trigger some player 1-exclusive events with companion players, player 1 input events are emulated and sent out to the game's control handlers, as if player 1 were the source of those inputs. Any mod which remaps or disables controls could cause issues with this system. Thus, ***using the default controlmap if you are playing co-op is essential***.
    - Also, playing without the co-op camera active is an option, but it hasn't been extensively tested. Certain binds, such as the 'Revive' bind, will not work for player 1, and the crosshair text will not update for companion players unless the co-op camera is active.

#### Common Troubleshooting Tips
- The game forcibly equips the gear it views as best-in-slot onto NPCs at certain times (example: when an item is added or equipped). This mod has a workaround in place to validate the equip state for companion players, but sometimes, players may still have their desired items unequipped or their character may begin stuttering when moving/attacking. If this happens, try using the ***'Debug: Re-equip Items in Hands'***, ***'Debug: Refresh Player Managers'***, or ***'Debug: Reset Player'*** binds and see if it corrects the issue. If the issue persists, try the ***'Reset Player' Debug Menu player option***.
- If the Summoning or Debug menus fail to open or if a player's inputs aren't recognized, ***try tabbing out and then tabbing back in***. This will sometimes happen when the game doesn't have focus. And if the Wait or Pause/Journal menus are opening instead, ALYSLC's .dll may not have been loaded properly by SKSE. A restart should fix the problem.
- If the physics system has bugged out and the player is frozen, warped, or otherwise unresponsive, attempt to reset the player with either ***'Debug: Ragdoll Player'*** or ***'Debug: Reset Player'***. Or flop. Flopping solves a lot of problems from my experience.
- If a companion player is controlling menus when they shouldn’t be, or the option to quicksave or save is greyed out even though no players are downed, opening the ***Debug Menu*** with any player and selecting ***'Stop Menu Input Manager'*** may resolve the issue.
- If the player crosshairs or the menu control outline is not showing, ***pause and unpause the game***, which should force the mod's overlay menu to open and fix the problem.
- If a follower or player ally is still aggroed towards a player, ***try sheathing all players' weapons or using the Debug Menu's 'Stop Combat' option***.
- If the player's character is rotating on their own or not facing the right way, re-equip the items in their hands with '***Debug: Re-equip Items In Hands***', or re-equip the items manually. Then, sheathe/unsheathe the player's weapons. Can also press the '***Debug: Reset Player***' bind if the automatic rotation persists.
- If the player is trying to use furniture but is just standing around in a complete daze, the pathing issue will likely resolve on its own after a while, but to break them out of their confusion instantly, either ***jump or press the 'Sneak' bind***.
- If attempting to interact with a nearby object by holding the 'Activate' bind, and the **"[Object] is not within player's line of sight"** crosshair notification message displays, ***try selecting the object with the player crosshair instead***.
- If a certain interaction is still not working for any player, ***switch back to the default third person camera and attempt the interaction again with player 1***.
- If a companion player is locked in an idle animation, or isn't responding to movement inputs, try ***jumping***, ***ragdolling***, or pressing the ***'Debug: Reset Player'*** bind.
- Finally, if all else fails, ***give the 'Reset Player' Debug Menu player option a try, reload a save, or relaunch the game***. Please inform me of any issues that are not resolved even after performing these last-ditch measures.

### Mod Compatiblity Notes
#### Important General Compatibility Note
Mods which ***exclusively apply their changes to player 1 will NOT be fully compatible*** with this mod, since their changes will not apply to other summoned companion players.
Examples of such mods include:
- Mods that add new spells with attached scripts that check if the effect source or target is player 1 before applying the effect.
- Mods that provide player 1-exclusive animations.
- Mods that change the death system for player 1.
- Mods that add new keybinds.
- Mods that modify the camera will have no effect at best or contribute to crashes at worst.
- Mods that introduce new, player 1-centric mechanics.
A compatibility patch would be required in the future if I have the time to deploy both a Papyrus scripting framework and an SKSE plugin interface.  

#### Shortlist of Tested Incompatible Mods
Flagged as present in AE and/or SE.  
Degrees of incompatibility:
1. `LIGHT`: Game will still run and only occasional issues will arise.
2. `MEDIUM`: Game will still run, but blocks/modifies some of ALYSLC's core functionality.
3. `HEAVY`: Game will not run or a significant amount of ALYSLC's functionality becomes broken when it is installed.

- `[SE/AE] MEDIUM`: [Better Third Person Selection - BTPS](https://www.nexusmods.com/skyrimspecialedition/mods/64339)
   - Since ALYSLC has its own object targeting system per player for interacting with objects, the BTPS widget will not highlight the currently-targeted object properly and other issues can arise. 
- `[SE/AE] LIGHT`: [Dragon's Eye Minimap](https://www.nexusmods.com/skyrimspecialedition/mods/135489)
	- Tween and Stats menus open and then immediately close ***sometimes*** when any player attempts to open them while in co-op. Looking into the issue, which is probably on my end.
- `[SE/AE] MEDIUM`: [Proteus](https://www.nexusmods.com/skyrimspecialedition/mods/62934)
	- Can set player 1's name to the name of a previous player 1 character when reloading a save. Source of the bug is likely some incompatibility with how ALYSLC stores player names. Fix TBD and you may not encounter this bug, but if it does occur, disabling Proteus for now should resolve the issue.
	- If editing a companion player's appearance, make sure to set their race through ALYSLC's Summoning Menu first before further modifying the player's appearance with Proteus's NPC editor options.
- `[SE/AE] MEDIUM`: [UltimateCombat](https://www.nexusmods.com/skyrimspecialedition/mods/17196)
   - An essential hook for preventing certain animations from playing on player 1 and companion players is not functioning while Ultimate Combat is enabled. Seems to be an issue involving Ultimate Combat's propagation of the original hooked function, as ALYSLC's hook never runs. Manifests as downed players immediately getting up and running in place instead of staying down. Likely other animation-event related issues as well, but haven't thoroughly tested yet. Disable Ultimate Combat if using ALYSLC's revive system until I find a workaround.
 
## [Developer's Note]
I started developing this mod in January of 2021 without ever making a mod or coding a personal project in C++ and with barely any programming knowledge at all. Over the intervening time period, I've clocked in over 10k hours and ***have decided to take a break from active development, primarily due to health issues***. So for the time being, I hope that the ample (excessive) amount of documentation spread throughout the codebase will provide you with my reasoning for certain design decisions and paint a clearer picture of what I was trying to achieve. There are definitely a lot of workarounds, hacky solutions, feature creep, and bugs, but that's to be expected (at least some of it) when implementing local multiplayer in a purely singleplayer game. I hope to someday come back and improve upon the code through a large scale refactor, but in the meantime, feel free to contribute and ask questions. I'll try to answer as many of them as I can. And please let me know if I've made any obvious oversights; I've re-implemented the core features of this mod in more ways than I can remember since early 2021, so there's bound to be some remnants of early, unpolished code that require removal.  

And with that being said, I really hope you enjoy the mod and thanks for reading!

## [Credits]
See the mod's source for more detailed credits.
- `Moopus1`
   - [Couch Co-Op Skyrim](https://www.nexusmods.com/skyrim/mods/72743)
   - Served as the original local co-op mod idea and laid out foundational work on adding a controllable NPC to the game. 
- `The SKSE Dev Team`
   - [SKSE](https://skse.silverlock.org/)
   - Thank you for allowing modders to truly push the boundaries of modding.
- `Ryan`
   - [CommonLibSSE](https://github.com/Ryan-rsm-McKenzie/CommonLibSSE)
   - Goes without saying that this mod would not exist, in concept or reality, without CommonLib’s additional library resources on top of SKSE.
   - [Check out Ryan's Skyrim mods](https://next.nexusmods.com/profile/Fudgyduff/mods?gameId=1704)
- `powerOf3`
   - For their [CommonLibSSE fork](https://github.com/powerof3/CommonLibSSE), which is used as a base for this mod’s plugin.
   - For their open-sourced plugins which served as an invaluable resource for learning to create plugins and for a number of hooks.
   - [Check out powerOf3's Skyrim mods](https://next.nexusmods.com/profile/powerofthree/mods?gameId=1704)
- `Ershin`
   - For their revolutionary mods [True Directional Movement](https://www.nexusmods.com/skyrimspecialedition/mods/51614), [TrueHUD](https://www.nexusmods.com/skyrimspecialedition/mods/62775), and [Precision](https://www.nexusmods.com/skyrimspecialedition/mods/72347), which I referenced heavily to understand how to structure my plugin and for a number of hooks.
   - [Check out Ershin's Skyrim mods](https://next.nexusmods.com/profile/Ershin/mods?gameId=1704)
- `Parapets`
   - [Face Discoloration Fix](https://www.nexusmods.com/skyrimspecialedition/mods/42441)
   - Squashes the “dark face bug” afflicting NPCs who have had their appearance changed without regenerating facegen data. The fix allows appearance presets and race swapping to work properly through ALYSLC’s character customization menu.
   - [MCM Helper](https://www.nexusmods.com/skyrimspecialedition/mods/53000)
   - Provided an extremely easy-to-use framework for setting up this mod's MCM.
   - And also for their open-sourced SKSE plugins.
   - [Check out Parapet's Skyrim mods](https://next.nexusmods.com/profile/Parapets/mods?gameId=1704)
- `FlyingParticle`
  - For groundbreaking Havok reverse engineering in their [HIGGS - Enhanced VR Interaction](https://www.nexusmods.com/skyrimspecialedition/mods/43930) and [PLANCK - Physical Animation and Character Kinetics](https://www.nexusmods.com/skyrimspecialedition/mods/66025) VR mods, from which a couple of functions were ID'd for use in moving around NPCs in SE/AE.
  - [Check out FlyingParticle's Skyrim VR mods](https://next.nexusmods.com/profile/FlyingParticle/mods?gameId=1704)
- `dTry`
  - For their revolutionary combat mod [Valhalla Combat](https://www.nexusmods.com/skyrimspecialedition/mods/64741), from which a melee hit hook was obtained.
  - [Check out dTry's Skyrim mods](https://next.nexusmods.com/profile/dTry/mods?gameId=1704)
- `doodlum`
   -  For the po3 CommonLibSSE project template linked on the Skyrim RE discord, which helped me set up a build environment for coding the early versions of this mod.
   -  [Check out doodlum's Skyrim mods](https://next.nexusmods.com/profile/doodlum/mods?gameId=1704)
- `Loki`
  -  For the outstanding mod [Skyrim's Paraglider](https://www.nexusmods.com/skyrimspecialedition/mods/53256), which was so much fun to use that I had to directly provide partial compatibility for companion players (must install Skyrim's Paraglider to unlock 'magical paragliding' for players 2-4).
  -  [Check out Loki's Skyrim mods](https://next.nexusmods.com/profile/0x4Ch0x4Fh0x4Bh0x49h/mods?gameId=1704)
- `mwilsnd`
   - For [SmoothCam](https://www.nexusmods.com/skyrimspecialedition/mods/41252), which was used as a reference for adjusting the camera’s orientation and for raycasting code.
   - [Check out mwilsnd's Skyrim mods](https://next.nexusmods.com/profile/mwilsnd/mods?gameId=1704)
- `Shrimperator`
   - For the Scaleform drawing code that was adapted from their mod [Better Third Person Selection](https://www.nexusmods.com/skyrimspecialedition/mods/64339).
   - [Check out Shrimperator's Skyrim mods](https://next.nexusmods.com/profile/Shrimperator/mods?gameId=1704)
- `maxsu2017`
   - For their [Detection Meter](https://www.nexusmods.com/skyrimspecialedition/mods/63057) mod, from which some detection level code and stealth points calculations were copied.
   - [Check out maxsu2017's Skyrim mods](https://next.nexusmods.com/profile/maxsu2017/mods?gameId=1704)
- `exiledviper` and `meh321`
   - For [PapyrusUtil SE](https://www.nexusmods.com/skyrimspecialedition/mods/13048), a vital component of this mod’s ability to maintain state between saves and also share data among scripts.
   - [Check out exiledviper's Skyrim mods](https://next.nexusmods.com/profile/exiledviper/mods?gameId=1704)
   - [Check out meh321's Skyrim mods](https://next.nexusmods.com/profile/meh321/mods?gameId=1704)
- `Expired6978`
   - For [UIExtensions](https://www.nexusmods.com/skyrimspecialedition/mods/17561), which provides the backbone for co-op player customization and a number of co-op menus.
   - [Check out Expired6978's Skyrim mods](https://next.nexusmods.com/profile/expired6978/mods?gameId=1704)
- `covey-j`
   - For their work on [Actor Copy Lib](https://github.com/covey-j/ActorCopyLib), a snippet of which is used to copy over actors’ appearances to companion players.
- `digitalApple`
   - For their [Explosion Collision Fix](https://www.nexusmods.com/skyrimspecialedition/mods/154076) mod, from which a hook was obtained for preventing friendly-fire damage from projectile explosions.
   - [Check out digitalApple's Skyrim mods](https://next.nexusmods.com/profile/digitalApple/mods?gameId=1704)
- `fenix31415`
	- For the method used in allowing grabbed projectiles to hit their original shooters when thrown back at them and for their extensive work on projectile-related code.
	- [Check out fenix31415's Skyrim Mods](https://next.nexusmods.com/profile/fenix31415/mods?gameId=1704)
- `VersuchDrei`
  - For a reversed actor movement function used to stop players from moving, found here: [OStim GitHub Source](https://github.com/VersuchDrei/OStimNG/blob/main/skse/src/GameAPI/GameActor.h).
  - [Check out VersuchDrei's Skyrim mods](https://next.nexusmods.com/profile/VersuchDrei/mods?gameId=1704)
- `TrashQuixote` 
  - For the function IDs for constructing and applying hit data, which are from their mod [TrashUtility](https://github.com/TrashQuixote/TrashUtility).
  - [Check out TrashQuixote's Skyrim Mods](https://next.nexusmods.com/profile/LowbeeBob?gameId=1704)
- `PJM Homebrew Fonts`
  - For the [font](https://www.fontspace.com/balgruf-font-f59539) (SIL Open Font License (OFL)) used in the banner image. 
- `SanneARBY`
  - For the extracted [Skyrim UI elements](https://www.nexusmods.com/skyrimspecialedition/mods/82169) used on ALYSLC's GitHub and Nexus pages.
  - [Check out SanneARBY's Skyrim mods](https://next.nexusmods.com/profile/SanneARBY?gameId=1704)
-  A ton of users on the Skyrim RE Discord: ***po3, meh321, Nukem, aers, KernalsEgg, CharmedBaryon, Loki, Parapets, fireundubh, Fenix31415, Ultra, Qudix, NoahBoddie, dTry, Shrimperator, Bingle, Atom, alandtse, MaxSu2019, Sylennus, and many more***.
   -   Thank you for helping a programming and C++ greenhorn get their bearings with CommonLibSSE.
- **Everyone who open-sources their SKSE plugin code on GitHub.**  
	- Thank you for providing a torch to light the way for all newbie plugin developers!
## [License]
[GPL V3](https://github.com/Joessarian/Adventurers-Like-You-Skyrim-Local-Co-op/blob/main/LICENSE)

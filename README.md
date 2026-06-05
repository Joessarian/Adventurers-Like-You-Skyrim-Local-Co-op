

# Adventurers Like You: Skyrim Local Co-op

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
### [Nexus Modpage](https://www.nexusmods.com/skyrimspecialedition/mods/156493)


## [Developer's Note]
I started developing this mod in January of 2021 without ever making a mod or coding a personal project in C++ and with barely any programming knowledge at all. Over the intervening time period, I've clocked in over 10k hours and ***have decided to take a step away from routine, active development, primarily due to health issues***. So for the time being, I hope that the ample (excessive) amount of documentation spread throughout the codebase will provide you with my reasoning for certain design decisions and paint a clearer picture of what I was trying to achieve. There are definitely a lot of workarounds, hacky solutions, feature creep, and bugs, but that's to be expected (at least some of it) when implementing local multiplayer in a purely singleplayer game. I hope to someday come back and improve upon the code through a large scale refactor, but in the meantime, feel free to contribute and ask questions. I'll try to answer as many of them as I can. And please let me know if I've made any obvious oversights; I've re-implemented the core features of this mod in more ways than I can remember since early 2021, so there's bound to be some remnants of early, unpolished code that require removal.  


With that being said, fellow adventurer, I really hope you enjoy the mod and thanks for reading!

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
   - For [Use Or Take](https://www.nexusmods.com/skyrimspecialedition/mods/70868) which was referenced to provide compatibility with ALYSLC, and is also a requirement to have the same P1 activation functionality for P2-P4.
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
- `Corsafire1`
  - For the havok game settings tweak to mitigate incidental physics damage from objects: [No Silly Physics Damage - Carts Pots Bones etc](https://www.nexusmods.com/skyrimspecialedition/mods/36132).
  - [Check out Corsafire1's Skyrim Mods](https://www.nexusmods.com/profile/Corsafire1/mods?gameId=1704)
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

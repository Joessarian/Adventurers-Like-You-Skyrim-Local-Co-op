
# Adventurers Like You: Skyrim Local Co-op ALPHA

![Banner](https://i.imgur.com/s0tX9uh.jpeg)
## [For Users]
### Getting Started and Important Notes
- First and foremost, ***expect a decent amount of jankiness/clunkiness***, simply by virtue of having to hack in controllable NPCs to use as players and from attempting to apply P1-exclusive features and player agency to these companion players. I've tried to make the mod's mechanics and features as modular as possible, with mod compatibility in mind, and will continue to offer additional customizability in the future, so check out the mod's MCM if there's a feature you'd like to adjust or disable entirely.
- The mod is meant to be played locally and ***is NOT a fully-fledged multiplayer experience*** like the incredible [Skyrim Together Reborn](https://www.nexusmods.com/skyrimspecialedition/mods/69993) mod. However, online play is still possible via leveraging Steam's [Remote Play Together](https://store.steampowered.com/remoteplay) with [Remote Play Whatever](https://github.com/m4dEngi/RemotePlayWhatever) or via streaming with [Parsec](https://parsec.app/). Your mileage may vary.
- This mod requires that ***at least two XInput-compatible controllers*** are plugged into your computer before starting a co-op session.
- Almost all of the mod's testing was done on ***Skyrim version 1.5.97***, so for the most stable experience, your best bet is to downgrade your Skyrim installation to this version.
- If you are playing ***Skyrim***, enable 'ALYSLC.esp' and disable 'ALYSLC Enderal.esp' in your load order. Otherwise, if you are playing ***Enderal***, enable 'ALYSLC Enderal.esp' and disable 'ALYSLC.esp'.
- After installing the mod for the first time, always ***start a new playthrough and dedicate the playthrough to co-op***. Minimize progressing player 1's character outside of co-op.
- As of this time, ***removing the mod mid-playthrough is not supported*** and will lead to freezes when loading saves.
- To summon other players, after loading a save, first ***ensure player 1 is not in combat***. Then hold the `Back` button and press the `Start` button on ***player 1's*** controller. This will establish which player is recognized as player 1 by the mod and open the Summoning Menu.
- Have a peek at the mod's ***MCM*** once it fully loads to ***learn about and customize the mod's extensive settings***.  Settings include:
  - Camera options
  - Cheats/debug options
  - Character and balancing options for all players
  - Emote idle assignment
  - Menu control options
  - Movement speed/rotation options for all players
  - Player action options for all players
  - Progression options
  - Unique controls customization for each player
  - User interface options
- ***Save frequently*** while playing.
- ***Stick together*** when possible to ensure all players are on-screen and easy to see. There are also some options to improve player visibility, such as player indicators, camera object fade options, and player-focal camera positioning when players are far apart. And finally, if seeing "the void" outside of the traversable map does not bother you, disabling camera collisions altogether provides the smoothest experience.
- If a certain interaction is not triggering for any player, ***switch back to the default third person camera and attempt the interaction again with player 1***.
- ***Bugs are inevitable, and in some cases, correctable***. Before reporting an issue, please use the ***debug binds*** or ***Debug Menu options*** to troubleshoot issues as they arise. If the bug recurs frequently even after using said options, please include what debug options you've used and/or a crash log if you've also installed a crash logger.

### Performance
- Expect a ***loss of ~4-5 FPS if the game regularly runs at 60 FPS without the mod***.
- Highly recommend ***capping your framerate to 60 FPS if possible***, since some of the physics-related code in this mod can act up at higher framerates.
- As the project, and my programming skills, are still very much in alpha, expect performance optimizations down the line once the mod becomes more stable.

### Prerequisite Mods
Install the following mods + all their prerequisites:
- [SKSE64](https://skse.silverlock.org/)
- [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604)
- [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [Face Discoloration Fix](https://www.nexusmods.com/skyrimspecialedition/mods/42441)
- [MCM Helper](https://www.nexusmods.com/skyrimspecialedition/mods/53000)
- [PapyrusExtender](https://www.nexusmods.com/skyrimspecialedition/mods/22854)
- [PapyrusUtil SE](https://www.nexusmods.com/skyrimspecialedition/mods/13048)
- [Precision](https://www.nexusmods.com/skyrimspecialedition/mods/72347)
- [UIExtensions](https://www.nexusmods.com/skyrimspecialedition/mods/17561)
   - Note: Already comes packaged with Enderal, so no need to install separately unless you’re only playing Skyrim.

### Highly Recommended Mods (Not Required)
Additional mods which are highly recommended, but not required for this mod to function:
- [Alternate Start](https://www.nexusmods.com/skyrimspecialedition/mods/272)
  - Allows players to skip everyone’s favorite intro sequence and to summon companion players right away.
  - NOTE: Summoning other players before choosing to follow Hadvar or Ralof causes a slew of bugs that must be fixed with console commands and is probably not worth the effort.
- [Auto Input Switch](https://www.nexusmods.com/skyrimspecialedition/mods/54309)
   - Seamlessly switch between controller and keyboard + mouse inputs.
- [Detection Meter SE](https://www.nexusmods.com/skyrimspecialedition/mods/63057), [Detection Meter AE](https://www.nexusmods.com/skyrimspecialedition/mods/77350)
   - Properly show detection levels for player 1 while sneaking when the co-op camera is active.
- [Party Combat Parameters (SE Only)](https://www.nexusmods.com/skyrimspecialedition/mods/57127)
   - Shows Health/Magicka/Stamina and more useful stats for companion players.
- [Proteus](https://www.nexusmods.com/skyrimspecialedition/mods/62934)
   - Companion players can further customize their characters to their liking through the NPC editor suite.
- [Quick Loot RE/EE](https://www.nexusmods.com/skyrimspecialedition/mods/21085)
    - Players can hover their crosshairs over a container to trigger and use the QuickLoot menu.
- [Skyrim Souls RE](https://www.nexusmods.com/skyrimspecialedition/mods/27859)
   - Unpause most menus, drastically improving the flow of co-op, since other players that are not in control of menus can still move and interact with certain objects. This mod has been developed, for the most part, with Skyrim Souls active.
- [SSE Engine Fixes](https://www.nexusmods.com/skyrimspecialedition/mods/17230)
   - Fixes many bugs present in vanilla Skyrim.
- [Super Fast Get Up Animation](https://www.nexusmods.com/skyrimspecialedition/mods/46714)
   - Regain control of your player characters faster after ragdolling.
- [TrueHUD](https://www.nexusmods.com/skyrimspecialedition/mods/62775) 
   - Displays floating health bars, boss health bars, and dynamic UI updates, all while being fully customizable.

### Known Issues and Tips and Tricks
Certain systems were built to work around player 1-exclusivity or around restrictions that the game places on NPCs. Using the ***debug binds (see the mod's MCM for details)*** and additional options in the ***Debug Menu*** (opened by holding the `Start` button and then pressing the `Back` button by default) is highly recommended for fixing issues that arise during gameplay.

#### The following features are more likely to bug out at times and require user intervention to fix:
- Equipping/unequipping gear and player equipment selection.
    - Player can lock up or have the wrong gear equipped.
    - The Favorites and Magic Menu selected entries reset when a companion player equips an item.
    - Ammo must be equipped 1 unit at a time for companion players to avoid creation of new inventory entries each time the ammo is equipped and to avoid issues with unequipping a large quantity of ammo at once.
- Player revive system.
    - 'All or nothing' system to incentivize team play: if one player dies, all players die.
    - Refrain from saving while a player is downed.
    - The game may sometimes consider P1 as dead after they are downed, which can bug out certain spells (such as Vigilant's weapon arts) or interactions (such as interacting with Myrad Keepers in Enderal). Must load a save made before the bug appeared to fix.
- Menu control assignment.
    - Since the mod cannot always determine the triggering source of certain menus, such as Message Box menus triggered externally by scripts, the wrong player can gain control of menus at times. A colored border that matches the menu-controlling player's crosshair color is drawn around the screen to indicate which player is in charge of menus.
    - Refrain from opening load doors, fast-traveling, or saving while menus are open, especially if a companion player is controlling menus. Data from the companion player (ex. name, race name, skill levels, or inventory) may still be copied to P1 when the game saves, which can only be undone by reloading an older save.

#### The following systems are not fully implemented or have some issues:
- Projectile trajectory adjustments:
    - Predictive projectiles are not accurate when targeting fast-moving targets, such as dragons, because angular momentum is not taken into account as of now and NPC targets accelerate erratically. Consider switching to homing projectiles instead.
    - Certain enemies are difficult to hit, such as mudcrabs, because ~~they are the strongest beings in Tamriel~~ some of their model's targetable nodes protrude from their collision capsule and do not collide with projectiles.
    - Beam projectiles, such as Lightning Bolt, fail to hit targets more often than other types of projectiles.
- Staff usage for companion players:
    - No animations when casting.
    - Enchantment charging not implemented.
- Torso and arm node adjustments:
    - Blending only implemented for torso nodes.
    - Collisions are not always accurate when spinal rotation is modified.
    - Attacking a target when looking straight up or down leads to some pretty odd looking animations.

#### The following systems do not support fundamental or sweeping changes from other mods:
- Player progression:
    - Per-player progression only supports 1 perk point gained and 1 health/magicka/stamina levelup per level. Any mod which changes these points allotted on level up will likely break the per-player progression system of this mod.
    - P1-specific sources of XP are not synchronized with other players.
    - Not all perks in Skyrim's perk tree will apply to other players when unlocked. Example: the "Eagle Eye" Archery perk.
    - Vampire/werewolf transformation perk trees only apply to P1 and not all powers are supported yet for companion players.
- Player 1's controlmap and the camera.
    - This mod has its own binds system to ensure that all players are working with the same base controls ***while the co-op camera is active***. However, in order to trigger some P1-exclusive events with companion players, P1 input events are emulated and sent out to the game's control handlers, as if P1 were the source of those inputs. Any mod which remaps or disables controls could cause issues with this system. In addition, using the default controlmap if you are playing co-op is highly recommended.
    - Also, playing without the co-op camera active is an option, but it hasn't been extensively tested, and certain binds, such as the 'Revive' bind will not work for P1 unless the co-op camera is active.

#### Common Troubleshooting Tips
- The game forcibly equips the gear it views as best-in-slot onto NPCs at certain times (example: when an item is added or equipped). This mod has a workaround in place to validate the equip state for companion players, but sometimes, players may still have their desired items unequipped or their character  may begin stuttering when moving/attacking. If this happens, try using the ***'Debug: Re-equip Items in Hands'***, ***'Debug: Refresh Player Managers'***, or ***'Debug: Reset Player'*** binds and see if it corrects the issue. If the issue persists, try the ***'Reset Player' Debug Menu player option***.
- If the physics system has bugged out and the player is frozen, warped, or otherwise unresponsive, attempt to reset the player with either ***'Debug: Ragdoll Player'*** or ***'Debug: Reset Player'***. Or flop. Flopping solves a lot of problems from my experience.
- If a companion player is controlling menus when they shouldn't be, opening the ***Debug Menu*** with any player and selecting ***'Stop Menu Input Manager'*** may resolve the issue.
- If the player crosshairs or the menu control outline is not showing, ***pause and unpause the game***, which should force the mod's overlay menu to open and fix the problem.
- If a follower or player ally is still aggroed towards a player, ***try sheathing all players' weapons or using the Debug Menu's 'Stop Combat' option***.
- If the player's character is rotating on their own or not facing the right way, re-equip the items in their hands with ***'Debug: Re-equip Items In Hands'***, or re-equip the items manually. Then, sheathe/unsheathe the player's weapons.
- If the player is trying to use furniture but is just standing around in a complete daze, the pathing issue will likely resolve on its own after a while, but to break them out of their confusion instantly, either ***jump or press the 'Sneak' bind***.
- Finally, if all else fails, ***give the 'Reset Player' Debug Menu player option a try, reload a save, or relaunch the game***. Please inform me of any issues that are not resolved even after performing these last-ditch measures.

### Mod Compatiblity Notes
#### Important General Compatibility Note
Mods which ***exclusively apply their changes to player 1 will NOT be fully compatible*** with this mod, since their changes will not apply to other summoned companion players.
Examples of such mods include:
- Mods that add new spells with attached scripts that check if the effect target is player 1 before applying the effect.
- Mods that provide player exclusive animations.
- Mods that change the death system for player 1.
- Mods that add new keybinds.
- Mods that modify the camera will have no effect at best or contribute to crashes at worst.
- Mods that introduce new, player-centric mechanics.
A compatibility patch would be required in the future if I have the time to deploy both a Papyrus scripting framework and an SKSE plugin interface.  

#### Shortlist of Tested Incompatible Mods
Flagged as present in AE and/or SE.  
Degrees of incompatibility:
1. `LIGHT`: Game will still run and only occasional issues will arise.
2. `MEDIUM`: Game will still run, but blocks/modifies some of ALYSLC's core functionality.
3. `HEAVY`: Game will not run or a significant amount of ALYSLC's functionality becomes broken when it is installed.

- `[SE/AE] MEDIUM`: [Better Third Person Selection - BTPS](https://www.nexusmods.com/skyrimspecialedition/mods/64339)
   - Since ALYSLC has its own object targeting system per player for interacting with objects, the BTPS widget will not highlight the currently targeted object properly and other issues can arise. 
- `[SE/AE] HEAVY`: [Persistent Favorites](https://www.nexusmods.com/skyrimspecialedition/mods/118174)
   - Reproducible crash when opening the Favorites Menu a second time with a companion player. Possibly a conflict when ALYSLC adds an item to P1 to favorite it for a companion player that is opening the Favorites Menu. Only solution for now is to remove the mod while playing co-op.
- `[SE/AE] LIGHT`: [SmoothCam](https://www.nexusmods.com/skyrimspecialedition/mods/41252)
   - Having the mod active while using the co-op camera can lead to sporadic raycasting crashes. For now, until a solution is bound, uninstall/disable SmoothCam with a hotkey if using the co-op camera while playing co-op.
- `[AE] (Maybe) HEAVY` [True Directional Movement - Modernized Third Person Gameplay](https://www.nexusmods.com/skyrimspecialedition/mods/51614)
   - ***Might*** interfere with ALYSLC's system for player movement ONLY on AE. If player 1 is sliding around and not moving in the direction of the left stick, please uninstall TDM temporarily before playing co-op, at least until I find the issue on my end.
- `[SE/AE] HEAVY`: [UltimateCombat](https://www.nexusmods.com/skyrimspecialedition/mods/17196)
   - Essential hook for preventing certain animations from playing on player 1 and companion players is not functioning while Ultimate Combat is enabled. Seems to be an issue involving Ultimate Combat's propagation of the original hooked function, as ALYSLC's hook never runs. Manifests as downed players immediately getting up and running in place instead of staying down. Likely other animation-event related issues as well, but haven't thoroughly tested yet. Disable Ultimate Combat if using ALYSLC's revive system until I find a workaround.
 
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
- Open a command prompt in the cloned repository folder.
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
Open up the `ALYSLC_SE.sln` or `ALYSLC_AE.sln` inside your newly-generated `build` or `buildae` folder.
- Once opened in Visual Studio 2022, click on `Project` then `<ALYSLC_SE/ALYSLC_AE> Properties` and in the `Configuration Properties` pane, click on `Build Events`. Then choose `Pre-Build`, click on the `Command Line` entry, and type in:
  ```
  del /s /q $(TargetDir)*.pdb
  ```
- Click `Apply` and then `Ok`.
- Switch the active solution configuration from `Debug` to `Release`. - ***Note***: Building in debug currently does not work, so ensure the project's build configuration is set to release.
- If you wish to enable all debug prints (`ALYSLC::Log()` calls), uncomment ```#define ALYSLC_DEBUG``` in `PCH.h`. ***Always comment out the line before making a PR***.
- Build the solution with `Ctrl + Shift + B` or by clicking on `Build` and then `Build Solution`.
- If the build succeeded and you previously specified an output path for the compiled plugin, you’re all set to test out your changes in-game!
- Special notes on configuring for Skyrim/Enderal before launching the game:
   - If you are playing Enderal, delete or deactivate the `ALYSLC.esp` in your mod manager.
      - Modify the mod's settings via the `/Data/MCM/Settings/ALYSLC Enderal.ini` file.
   - If you are playing Skyrim, delete or deactivate the `ALYSLC Enderal.esp` in your mod manager.
      - Modify the mod's settings via the `/Data/MCM/Settings/ALYSLC.ini` file.

## Developer's Note
I started developing this mod in January of 2021 without ever coding a personal project in C++ and without much programming knowledge at all. The mission: craft a ***fun*** multiplayer experience with as few concessions as possible, allowing each player to feel as if they were player 1 and not just a controllable NPC (even though under the hood, that is essentially what companion players 2-4 are).

Over the intervening time period, I've probably clocked in close to 10k hours and ***have decided to take a break from active development, primarily due to health concerns***. So for the time being, I hope that the ample amount of documentation spread throughout the codebase will provide you with my reasoning for certain design decisions and paint a clearer picture of what I was trying to achieve. There are clearly a lot of workarounds, hacky solutions, feature creep, and bugs, but that's to be expected when implementing local multiplayer in a singleplayer game, especially with my complete lack of reverse-engineering knowledge and very limited programming knowledge in general. I hope to someday come back and improve upon the code through a large scale refactor, but in the meantime, feel free to contribute and ask questions. I'll try to answer as many of them as I can. And please let me know if I've made any obvious oversights; I've re-implemented the core features of this mod in more ways than I can remember since early 2021, so there's bound to be some remnants of early, unpolished code that require removal.  

And with that being said, I hope you enjoy the mod because what's better than experiencing the boundless magic of modded Skyrim?  
***Experiencing Skyrim with friends and family — with adventurers like you!***

## Credits
See the mod's source for more detailed credits.
- `Moopus1`
   - [Couch Co-Op Skyrim](https://www.nexusmods.com/skyrim/mods/72743)
   - Served as the original local co-op mod idea and laid out foundational work on adding a controllable npc to the game. 
- `The SKSE Dev Team`
   - [SKSE](https://skse.silverlock.org/)
   - Thank you for allowing modders to truly push the boundaries of modding over a decade.
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
  - For their revolutionary combat mod [Valhalla Combat](), from which a melee hit hook was obtained.
  - [Check out dTry's Skyrim mods](https://next.nexusmods.com/profile/dTry/mods?gameId=1704)
- `doodlum`
   -  For the po3 CommonLibSSE project template linked on the Skyrim RE discord, which helped me set up a build environment for coding the early versions of this mod.
   -  [Check out doodlum's Skyrim mods](https://next.nexusmods.com/profile/doodlum/mods?gameId=1704)
- `Loki`
  -  For the outstanding mod [Skyrim's Paraglider](https://www.nexusmods.com/skyrimspecialedition/mods/53256), which was so much fun to use that I had to directly provide partial compatibility for companion players (must install Skyrim's Paraglider to unlock 'magical paragliding' for players 2-4).
  -  [Check out Loki's Skyrim mods](https://next.nexusmods.com/profile/LokiCXXVIII/mods?gameId=1704)
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
   - [Check out Expired6978's Skyrim mods](https://next.nexusmods.com/profile/expired6978/mods?gameId=110)
- `bosn889`
  - For the actor unequip hook from their grip switch mod [Dynamic Grip](https://www.nexusmods.com/skyrimspecialedition/mods/110031), which is used to prevent companion players from auto-equipping gear.
  - [Check out bosn889's Skyrim mods](https://next.nexusmods.com/profile/bosn889/mods?gameId=1704)
- `covey-j`
  - For their work on [Actor Copy Lib](https://github.com/covey-j/ActorCopyLib), a snippet of which is used to copy over actors’ appearances to companion players.
- `Sylennus`
  - For a function used by their mod [Personalized Mannequins](https://www.nexusmods.com/skyrimspecialedition/mods/76386), which is used to set base actor flags.
  - [Check out Sylennus's Skyrim mods](https://next.nexusmods.com/profile/Sylennus/mods?gameId=1704)
- `VersuchDrei`
  - For a reversed actor movement function used to stop players from moving, found here: [OStim GitHub Source](https://github.com/VersuchDrei/OStimNG/blob/main/skse/src/GameAPI/GameActor.h).
  - [Check out VersuchDrei's Skyrim mods](https://next.nexusmods.com/profile/VersuchDrei/mods?gameId=1704)
-  A ton of users on the Skyrim RE Discord: ***po3, meh321, Nukem, aers, KernalsEgg, CharmedBaryon, Loki, Parapets, fireundubh, Fenix31415, Ultra, Qudix, NoahBoddie, dTry, Shrimperator, Bingle, Atom, alandtse, MaxSu2019, Sylennus, and many more***.
   - Thank you for helping a programming and C++ greenhorn get their bearings and for answering a ton of questions that I had during the development process.

## License
[GPL V3](https://github.com/Joessarian/Adventurers-Like-You-Skyrim-Local-Co-op/blob/main/LICENSE)



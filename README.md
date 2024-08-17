# Adventurers-Like-You-Skyrim-Local-Co-op
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
- Switch the active solution configuration from `Debug` to `Release`.
- Build the solution with `Ctrl + Shift + B` or by clicking on `Build` and then `Build Solution`.
- If the build succeeded and you previously specified an output path for the compiled plugin, you’re all set to test out your changes in-game!
- Special notes on configuring for Skyrim/Enderal before launching the game:
   - If you are playing Enderal, delete or deactivate the `ALYSLC.esp` in your mod manager.
      - Modify the mod's settings via the `/Data/MCM/Settings/ALYSLC Enderal.ini` file.
   - If you are playing Skyrim, delete or deactivate the `ALYSLC Enderal.esp` in your mod manager.
      - Modify the mod's settings via the `/Data/MCM/Settings/ALYSLC.ini` file.

## [For Users]
### Getting Started
- After installing the mod for the first time, always ***start a new playthrough and dedicate the playthrough to co-op***. Minimize progressing player 1's character outside of co-op.
- As of this time, ***removing the mod mid-playthrough is not recommended*** and will lead to freezes when loading saves.
- This mod requires that ***at least two XInput-compatible controllers*** are plugged in to your computer before starting a co-op session.
- To summon other players, after loading a save, first ensure player 1 is not in combat. Then press and hold the `Back` button + press the `Start` button on ***player 1's*** controller. This will establish which player is recognized as player 1 by the mod and open the Summoning Menu.

### Important Notes
- Almost all of the mod's testing was done on ***Skyrim version 1.5.97***, so for the most stable experience, your best bet is to downgrade your Skyrim installation to this version.
- Have a peek at the mod's MCM once it fully loads and customize the mod's settings. In particular, ***reference and modify each players' MCM 'Binds' page*** to get a feel for the controls.
- ***Save frequently*** while playing.
- ***Bugs are inevitable, and in some cases, correctable***. Before reporting an issue, please use the ***debug binds*** or ***Debug Menu options*** to troubleshoot issues as they arise. If the bug recurs frequently even after using said options, please include what debug options you've used and/or a crash log if you've also installed a crash logger.
- See [Mod Incompatibility](#mod-incompatibility) for details on SE/AE-specific mod concerns.

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
- [Skyrim Souls RE](https://www.nexusmods.com/skyrimspecialedition/mods/27859)
   - Unpause most menus, drastically improving the flow of co-op, since other players that are not in control of menus can still move and interact with certain objects.
- [SSE Engine Fixes](https://www.nexusmods.com/skyrimspecialedition/mods/17230)
   - Fixes many bugs present in vanilla Skyrim.
- [Super Fast Get Up Animation](https://www.nexusmods.com/skyrimspecialedition/mods/46714)
   - Regain control of your player characters faster after ragdolling.
- [TrueHUD](https://www.nexusmods.com/skyrimspecialedition/mods/62775) 
   - Displays floating health bars, boss health bars, and dynamic UI updates, all while being fully customizable.

### Mod Incompatibility
Flagged as present in AE and/or SE.  
Degrees of incompatibility:
1. `LIGHT`: Game will still run and only occasional issues will arise.
2. `MEDIUM`: Game will still run, but blocks/modifies some of ALYSLC's core functionality.
3. `HEAVY`: Game will not run or a significant amount of ALYSLC's functionality becomes broken when it is installed.

- `[SE/AE] MEDIUM`: [Better Third Person Selection - BTPS](https://www.nexusmods.com/skyrimspecialedition/mods/64339)
   - Since ALYSLC has its own object targeting system per player for interacting with objects, the BTPS widget will not highlight the currently targeted object properly and other issues can arise. 
- `[SE/AE] LIGHT`: [SmoothCam](https://www.nexusmods.com/skyrimspecialedition/mods/41252)
   - Having the mod active while using the co-op camera can lead to sporadic raycasting crashes. For now, until a solution is bound, uninstall/disable SmoothCam with a hotkey if using the co-op camera while playing co-op.
- `[AE] HEAVY` [True Directional Movement - Modernized Third Person Gameplay](https://www.nexusmods.com/skyrimspecialedition/mods/51614)
   - Interferes with ALYSLC's system for player movement ONLY on AE. Until I find the issue on my end, please uninstall TDM temporarily before playing co-op.

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



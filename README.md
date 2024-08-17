# Adventurers-Like-You-Skyrim-Local-Co-op
A Skyrim SE (1.5.97 and above) mod which allows up to 3 additional players to locally tag along with the Dragonborn in their adventures throughout Skyrim. And potentially beyond.

## [Build Steps and Tips]
### Setting Up the Build Environment and Dependencies.
- Set up Visual Studio 2022 and install desktop development with C++.
- This project uses po3’s fork of CommonLib. 
Ensure you have all of CommonLibSSE’s dependencies set up.
- Link to the fork’s main page:
   > https://github.com/powerof3/CommonLibSSE
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


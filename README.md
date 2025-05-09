<h1 align="center">
<a href="https://github.com/cdozdil/OptiScaler">
        <img src="https://github.com/user-attachments/assets/c7dad5da-0b29-4710-8a57-b58e4e407abd">
</h1>

<h1 align="center">
 <a href="https://discord.gg/2JDHx6kcXB">
        <img src="https://img.shields.io/badge/DLSS2FSR%20Discord-invite?logo=discord&logoColor=white&color=5865F2" width="155">
 <a href="https://github.com/cdozdil/OptiScaler/releases/latest">
        <img src="https://img.shields.io/badge/Download-Latest-green?logo=github&logoColor=white" width="150">
 <a href="https://github.com/cdozdil/OptiScaler/commits/master">
        <img src="https://img.shields.io/github/last-commit/cdozdil/OptiScaler/master" width="135">
 <a href="https://github.com/cdozdil/OptiScaler?tab=GPL-3.0-1-ov-file#readme">
        <img src="https://img.shields.io/github/license/cdozdil/OptiScaler" width="120">
 <a href="https://github.com/cdozdil/OptiScaler/wiki">
        <img src="https://img.shields.io/badge/Wiki-blue?logo=mdbook" width="60">
 <a href="https://github.com/cdozdil/OptiScaler/stargazers">
        <img src="https://img.shields.io/github/stars/cdozdil/OptiScaler" width="110">
</h1>




## Table of Contents

**1.** [**About**](#about)  
**2.** [**How it works?**](#how-it-works)  
**3.** [**Supported APIs and Upscalers**](#which-apis-and-upscalers-are-supported)  
**4.** [**Installation**](#installation)  
**5.** [**Known Issues**](#known-issues)  
**6.** [**Compilation and Credits**](#compilation)  
**7.** [**Wiki**](https://github.com/cdozdil/OptiScaler/wiki)

## About

**OptiScaler** is a tool that lets you replace upscalers in games that ***already support*** DLSS2+ / FSR2+ / XeSS, now also supports enabling frame generation in those same games (through OptiFG or Nukem's dlssg-to-fsr3).

While previously only DLSS2+ inputs were supported, newer versions also added support for XeSS and FSR2+ inputs (_with some caveats_$`^1`$). For example, if a game has DLSS only, the user can replace DLSS with XeSS or FSR 3.1 (same goes for an FSR or XeSS-only game). It also offers extensive customization options for all users, including those with Nvidia GPUs using DLSS.

**Key aspects of OptiScaler:**
- Enables usage of XeSS, FSR2, FSR3, **FSR4**$`^2`$ (_RDNA4 only_) and DLSS in upscaler-enabled games
- Allows users to fine-tune their upscaling experience with a wide range of tweaks and enhancements (RCAS & MAS, Output Scaling, DLSS Presets, Ratio & DRS Overrides etc.)
- Since v0.7.0+, added experimental DX12 frame generation support with possible HUDfix solution ([**OptiFG**](#optifg-powered-by-fsr3-fg--hudfix-experimental-hud-ghosting-fix) by FSR3)
- Supports [**Fakenvapi**](#fakenvapi) integration - enables Reflex hooking and injecting _Anti-Lag 2_ (RDNA1+ only) or _LatencyFlex_ (LFX) - **_not bundled_**$`^3`$  
- Since v0.7.7, support for Nukem's FSR FG mod [**dlssg-to-fsr3**](#nukems-dlssg-to-fsr3) has also been added - **_not bundled_**$`^3`$  
- For a detailed list of all features, check [Features](Features.md)


> [!IMPORTANT]
> _**Always check the [Wiki Compatibility list](https://github.com/cdozdil/OptiScaler/wiki) for known game issues and workarounds.**_  
> Also please check the  [***Optiscaler known issues***](#known-issues) at the end regarding **RTSS** compatibility.  
> A separate [***FSR4 Compatibility list***](https://github.com/cdozdil/OptiScaler/wiki/FSR4-Compatibility-List) is available for community-sourced tested games.  
> ***[3]** For **not bundled** items, please check [Installation](#installation).*  

> [!NOTE]
> <details>
>  <summary><b>Expand for [1], [2] </b></summary>  
>  
> ***[1]** Regarding **XeSS**, since Unreal Engine plugin does not provide depth, replacing in-game XeSS breaks other upscalers (e.g. Redout 2 as a XeSS-only game), but you can still apply RCAS sharpening to XeSS to reduce blurry visuals (in short, if it's a UE game, in-game XeSS only works with XeSS in OptiScaler overlay).*
>
> *Regarding **FSR inputs**, FSR 3.1 is the first version with a fully standardised, forward-looking API and should be fully supported. Since FSR2 and FSR3 support custom interfaces, game support will depend on the developers' implementation. With Unreal Engine games, you might need [ini tweaks](https://github.com/cdozdil/OptiScaler/wiki/Unreal-Engine-Tweaks) for FSR inputs.*  
>
> ***[2]** Regarding **FSR4**, support added with recent Nightly builds. Please check [FSR4 Compatibility list](https://github.com/cdozdil/OptiScaler/wiki/FSR4-Compatibility-List) for known supported games.*
> 
> </details>


## Official Discord Server: [DLSS2FSR](https://discord.gg/2JDHx6kcXB)

*This project is based on [PotatoOfDoom](https://github.com/PotatoOfDoom)'s excellent [CyberFSR2](https://github.com/PotatoOfDoom/CyberFSR2).*

## How it works?
OptiScaler implements the necessary API methods of DLSS2+ & NVAPI, XeSS and FSR2+ to act as a middleware. It intercepts upscaler calls from the game (_**Inputs**_) and redirects them to the chosen upscaling backend (_**Output**_), allowing games using one technology to use another  of your choice. **Inputs -> OptiScaler -> Outputs**
> [!NOTE]
> Pressing **`Insert`** should open the Optiscaler **Overlay** in-game with all of the options (`ShortcutKey=` can be changed in the config file). Pressing **`Page Up`** shows the performance stats overlay in the top left, and can be cycled between different modes with **`Page Down`**.


![image](https://github.com/user-attachments/assets/e138c979-c5d9-499f-a89b-165bb7cfcb32)


## Which APIs and Upscalers are Supported?
Currently **OptiScaler** can be used with DirectX 11, DirectX 12 and Vulkan, but each API has different sets of upscaler options.  
[**OptiFG**](#optifg-powered-by-fsr3-fg--hudfix-experimental-hud-ghosting-fix) currently **only supports DX12** and is explained in a separate paragraph.

#### For DirectX 12
- XeSS (Default)
- FSR2 2.1.2, 2.2.1
- FSR3 3.1 (and FSR2 2.3.2)
- DLSS
- FSR4 (Preliminary support, RDNA4 only)

#### For DirectX 11
- FSR2 2.2.1 (Default, native DX11)
- FSR3 3.1.2 (unofficial port to native DX11)
- XeSS, FSR2 2.1.2, 2.2.1, FSR3 3.1 w/Dx12 (_via D3D11on12_)$`^1`$
- DLSS (native DX11)
- XeSS 2.x (native DX11, _Intel ARC only_)

> [!NOTE]
> <details>
>  <summary><b>Expand for [1]</b></summary>
>
> _**[1]** These implementations use a background DirectX12 device to be able to use Dirext12-only upscalers. There is a 10-15% performance penalty for this method, but allows many more upscaler options. Also native DirectX11 implementation of FSR 2.2.1 is a backport from Unity renderer and has its own problems of which some were fixed by OptiScaler. These implementations **do not support Linux** and will result in a black screen._
> </details>

#### For Vulkan
- FSR2 2.1.2 (Default), 2.2.1
- FSR3 3.1 (and FSR2 2.3.2)
- DLSS
- XeSS 2.x

#### OptiFG (powered by FSR3 FG) + HUDfix (experimental HUD ghosting fix) 
**OptiFG** was added with **v0.7** and is **only supported in DX12**. 
It's an **experimental** way of adding FSR3 FG to games without native Frame Generation, or can also be used as a last case scenario if the native FG is not working properly.

For more information on OptiFG and how to use it, please check the Wiki page - [OptiFG](https://github.com/cdozdil/OptiScaler/wiki/OptiFG).


## Installation
> [!CAUTION]
> _**Warning**: **Do not use this mod with online games.** It may trigger anti-cheat software and cause bans!_

> [!IMPORTANT]
> ***Please use the [Nightly builds](https://github.com/cdozdil/OptiScaler/releases/tag/nightly) as the latest Stable is vastly outdated and the Readme does not apply to it anymore due to many missing features.***  
> _Since 0.7.7-Pre8, forced debugging is disabled. If you want to send a log file, set `LogLevel=0` and `LogToFile=true` and zip the log if it's too big._

---

### [Automated]
**1.** Extract **all** of the Optiscaler files **by the main game exe** _(for Unreal Engine games, that's usually the _win_shipping.exe_ in one of the subfolders, generally `<path-to-game>\Game-or-Project-name\Binaries\Win64 or WinGDK\`, **ignore** the `Engine` folder)_  
**2.** Try the `OptiScaler Setup.bat` script for automating the renaming process.  
_**3.** If the Bat file wasn't successful, please check the **Manual** steps._

---

### [Manual]
#### Nvidia

**`Step-by-step installation:`**  
**1.** Extract **all** Optiscaler files from the zip **by the main game exe** _(for Unreal Engine games, that's usually the _win_shipping.exe_ in one of the subfolders, generally `<path-to-game>\Game-or-Project-name\Binaries\Win64 or WinGDK\`, **ignore** the `Engine` folder)_.  
**2.** Rename OptiScaler's `OptiScaler.dll` (for old versions, it's `nvngx.dll`) to one of the [supported filenames](#optiscaler-supports-these-filenames) (preferred `dxgi.dll`, but depends on the game)$`^1`$  

> [!NOTE]
> _For FSR2/3-only games that don't have DLSS (e.g. The Callisto Protocol or The Outer Worlds: Spacer's Choice Edition), you have to provide the `nvngx_dlss.dll` in order to use DLSS in Optiscaler - download link e.g. [TechPowerUp](https://www.techpowerup.com/download/nvidia-dlss-dll/) or [Streamline SDK repo](https://github.com/NVIDIAGameWorks/Streamline/tree/main/bin/x64)_

#### AMD/Intel

**`Step-by-step installation:`**  
**1.** Extract **all** Optiscaler files from the zip **by the main game exe** _(for Unreal Engine games, that's usually the _win_shipping.exe_ in one of the subfolders, generally `<path-to-game>\Game-or-Project-name\Binaries\Win64 or WinGDK\`, **ignore** the `Engine` folder)_  
**2.** Rename OptiScaler's `OptiScaler.dll` (for old versions, it's `nvngx.dll`) to one of the [supported filenames](#optiscaler-supports-these-filenames) (preferred `dxgi.dll`, but depends on the game)$`^1`$  
**3a.** **Either** locate the `nvngx_dlss.dll` file (for UE games, generally in one of the subfolders under `Engine/Plugins`), create a copy, rename the copy to `nvngx.dll` and put it beside Optiscaler    
**3b.** **OR** download `nvngx_dlss.dll` from e.g. [TechPowerUp](https://www.techpowerup.com/download/nvidia-dlss-dll/) or [Streamline SDK repo](https://github.com/NVIDIAGameWorks/Streamline/tree/main/bin/x64) if you don't want to search, rename it to `nvngx.dll` and put it beside Optiscaler   

_Check the [screenshot](#example-of-correct-installation-with-additional-fakenvapi-and-nukem-mod) for proper installation_

---

#### [Nukem's dlssg-to-fsr3]

**1.** Download the mod's regular version - [**dlssg-to-fsr3 NexusMods**](https://www.nexusmods.com/site/mods/738) or [**dlssg-to-fsr3 Github**](https://github.com/Nukem9/dlssg-to-fsr3)     
**2.** Put the `dlssg_to_fsr3_amd_is_better.dll` in the same folder as Optiscaler (by the main game exe) and set `FGType=nukems` in `Optiscaler.ini`  
**3.** For **AMD/Intel GPUs**, **Fakenvapi** is also **required** when using **Nukem mod** in order to successfully expose DLSS FG in-game. 

---

#### [Fakenvapi]

**0.** **Do not use with Nvidia**, only required for AMD/Intel  
**1.** Download the mod - [**Fakenvapi**](https://github.com/FakeMichau/fakenvapi)  
**2.** Extract the files and transfer `nvapi64.dll` and `fakenvapi.ini` to the same folder as Optiscaler (by the main game exe)   

_**Anti-Lag 2** only supports RDNA cards and is Windows only atm (shortcut for cycling the overlay - `Alt+Shift+L`). For information on how to verify if Anti-Lag 2 is working, please check [Anti-Lag 2 SDK](https://github.com/GPUOpen-LibrariesAndSDKs/AntiLag2-SDK?tab=readme-ov-file#testing). **Latency Flex** is cross-vendor and cross-platform, can be used as an alternative if AL2 isn't working._ 

---

> [!TIP]
> *[1] Linux users should add renamed dll to overrides:*
> ```
> WINEDLLOVERRIDES=dxgi=n,b %COMMAND% 
> ```

> [!IMPORTANT]
> **Please don't rename the ini file, it should stay as `OptiScaler.ini`**.

> [!NOTE]
> ### OptiScaler supports these filenames:
> * dxgi.dll 
> * winmm.dll
> * dbghelp.dll (nightly only)  
> * d3d12.dll (nightly only)  
> * version.dll
> * wininet.dll
> * winhttp.dll
> * OptiScaler.asi (with an [ASI loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases))

> [!NOTE]
> ### _Example of correct AMD/Intel installation (with additional Fakenvapi and Nukem mod)_
> ![Installation](https://github.com/user-attachments/assets/977a2a68-d117-42ea-a928-78ec43eedd28)

---

> [!NOTE]
> If there is another mod (e.g. **Reshade** etc.) that uses the same filename (e.g. `dxgi.dll`), you can create a new folder called `plugins` and put other mod files in this folder. OptiScaler will check this folder and if it finds the same dll file (for example `dxgi.dll`), it will load this file instead of the original library.  
> Another option for **Reshade** - rename Reshade dll to `ReShade64.dll`, put it next to Optiscaler and set `LoadReshade=true` in OptiScaler.ini  
>
>![image](https://github.com/cdozdil/OptiScaler/assets/35529761/c4bf2a85-107b-49ac-b002-59d00fd06982)


### Legacy installation (deprecated, no FG and limited features, `nvngx.dll`)
<details>
  <summary><b>Legacy</b></summary>

`Step-by-step installation:`
1. Download the latest relase from [releases](https://github.com/cdozdil/OptiScaler/releases).
2. Extract the contents of the archive next to the game executable file in your games folder. (e.g. for Unreal Engine games, it's `<path-to-game>\Game-or-Project-name\Binaries\Win64 or WinGDK\`)$`^1`$
3. Rename `OptiScaler.dll` to `nvngx.dll` (For older builds, file name is already `nvngx.dll`, so skip this step)
4. Run `EnableSignatureOverride.reg` from `DlssOverrides` folder and confirm merge.$`^2`$$`^3`$

*[1] This package contains latest version of `libxess.dll` and if the game folder contains any older version of the same library, it will be overwritten. Consider backing up or renaming existing files.*

*[2] Normally Streamline and games check if nvngx.dll is signed, by merging this `.reg` file we are overriding this signature check.*

*[3] Adding signature override on Linux - There are many possible setups, this one will focus on Steam games:*
* *Make sure you have protontricks installed*
* *Run in a terminal protontricks <steam-appid> regedit, replace <steam-appid> with an id for your game*
* *Press "registry" in the top left of the new window -> `Import Registry File` -> navigate to and select `EnableSignatureOverride.reg`*
* *You should see a message saying that you successfully added the entries to the registry*

*If your game is not on Steam, it all boils down to opening regedit inside your game's prefix and importing the file.*
</details>

## Update OptiScaler version when using DLSS Enabler  
1. Delete/rename `dlss-enabler-upscaler.dll` in game folder
2. Extract `OptiScaler.dll` (for old versions, it's `nvngx.dll`) file from OptiScaler 7zip file to a temp folder
3. Rename `OptiScaler.dll` (for old versions, it's `nvngx.dll`) to `dlss-enabler-upscaler.dll`
4. Copy `dlss-enabler-upscaler.dll` from temp folder to the game folder

## Uninstallation
* Run `DisableSignatureOverride.reg` file 
* Delete `EnableSignatureOverride.reg`, `DisableSignatureOverride.reg`, `OptiScaler.dll` (for old versions, it's `nvngx.dll`), `OptiScaler.ini` files (if you used Fakenvapi and/or Nukem mod, then also delete `fakenvapi.ini`, `nvapi64.dll` and `dlssg_to_fsr3` files)
* If there was a `libxess.dll` file and you have backed it up, delete the new file and restore the backed up file. If you overwrote/replaced the old file, **DO NOT** delete `libxess.dll` file. If there was no `libxess.dll` before, it's safe to delete. Same goes for FSR files (`amd_fidelityfx`).

## Configuration
Please check [this](Config.md) document for configuration parameters and explanations. If your GPU is not an Nvidia one, check [GPU spoofing options](Spoofing.md) *(Will be updated)*

## Known Issues
If you can't open the in-game menu overlay:
1. Please check that you have enabled DLSS, XeSS or FSR from game options
2. If using legacy installation, please try opening menu while you are in-game (while 3D rendering is happening)
3. If you are using **RTSS** (MSI Afterburner, CapFrameX), please enable this setting in RTSS and/or try updating RTSS. **When using OptiFG please disable RTSS for best compatibility**
 
 ![image](https://github.com/cdozdil/OptiScaler/assets/35529761/8afb24ac-662a-40ae-a97c-837369e03fc7)

Please check [this](Issues.md) document for the rest of the known issues and possible solutions for them. Also check the community [Wiki](https://github.com/cdozdil/OptiScaler/wiki) for possible game issues and HUDfix incompatible games.

## Compilation

### Requirements
* Visual Studio 2022

### Instructions
* Clone this repo with **all of its submodules**.
* Open the OptiScaler.sln with Visual Studio 2022.
* Build the project

## Thanks
* @PotatoOfDoom for CyberFSR2
* @Artur for DLSS Enabler and helping me implement NVNGX api correctly
* @LukeFZ & @Nukem for their great mods and sharing their knowledge 
* @FakeMichau for continous support, testing and feature creep
* @QM for continous testing efforts and helping me to reach games
* @TheRazerMD for continous testing and support
* @Cryio, @krispy, @krisshietala, @Lordubuntu, @scz, @Veeqo for their hard work on [compatibility matrix](https://docs.google.com/spreadsheets/d/1qsvM0uRW-RgAYsOVprDWK2sjCqHnd_1teYAx00_TwUY)
* And the whole DLSS2FSR community for all their support

## Credit
This project uses [FreeType](https://gitlab.freedesktop.org/freetype/freetype) licensed under the [FTL](https://gitlab.freedesktop.org/freetype/freetype/-/blob/master/docs/FTL.TXT)

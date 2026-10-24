# Last Call at Larkspur Galleria

A first-person zombie survival shooter set in a two-storey shopping mall, built in Unreal Engine 5.8 (C++).
Everything in the mall — architecture, props, materials, signage, weapons, audio — is generated at runtime
by the `MallMadness` C++ module. The template mannequin skeletal meshes and animations drive the zombies.

## Moving to another computer

Copy these from the project folder: `MallMadness.uproject`, `Config/`, `Content/`, `Source/`, `README.md`,
`Play.command`, `Play.bat` (about 130 MB, almost all of it `Content/`). Skip `Binaries/`, `Intermediate/`,
`Saved/` and `DerivedDataCache/`; they are machine-specific and get regenerated.

Requirements: Unreal Engine 5.8, plus Xcode (Mac) or Visual Studio 2022 with the "Game development with C++"
workload (Windows). A machine with 16 GB+ RAM and a discrete or recent Apple GPU is recommended; the game uses
Lumen, virtual shadow maps and ~150 dynamic lights.

## Build

Mac:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" MallMadnessEditor Mac Development -Project="$PWD/MallMadness.uproject" -WaitMutex
```

Windows (or just double-click `Play.bat`, which builds on first run):

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" MallMadnessEditor Win64 Development -Project="%CD%\MallMadness.uproject" -WaitMutex
```

Opening `MallMadness.uproject` in the editor also offers to build the module.

## Play

Double-click `Play.command` (Mac) or `Play.bat` (Windows), or run:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/MallMadness.uproject" -game -windowed -ResX=1600 -ResY=900
```

Close the Unreal Editor first if memory is tight: two engine instances at once need a lot of RAM.

From the editor: open the project (rebuild the module if prompted), then use **Play > Standalone Game**. The
project's default game map is `/Engine/Maps/Entry` with `MallGameMode` as the global game mode, so the mall is
built on load. (The template level `Lvl_FirstPerson` is untouched and still uses its own game mode.)

The first launch compiles shaders for the runtime-generated materials; later launches reuse the cache.

## Controls

| Input | Action |
|---|---|
| W A S D | Move |
| Mouse | Look |
| Shift | Sprint |
| Space | Jump |
| C (toggle) / Ctrl (hold) | Crouch |
| Left mouse | Fire |
| Right mouse (hold) | Aim down sights / scope |
| R | Reload |
| E | Interact / loot (hold for the radio) |
| 1–4, mouse wheel | Select weapon |
| Q | Use medkit |
| Esc / P | Pause (releases the mouse) |

Mouse sensitivity and volume are adjustable on the title and pause screens.

## Objective

1. Find the replacement fuse in **Voltline Electronics** (Level 2, north-west, storage room).
2. Bring it to the **Maintenance** room (Level 2, east service hall) and hold E on the emergency radio.
3. Survive 75 seconds while the distress call transmits — zombie activity spikes.
4. Reach the **main entrance** (Level 1, west end) once the shutter opens.

Weapons: 9mm handgun (start), AR15 (Security office, L1), pump shotgun (Summit Outfitters hunting counter, L1),
bolt-action rifle (abandoned survivor nest on the L2 balcony outside the pharmacy).

## Layout

- Level 1: main entrance vestibule (west), Thread & Theory clothing (+ stockroom, fitting rooms), Security office,
  Summit Outfitters sporting goods (+ stock area), food court with five counters and a burger kitchen (east),
  restroom and loading-dock service corridors, closed shops.
- Level 2: Voltline electronics (+ storage), CareWell Pharmacy (+ dispensary), Marquee 6 Cinemas lobby and a raked
  auditorium, maintenance hall and room with the emergency radio, closed shops.
- Central skylit atrium with a dry fountain, hanging sign, kiosk and directories; a broad stair (west) and a pair
  of stopped escalators (east) form a loop between floors.

## Automated checks

```bash
# Gameplay test: traversal, pickups, all four weapons, reload rules, melee, cross-floor AI, objectives, win, restart, death.
UnrealEditor "$PWD/MallMadness.uproject" -game -windowed -ResX=1280 -ResY=720 -MallTest
# results: Saved/MallTest_0.txt (first run) and Saved/MallTest_2.txt (after restart), screenshots in Saved/AutoShots

# Screenshot tour: -MallShots="x,y,z,pitch,yaw;..." (metres / degrees)
```

## Status (end of first session)

Verified on the development Mac (8 GB RAM, editor open at the same time, so frame rates were not representative):
- The module compiles; the mall builds at runtime (704 nav nodes, 40 interactables, 17 starting zombies).
- A screenshot tour confirmed every location renders: atrium, stairs, escalators, food court, clothing,
  sporting goods, security, cinema lobby and auditorium, pharmacy, electronics, maintenance.
- The automated test walked the player to the stair foot and up to level 2 (z = 690 cm as expected) before it
  was stopped for memory thrashing.

Not yet verified (run `-MallTest` on the new machine; results land in `Saved/MallTest_0.txt` and `_2.txt`):
- The rest of the scripted test: the escalator route, pickups, all four weapons, the reload-cancel rule, melee,
  cross-floor zombie pathing, the objective chain, victory, restart and death.
- Fixes made after the last successful run: HUD text drawn with engine fonts, raised first-person weapon
  placement, zombie clothing material, dimmer emergency lights, nav pruning of isolated waypoints.
- Hands-on feel: mouse look, recoil, audio levels and performance.
- Windows build (the code is standard UE C++ but has only been compiled with Apple clang).

## Code map (`Source/MallMadness`)

| File | Responsibility |
|---|---|
| `MallGameMode` | Game flow (title/pause/death/victory/restart), objectives, zombie director, interactions, test harness |
| `MallPlayer` | First-person movement, weapons, recoil, ADS, reload, healing, interaction focus |
| `MallWeapons` | Weapon tuning table and procedural first-person weapon models |
| `MallZombie` | Zombie variants, perception (sight + hearing), waypoint pathing, attacks, reactions, death |
| `MallNav` | Waypoint graph with automatic sweep-tested links and A* |
| `MallBuilder` / `MallLayout` / `MallProps` | Mall construction, stores, props, lighting, loot placement, nav regions |
| `MallInteractable` | Pickups, containers, weapons, fuse, radio, exit |
| `MallMaterials` | Procedural textures + master materials built at runtime |
| `MallAudio` | Synthesised sound effects and ambience |
| `MallFX` | Pooled impacts, debris and shell casings |
| `MallHUD` / `MallPlayerController` | HUD, menus, input routing, pause/focus handling |
